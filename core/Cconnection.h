#pragma once
#include "../misc/NBSocket.h"
#include "../agent/idata.h"
#include <string>
#include "../zstd/lib/zstd.h"
#include "../zstd/lib/zstd_errors.h"

bool issyserror(int err);
bool isneterror(int err);

class IConnectionCallback {
public:
	virtual void OnConnLog(LPCWSTR lpszText) {}
};


class Cconnection : public InetAborter
{	
	CInetTcp *m_conn;
	BOOL m_FlagExit;
	CInetAbortorBase _ab;
	std::string m_ip;
	int m_port;
	std::string m_passwd;
	DWORD m_retry_count;
	IConnectionCallback* m_pCB;
	std::string m_zbuf;
public:
	BOOL* _pflag;
	std::wstring last_errmsg;
	DWORD last_errcode;

	Cconnection();
	~Cconnection();

	virtual bool aborted() {
		if (*_pflag)
			return true;
		return false;
	}

	void SetCallback(IConnectionCallback* pCB);
	void Close();
	bool Retry();
	void setTarget(LPCTSTR lpszIP, int port, LPCTSTR lpszPasswd);
	bool Connect();
	void Abort();
	int  GetPCInfo(std::string& result);
	int  GetDiskInfo(std::string& result);
	int  Quit();
	int  OpenDisk(LPCTSTR lpszName);
	int  SeekDisk(uint64_t pos);
	int  ReadDisk(uint64_t size);
	int  ReadDiskStart(uint64_t pos, uint64_t size);
	int  ReadDiskData(void *buf, uint32_t size, DWORD *pBytesRead);
	int  CloseDisk();
	int  OpenVolume(LPCTSTR lpszName, DWORD dwOptions, uint64_t pos, uint64_t size, uint64_t *pCalcDataSize);
	int  ReadVolume(uint64_t size);
	int  ReadVolumeData(void* buf, uint32_t size, uint64_t* pBytesRead, BOOL *pIsFreeSpace);
	int  CloseVolume();

protected:
	int RequestJ(uint8_t cmd, const void *req, int reqlen, std::string &resultJ, int limitresultsize=0);
	bool parsejson_errmsg(std::string& result);
	void Log(LPCTSTR lpszText);
	void LogFmt(LPCTSTR fmt, ...);
};

