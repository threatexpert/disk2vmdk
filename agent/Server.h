#pragma once

#include "../misc/netutils.h"
#include "../misc/NBSocket.h"
#include "../core/Thread.h"
#include <list>
#include <string>
#include "idata.h"
#include "../app/GetPCInfo.h"
#include "../core/Disks.h"
#include "../core/ImageMaker.h"
#include "../core/VolumeReader.h"
#include "../core/Buffer.h"

class IRequestLogger {
public:
	virtual void req_log_output(int cmd, int err, LPCWSTR lpszDetails) {}
};

extern BOOL ENABLE_ZSTD;

class CRequestHandler
	: public CThread
{
	class CServer* m_pMgr;
	BOOL m_FlagExit;
	CInetTcp* m_ptcp;
	CInetAbortorBase _ab;
	HANDLE m_hOpenedDisk;
	std::string m_openedDiskName;
	uint64_t m_disk_pos;
	uint64_t m_vol_pos;
	CVolumeReader* m_pVR;
	IRequestLogger* m_pLog;
	CStringW m_err;
	const char* _passwd_ref;
	std::string m_zbuf;
	enum {
		buf_reserved_head_space = 512
	};
	CBuffers m_buffers_reader;
	CBuffers m_buffers_data2write;
	HANDLE m_hThreadWriter;
	int m_writer_error;

public:
	const int REQUEST_TIMEOUT_SEC = 120;
	BOOL m_bEnd;
	int m_id;

	CRequestHandler();
	~CRequestHandler();

	BOOL Init(class CServer* pMgr, SOCKET s, const char* passwd_ref);
	void Close();

protected:
	virtual DWORD run();
	static DWORD WINAPI sThreadWriter(void* param) {
		CRequestHandler* _this = (CRequestHandler*)param;
		return _this->ThreadWriter();
	}
	DWORD ThreadWriter();
	void StopThreadWriter();

	void log(int cmd, int err, LPCWSTR lpszDetails);

	bool reply(BYTE cmd, BYTE err, BYTE datatype, const void* data, uint64_t datalen);
	bool reply_diskdata(BYTE cmd, BYTE err, BYTE datatype, uint64_t datalen, CBuffers::Buffer* &data);
	bool cmd_get_pc_info(int reqlen);
	bool cmd_get_disk_info(int reqlen);
	bool cmd_open_disk(int reqlen);
	bool cmd_seek_disk(int reqlen);
	bool cmd_read_disk(int reqlen);
	bool cmd_close_disk(int reqlen);
	bool cmd_open_volume(int reqlen);
	bool cmd_read_volume(int reqlen);
	bool cmd_close_volume(int reqlen);
};

class CServer
	: public CThread
	, public IRequestLogger
{
	SOCKET m_listen;
	BOOL m_FlagExit;
	HANDLE m_hExitEvent;
	HANDLE m_hCleanEvent;
	std::list<CRequestHandler*> m_reqs;
	BOOL m_single_session_Only;
	char m_passwd[128];
public:
	DWORD m_id;
	CServer();
	~CServer();

	BOOL Init(const char* bindIP, int port, const char* passwd);
	void Close();

	void OnRequestEnd(CRequestHandler* req);

	virtual bool OnNewClientConnected(const char* ip, int port) { return true; };
	virtual void req_log_output(int cmd, int err, LPCWSTR lpszDetails);

protected:
	void CleanRequests(BOOL all);
	virtual DWORD run();
};

