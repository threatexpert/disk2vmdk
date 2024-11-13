#include "pch.h"
#include <windows.h>
#include <stdio.h>
#include "SubProcess.h"

static ULONG PipeSerialNumber = 0;


BOOL
APIENTRY
MyCreatePipeEx(
	OUT LPHANDLE lpReadPipe,
	OUT LPHANDLE lpWritePipe,
	IN LPSECURITY_ATTRIBUTES lpPipeAttributes,
	IN DWORD nSize,
	DWORD dwReadMode,
	DWORD dwWriteMode
)

/*++
Routine Description:
	The CreatePipeEx API is used to create an anonymous pipe I/O device.
	Unlike CreatePipe FILE_FLAG_OVERLAPPED may be specified for one or
	both handles.
	Two handles to the device are created.  One handle is opened for
	reading and the other is opened for writing.  These handles may be
	used in subsequent calls to ReadFile and WriteFile to transmit data
	through the pipe.
Arguments:
	lpReadPipe - Returns a handle to the read side of the pipe.  Data
		may be read from the pipe by specifying this handle value in a
		subsequent call to ReadFile.
	lpWritePipe - Returns a handle to the write side of the pipe.  Data
		may be written to the pipe by specifying this handle value in a
		subsequent call to WriteFile.
	lpPipeAttributes - An optional parameter that may be used to specify
		the attributes of the new pipe.  If the parameter is not
		specified, then the pipe is created without a security
		descriptor, and the resulting handles are not inherited on
		process creation.  Otherwise, the optional security attributes
		are used on the pipe, and the inherit handles flag effects both
		pipe handles.
	nSize - Supplies the requested buffer size for the pipe.  This is
		only a suggestion and is used by the operating system to
		calculate an appropriate buffering mechanism.  A value of zero
		indicates that the system is to choose the default buffering
		scheme.
Return Value:
	TRUE - The operation was successful.
	FALSE/NULL - The operation failed. Extended error status is available
		using GetLastError.
--*/

{
	HANDLE ReadPipeHandle, WritePipeHandle;
	DWORD dwError;
	CHAR PipeNameBuffer[MAX_PATH];

	//
	// Only one valid OpenMode flag - FILE_FLAG_OVERLAPPED
	//

	if ((dwReadMode | dwWriteMode) & (~FILE_FLAG_OVERLAPPED)) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	//
	//  Set the default timeout to 120 seconds
	//

	if (nSize == 0) {
		nSize = 4096;
	}

	sprintf_s(PipeNameBuffer, MAX_PATH,
		"\\\\.\\Pipe\\RemoteExeAnon.%08x.%08x",
		GetCurrentProcessId(),
		InterlockedIncrement(&PipeSerialNumber)
	);

	ReadPipeHandle = CreateNamedPipeA(
		PipeNameBuffer,
		PIPE_ACCESS_INBOUND | dwReadMode,
		PIPE_TYPE_BYTE | PIPE_WAIT,
		1,             // Number of pipes
		nSize,         // Out buffer size
		nSize,         // In buffer size
		120 * 1000,    // Timeout in ms
		lpPipeAttributes
	);

	if (!ReadPipeHandle) {
		return FALSE;
	}

	WritePipeHandle = CreateFileA(
		PipeNameBuffer,
		GENERIC_WRITE,
		0,                         // No sharing
		lpPipeAttributes,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | dwWriteMode,
		NULL                       // Template file
	);

	if (INVALID_HANDLE_VALUE == WritePipeHandle) {
		dwError = GetLastError();
		CloseHandle(ReadPipeHandle);
		SetLastError(dwError);
		return FALSE;
	}

	*lpReadPipe = ReadPipeHandle;
	*lpWritePipe = WritePipeHandle;
	return(TRUE);
}

SubProcess::SubProcess()
	: _hChildStd_IN_Rd(NULL)
	, _hChildStd_IN_Wr(NULL)
	, _hChildStd_OUT_Rd(NULL)
	, _hChildStd_OUT_Wr(NULL)
{
	memset(&mPI, 0, sizeof(mPI));
}


