#pragma once

#include "Thread.h"

BOOL
APIENTRY
MyCreatePipeEx(
	OUT LPHANDLE lpReadPipe,
	OUT LPHANDLE lpWritePipe,
	IN LPSECURITY_ATTRIBUTES lpPipeAttributes,
	IN DWORD nSize,
	DWORD dwReadMode,
	DWORD dwWriteMode
);

class SubProcess
	: public CThread
{
	HANDLE _hChildStd_IN_Rd;
	HANDLE _hChildStd_IN_Wr;
	HANDLE _hChildStd_OUT_Rd;
	HANDLE _hChildStd_OUT_Wr;

	PROCESS_INFORMATION mPI;
	CMyCriticalSection mCS;
public:

	SubProcess();
	~SubProcess();

	bool CreateChild(const wchar_t* cmd, bool bHandleStderr);
	void Close();
	int Write(const void *data, unsigned int len);
	bool WriteAll(const void *data, unsigned int len);
	int Read(void *buf, unsigned int *size, unsigned long timeout);

	bool EnablePushMode();
	bool WaitForProcessExitCode(DWORD timeout, DWORD* pEC);
	int  WaitForProcessExitCode2(DWORD timeout, DWORD* pEC);
	DWORD GetPid();

protected:
	bool InitPipes();

	virtual DWORD run();
	virtual void OnReadOutput(void *data, unsigned int len) {}
};
