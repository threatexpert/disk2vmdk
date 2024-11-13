#pragma once

#include "Thread.h"
#include <string>
#include "RangeMgr.h"
#include "SubProcess.h"
#include <list>
#include "VolumeReader.h"
#include "VBoxSimple.h"
#include "Cconnection.h"
#include "Buffer.h"

class IImageMakerCallback
{
public:
	virtual void ImageMaker_BeforeStart(LPCWSTR lpszStatus) {};
	virtual void ImageMaker_OnStart() = 0;
	virtual void ImageMaker_OnCopied(uint64_t position, uint64_t disksize, uint64_t data_copied, uint64_t data_space_size) = 0;
	virtual void ImageMaker_OnEnd(int err_code) = 0;
	virtual void ImageMaker_OnRemoteConnLog(LPCWSTR lpszStatus) = 0;
};


class CProcVBoxM : public SubProcess
{
	std::string _output;
public:

	std::string get_output() {
		CThread::Wait();
		return _output;
	}

	size_t get_outputsize() {
		return _output.size();
	}

	virtual void OnReadOutput(void* data, unsigned int len) {
		_output.append((char*)data, len);
	}

};

class CDiskReader
{
	HANDLE m_hSource;
	BOOL m_bRemoteMode;
	std::wstring m_remote_diskname;
	uint64_t m_needread;
	uint64_t m_setpos;
	Cconnection m_conn;
	BOOL m_isReady;
public:
	CDiskReader();
	virtual ~CDiskReader();

	BOOL OpenLocal(LPCWSTR lpszName);
	BOOL OpenRemote(LPCWSTR lpszIP, int port, LPCWSTR lpszPasswd, LPCWSTR lpszName);
	BOOL NeedToRead(uint64_t size);
	BOOL Read(LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, uint64_t pos);
	BOOL SetPos(uint64_t pos);
	void Abort();
	void Shutdown();
	void Close();
	void SetConnectionCallback(IConnectionCallback* pConnCB);
protected:

};

class CImageMaker
	: public CThread
	, public IConnectionCallback
{
public:
	enum {
		OptStandard	= 1,
		OptFixed	= 2,
		OptSplit2G	= 4
	};

	enum {
		Mode_Unk = 0,
		Mode_DD,
		Mode_VD_Piped,
		Mode_VD_Lib
	};

protected:
	std::wstring m_strVBoxManage;
	std::wstring m_strSource;
	uint64_t m_nSourceSize;
	uint64_t m_nSourceDataSpaceSize;
	std::wstring m_strDest;
	HANDLE m_hDest;
	uint64_t m_nDestTotalWritten;
	uint64_t m_nDestDataWritten;
	uint64_t m_nVDCapacity;
	DWORD m_dwOptions;
	BOOL m_bVBoxmUsed;
	CProcVBoxM m_vboxm_proc;
	BOOL m_vboxm_proc_Terminated;
	DWORD m_convertor_exit_code;
	IImageMakerCallback* m_pCB;

	BOOL m_vbox_libmode;
	CVBoxSimple m_vb_writer;

	CBuffers m_buffers_reader;
	DWORD m_dwBufferSize;
	CBuffers m_buffers_data2write;
	CDiskReader m_diskReader;
	uint64_t m_nSPos;
	CRangeMgr m_ranges;
	std::wstring m_lasterr;
	std::wstring m_format;
	int m_exit_flag;
	std::wstring m_writer_err;
	int m_WorkingMode;

public:
	CImageMaker();
	virtual ~CImageMaker();

	BOOL setVBoxManage(LPCWSTR lpszApp);
	BOOL setOptions(DWORD dwOptions);
	BOOL setSource(LPCWSTR lpszSrc, uint64_t nSrcSize);
	BOOL setSource(int diskno, uint64_t nSrcSize);
	BOOL setRemoteSource(LPCWSTR lpszIP, int port, LPCWSTR lpszPasswd, int diskno, uint64_t nSrcSize);
	BOOL setDest(LPCWSTR lpszDst, bool FormatBySuffix);
	BOOL setBufferSize(DWORD nSize);
	BOOL addExcludeRange(uint64_t start, uint64_t end, int unit_size);
	BOOL addRangeHandler(uint64_t start, uint64_t end, int unit_size, CRangeMgr::RangeHandler *pHandler);
	BOOL setCallback(IImageMakerCallback *pCB);
	BOOL setFormat(LPCWSTR lpszFmt);
	std::wstring getFormat();

	BOOL start();
	int wait(DWORD dwMS);
	BOOL stop();
	std::wstring lasterr() { return m_lasterr; };
	DWORD getConvertorExitCode() { return m_convertor_exit_code; }
	BOOL setDestReadOnly();
	void EnableVBoxLibMode();
	int GetWorkingMode() { return m_WorkingMode; }
protected:
	virtual DWORD run();
	static DWORD WINAPI s_ThreadWriter(void* p);
	DWORD ThreadWriter();
	void OnConnLog(LPCWSTR lpszText);
};

class CVolumeRangeHandler
	: public CVolumeReader
	, public CRangeMgr::RangeHandler
{
	long m_ref;

public:

	CVolumeRangeHandler();
	virtual ~CVolumeRangeHandler();
	virtual long AddRef();
	virtual long Release();
	virtual BOOL GetSize(uint64_t* pSize);
	virtual BOOL CalcDataSize(uint64_t* pSize);
	virtual BOOL Read(void* lpBuffer, DWORD nNumberOfBytesToRead, uint64_t* lpNumberOfBytesRead, BOOL* pIsFreeSpace);
	virtual void Close();
	virtual void SetExitFlag(BOOL* p);
};


class CRemoteVolumeRangeHandler
	: public CRangeMgr::RangeHandler
{
	long m_ref;
	uint64_t m_Size;
	uint64_t m_SizeUsed;
	uint64_t m_pos;
	DWORD m_dwOptions;
	std::wstring m_remote_volname;
	Cconnection m_conn;
	BOOL m_isReady;

public:
	std::wstring m_lasterr;

	CRemoteVolumeRangeHandler();
	virtual ~CRemoteVolumeRangeHandler();

	void SetConnectionCallback(IConnectionCallback* pConnCB);
	BOOL setOptions(DWORD dwOptions);
	DWORD getOptions();
	BOOL OpenRemote(LPCWSTR lpszIP, int port, LPCWSTR lpszPasswd, LPCWSTR lpszSrc, uint64_t nSrcSize);
	std::wstring lasterr();

	virtual long AddRef();
	virtual long Release();
	virtual BOOL GetSize(uint64_t* pSize);
	virtual BOOL CalcDataSize(uint64_t* pSize);
	virtual BOOL Read(void* lpBuffer, DWORD nNumberOfBytesToRead, uint64_t* lpNumberOfBytesRead, BOOL* pIsFreeSpace);
	virtual void Close();
	virtual void SetExitFlag(BOOL* p);
};