SubProcess::~SubProcess()
{
	Close();
	CThread::Wait();
}

bool SubProcess::CreateChild(const wchar_t * cmd, bool bHandleStderr)
{
	STARTUPINFO siStartInfo;
	bool bSuccess = false;
	wchar_t *cmdline = NULL;

	if (!InitPipes())
		return false;


	do {

		cmdline = _wcsdup(cmd);
		// Set up members of the STARTUPINFO structure. 
		// This structure specifies the STDIN and STDOUT handles for redirection.

		ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
		siStartInfo.cb = sizeof(STARTUPINFO);
		siStartInfo.hStdError = bHandleStderr ? _hChildStd_OUT_Wr : GetStdHandle(STD_ERROR_HANDLE);
		siStartInfo.hStdOutput = _hChildStd_OUT_Wr;
		siStartInfo.hStdInput = _hChildStd_IN_Rd;
		siStartInfo.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
		siStartInfo.wShowWindow = SW_HIDE;

		// Create the child process. 

		if (!CreateProcessW(NULL,
			cmdline,     // command line 
			NULL,          // process security attributes 
			NULL,          // primary thread security attributes 
			TRUE,          // handles are inherited 
			0,             // creation flags 
			NULL,          // use parent's environment 
			NULL,          // use parent's current directory 
			&siStartInfo,  // STARTUPINFO pointer 
			&mPI))  // receives PROCESS_INFORMATION 
		{
			break;
		}

		//已经继承到子进程的handle引用计数会增1，在父进程关闭的意义是把引用计数减1，这样等子进程关闭它时管道才会真正关闭
		//否则如果子进程退出了而管道仍没关闭，这个handle继续读写仍然不会异常
		CloseHandle(_hChildStd_OUT_Wr);
		_hChildStd_OUT_Wr = NULL;
		CloseHandle(_hChildStd_IN_Rd);
		_hChildStd_IN_Rd = NULL;

		bSuccess = true;
	} while (0);

	free(cmdline);
	return bSuccess;
}

void SubProcess::Close()
{
	CAutoCriticalSection lc(mCS);

	if (mPI.hProcess) {
		if (WaitForSingleObject(mPI.hProcess, 0) != WAIT_OBJECT_0)
			TerminateProcess(mPI.hProcess, -1);
		CloseHandle(mPI.hProcess);
		mPI.hProcess = NULL;
	}

	if (mPI.hThread) {
		CloseHandle(mPI.hThread);
		mPI.hThread = NULL;
	}

	if (_hChildStd_IN_Rd) {
		CloseHandle(_hChildStd_IN_Rd);
		_hChildStd_IN_Rd = NULL;
	}
	if (_hChildStd_IN_Wr) {
		CloseHandle(_hChildStd_IN_Wr);
		_hChildStd_IN_Wr = NULL;
	}
	if (_hChildStd_OUT_Rd) {
		CloseHandle(_hChildStd_OUT_Rd);
		_hChildStd_OUT_Rd = NULL;
	}
	if (_hChildStd_OUT_Wr) {
		CloseHandle(_hChildStd_OUT_Wr);
		_hChildStd_OUT_Wr = NULL;
	}
}

int SubProcess::Write(const void * data, unsigned int len)
{
	DWORD cbW = 0;

	if (!_hChildStd_IN_Wr)
		return -1;

	if (!WriteFile(_hChildStd_IN_Wr, data, len, &cbW, NULL))
		return -1;

	return (int)cbW;
}

bool SubProcess::WriteAll(const void * data, unsigned int len)
{
	unsigned int n = 0;
	int ret;
	while (len > 0)
	{
		ret = Write((char*)data + n, len);
		if (ret <= 0)
			return false;

		n += ret;
		len -= ret;
	}
	return true;
}

int SubProcess::Read(void * buf, unsigned int *size, unsigned long timeout)
{
	OVERLAPPED ol;
	DWORD cbR = 0;
	DWORD dwWait;
	int ret = 0;

	memset(&ol, 0, sizeof(ol));

	mCS.lock();

	if (!_hChildStd_OUT_Rd) {
		mCS.unlock();
		return -1;
	}

	ol.hEvent = CreateEvent(0, 1, 0, NULL);
	if (!ol.hEvent)
	{
		mCS.unlock();
		return -1;
	}

	if (::ReadFile(_hChildStd_OUT_Rd, buf, *size, &cbR, &ol))
	{
		mCS.unlock();
		CloseHandle(ol.hEvent);
		*size = cbR;
		return 1;
	}

	*size = 0;

	if (GetLastError() != ERROR_IO_PENDING)
	{
		mCS.unlock();
		CloseHandle(ol.hEvent);
		return -1;
	}

	mCS.unlock();

	dwWait = WaitForSingleObject(ol.hEvent, timeout);

	mCS.lock();

	if (_hChildStd_OUT_Rd) {
		if (!GetOverlappedResult(_hChildStd_OUT_Rd, &ol, &cbR, FALSE))
		{
			if (!CancelIo(_hChildStd_OUT_Rd))
				ret = -1;
		}
		else
		{
			*size = cbR;
			ret = 1;
		}
	}
	else {
		ret = -1;
	}

	mCS.unlock();

	CloseHandle(ol.hEvent);
	return ret;
}

bool SubProcess::EnablePushMode()
{
	if (mPI.hProcess == NULL)
		return false;

	return !!CThread::Start();
}

bool SubProcess::WaitForProcessExitCode(DWORD timeout, DWORD *pEC)
{
	bool ret = false;
	if (mPI.hProcess) {
		if (WaitForSingleObject(mPI.hProcess, timeout) == WAIT_OBJECT_0)
		{
			if (GetExitCodeProcess(mPI.hProcess, pEC))
				ret = true;
		}
	}
	return ret;
}

int SubProcess::WaitForProcessExitCode2(DWORD timeout, DWORD* pEC)
{
	int ret;
	if (mPI.hProcess) {
		switch (WaitForSingleObject(mPI.hProcess, timeout))
		{
		case WAIT_OBJECT_0:
			if (!GetExitCodeProcess(mPI.hProcess, pEC))
				ret = 1;
			else
				ret = 2;
			break;
		case WAIT_TIMEOUT:
			ret = 0;
			break;
		default:
			ret = -1;
			break;
		}
	}
	else {
		ret = -2;
	}
	return ret;
}

DWORD SubProcess::GetPid()
{
	return mPI.dwProcessId;
}

bool SubProcess::InitPipes()
{
	SECURITY_ATTRIBUTES saAttr;

	// Set the bInheritHandle flag so pipe handles are inherited. 

	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	// Create a pipe for the child process's STDOUT. 

	if (!MyCreatePipeEx(&_hChildStd_OUT_Rd, &_hChildStd_OUT_Wr, &saAttr, 0, FILE_FLAG_OVERLAPPED, FILE_FLAG_OVERLAPPED))
		return false;

	// Ensure the read handle to the pipe for STDOUT is not inherited.

	if (!SetHandleInformation(_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0))
		return false;

	// Create a pipe for the child process's STDIN. 

	if (!MyCreatePipeEx(&_hChildStd_IN_Rd, &_hChildStd_IN_Wr, &saAttr, 0, FILE_FLAG_OVERLAPPED, FILE_FLAG_OVERLAPPED))
		return false;

	// Ensure the write handle to the pipe for STDIN is not inherited. 

	if (!SetHandleInformation(_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0))
		return false;

	return true;
}

DWORD SubProcess::run()
{
	enum {bufsize = 1024*64+2};
	char *buf = (char*)malloc(bufsize);
	unsigned int sz;
	int ret;

	while (mPI.hProcess != NULL)
	{
		sz = bufsize - 2;
		ret = Read(buf, &sz, 1000);
		if (ret == 0)
		{
			continue;
		}
		else if (ret < 0)
		{
			if (mPI.hProcess)
				OnReadOutput(NULL, 0);
			break;
		}
		else
		{
			buf[sz] = buf[sz + 1] = 0;
			OnReadOutput(buf, sz);
			if (sz == 0)
				break;
		}

	}

	free(buf);
	return 0;
}
