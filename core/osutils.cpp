#include "pch.h"
#include "osutils.h"
#include <memory>

#pragma comment(lib, "version.lib" )


std::wstring GetErrorAsString(DWORD errorMessageID)
{
	LPWSTR messageBuffer = nullptr;

	//Ask Win32 to give us the string version of that message ID.
	//The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
	size_t size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&messageBuffer, 0, NULL);

	while (size > 0) {
		if (messageBuffer[size - 1] == '\r' || messageBuffer[size - 1] == '\n' || messageBuffer[size - 1] == ' ') {
			messageBuffer[size - 1] = 0;
		}
		else
			break;
		--size;
	}
	//Copy the error message into a std::string.
	std::wstring message(messageBuffer, size);

	//Free the Win32's string's buffer.
	LocalFree(messageBuffer);

	return message;
}


std::wstring GetLastErrorAsString()
{
    DWORD errorMessageID = ::GetLastError();
	if (errorMessageID == 0) {
		return L"";
	}
	return GetErrorAsString(errorMessageID);
}

std::wstring fromHResult(HRESULT h)
{
    wchar_t text[64] = L"0x";
    _ltow(h, text+2, 16);
    return text;
}

BOOL IsWow64()
{
	BOOL bIsWow64 = FALSE;

	//IsWow64Process is not available on all supported versions of Windows.
	//Use GetModuleHandle to get a handle to the DLL that contains the function
	//and GetProcAddress to get a pointer to the function if available.

	typedef BOOL(WINAPI* LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
	static LPFN_ISWOW64PROCESS fnIsWow64Process = NULL;
	if (fnIsWow64Process == NULL)
		fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process");

	if (NULL != fnIsWow64Process)
	{
		if (!fnIsWow64Process(GetCurrentProcess(), &bIsWow64))
		{
			//handle error
		}
	}
	return bIsWow64;
}

#ifndef _versionhelpers_H_INCLUDED_

BOOL IsWindowsVersionOrGreater(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor)
{
	OSVERSIONINFOEXW osvi = {};
	osvi.dwOSVersionInfoSize = sizeof(osvi);
	DWORDLONG        const dwlConditionMask = VerSetConditionMask(
		VerSetConditionMask(
			VerSetConditionMask(
				0, VER_MAJORVERSION, VER_GREATER_EQUAL),
			VER_MINORVERSION, VER_GREATER_EQUAL),
		VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);

	osvi.dwMajorVersion = wMajorVersion;
	osvi.dwMinorVersion = wMinorVersion;
	osvi.wServicePackMajor = wServicePackMajor;

	return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR, dwlConditionMask) != FALSE;
}


BOOL IsWindows7OrGreater()
{
    return IsWindowsVersionOrGreater(HIBYTE(_WIN32_WINNT_WIN7), LOBYTE(_WIN32_WINNT_WIN7), 0);
}

BOOL IsWindowsVistaOrGreater()
{
	return IsWindowsVersionOrGreater(HIBYTE(_WIN32_WINNT_VISTA), LOBYTE(_WIN32_WINNT_VISTA), 0);
}

BOOL IsWindowsXPOrGreater()
{
    return IsWindowsVersionOrGreater(HIBYTE(_WIN32_WINNT_WINXP), LOBYTE(_WIN32_WINNT_WINXP), 0);
}

BOOL IsWindowsXPSP2OrGreater()
{
    return IsWindowsVersionOrGreater(HIBYTE(_WIN32_WINNT_WINXP), LOBYTE(_WIN32_WINNT_WINXP), 2);
}

BOOL IsWindows2003OrGreater()
{
    return IsWindowsVersionOrGreater(HIBYTE(_WIN32_WINNT_WS03), LOBYTE(_WIN32_WINNT_WS03), 0);
}

BOOL IsWindowsServer()
{
    OSVERSIONINFOEXW osvi = { sizeof(osvi), 0, 0, 0, 0, {0}, 0, 0, 0, VER_NT_WORKSTATION };
    DWORDLONG        const dwlConditionMask = VerSetConditionMask(0, VER_PRODUCT_TYPE, VER_EQUAL);

    return !VerifyVersionInfoW(&osvi, VER_PRODUCT_TYPE, dwlConditionMask);
}

#endif //_versionhelpers_H_INCLUDED_

BOOL IsElevated() {

    if (!IsWindowsVistaOrGreater()) {
        return TRUE;
    }

    BOOL fRet = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION Elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &cbSize)) {
            fRet = !!Elevation.TokenIsElevated;
        }
    }
    if (hToken) {
        CloseHandle(hToken);
    }
    return fRet;
}

BOOL Is64BitWindows()
{
#if defined(_WIN64)
	return TRUE;  // 64-bit programs run only on Win64
#elif defined(_WIN32)
	return IsWow64();
#else
	return FALSE; // Win64 does not support Win16
#endif
}


LPCTSTR GetArch()
{
	typedef void WINAPI __GetNativeSystemInfo(
		LPSYSTEM_INFO lpSystemInfo
	);
	__GetNativeSystemInfo* pGetNativeSystemInfo = NULL;

	SYSTEM_INFO si;
	memset(&si, 0, sizeof(si));
	if ((unsigned int)IsWow64())
	{
		if (!pGetNativeSystemInfo) {
			*(void**)&pGetNativeSystemInfo = GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetNativeSystemInfo");
		}
		if (pGetNativeSystemInfo) {
			pGetNativeSystemInfo(&si);
		}
	}
	else
	{
		GetSystemInfo(&si);
	}

	switch (si.wProcessorArchitecture)
	{
	case PROCESSOR_ARCHITECTURE_AMD64:
		return _T("x64");
	case PROCESSOR_ARCHITECTURE_ARM:
		return _T("arm");
	case 12: //PROCESSOR_ARCHITECTURE_ARM64:
		return _T("arm64");
	case PROCESSOR_ARCHITECTURE_IA64:
		return _T("ia64");
	case PROCESSOR_ARCHITECTURE_INTEL:
		return _T("x86");
	default:
		return _T("Unknown");
	}
}

LPCTSTR GetAppArch()
{
	SYSTEM_INFO si;
	memset(&si, 0, sizeof(si));
	GetSystemInfo(&si);

	switch (si.wProcessorArchitecture)
	{
	case PROCESSOR_ARCHITECTURE_INTEL:
		return _T("x86");
	case PROCESSOR_ARCHITECTURE_AMD64:
		return _T("x64");
	case PROCESSOR_ARCHITECTURE_IA64:
		return _T("ia64");
	case PROCESSOR_ARCHITECTURE_ARM:
		return _T("arm");
	case 12: //PROCESSOR_ARCHITECTURE_ARM64:
		return _T("arm64");
	default:
		return _T("");
	}
}

bool GetFileVersionString(LPCWSTR path, WCHAR* version, size_t maxlen, BOOL only2)
{
	//
	// Based on example code from this article
	// http://support.microsoft.com/kb/167597
	//

	DWORD handle;
	DWORD len = GetFileVersionInfoSizeW(path, &handle);
	if (!len)
		return false;

	std::unique_ptr<uint8_t> buff(new (std::nothrow) uint8_t[len]);
	if (!buff)
		return false;

	if (!GetFileVersionInfoW(path, 0, len, buff.get()))
		return false;

	VS_FIXEDFILEINFO* vInfo = nullptr;
	UINT infoSize;

	if (!VerQueryValueW(buff.get(), L"\\", reinterpret_cast<LPVOID*>(&vInfo), &infoSize))
		return false;

	if (!infoSize)
		return false;

	if (only2) {
		swprintf_s(version, maxlen, L"%u.%u",
			HIWORD(vInfo->dwFileVersionMS),
			LOWORD(vInfo->dwFileVersionMS));
	}
	else {
		swprintf_s(version, maxlen, L"%u.%u.%u.%u",
			HIWORD(vInfo->dwFileVersionMS),
			LOWORD(vInfo->dwFileVersionMS),
			HIWORD(vInfo->dwFileVersionLS),
			LOWORD(vInfo->dwFileVersionLS));
	}

	return true;
}

bool GetAppVersionString(WCHAR* version, size_t maxlen)
{
	WCHAR szPath[MAX_PATH];
	GetModuleFileNameW(NULL, szPath, MAX_PATH);
	return GetFileVersionString(szPath, version, maxlen, TRUE);
}


BOOL SetPrivilege(
	HANDLE hToken,          // token handle
	LPCTSTR Privilege,      // Privilege to enable/disable
	BOOL bEnablePrivilege   // TRUE to enable.  FALSE to disable
)
{
	TOKEN_PRIVILEGES tp;
	LUID luid;
	TOKEN_PRIVILEGES tpPrevious;
	DWORD cbPrevious = sizeof(TOKEN_PRIVILEGES);

	if (!LookupPrivilegeValue(NULL, Privilege, &luid)) return FALSE;

	// 
	// first pass.  get current privilege setting
	// 
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	tp.Privileges[0].Attributes = 0;

	AdjustTokenPrivileges(
		hToken,
		FALSE,
		&tp,
		sizeof(TOKEN_PRIVILEGES),
		&tpPrevious,
		&cbPrevious
	);

	if (GetLastError() != ERROR_SUCCESS) return FALSE;

	// 
	// second pass.  set privilege based on previous setting
	// 
	tpPrevious.PrivilegeCount = 1;
	tpPrevious.Privileges[0].Luid = luid;

	if (bEnablePrivilege) {
		tpPrevious.Privileges[0].Attributes |= (SE_PRIVILEGE_ENABLED);
	}
	else {
		tpPrevious.Privileges[0].Attributes ^= (SE_PRIVILEGE_ENABLED &
			tpPrevious.Privileges[0].Attributes);
	}

	AdjustTokenPrivileges(
		hToken,
		FALSE,
		&tpPrevious,
		cbPrevious,
		NULL,
		NULL
	);

	if (GetLastError() != ERROR_SUCCESS) return FALSE;

	return TRUE;
}


BOOL EnablePrivileges(LPCTSTR *PrivilegeNames, int count)
{
	static const TCHAR* names[] = {
		SE_DEBUG_NAME, SE_INC_BASE_PRIORITY_NAME, SE_INC_WORKING_SET_NAME, SE_LOAD_DRIVER_NAME,
		SE_PROF_SINGLE_PROCESS_NAME, SE_BACKUP_NAME, SE_RESTORE_NAME, SE_SHUTDOWN_NAME, SE_TAKE_OWNERSHIP_NAME
	};

	BOOL              bResult = TRUE;
	HANDLE            hToken;

	if (PrivilegeNames == NULL && count == 0) {
		PrivilegeNames = names;
		count = _countof(names);
	}

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &hToken))
	{
		return FALSE;
	}

	for (int i = 0; i < count; i++)
	{
		SetPrivilege(hToken, PrivilegeNames[i], TRUE);
	}

	CloseHandle(hToken);

	return TRUE;
}

BOOL EnableBackupPrivilege()
{
	static const TCHAR* names[] = {
		SE_BACKUP_NAME
	};

	return EnablePrivileges(names, _countof(names));
}


static void FillBuf_BadSector(void* buf, DWORD size)
{
	const char* s = "UNREADABLESECTOR";
	const DWORD slen = 16;

	DWORD len = min(slen, size);
	memcpy(buf, s, len);
	memset((char*)buf + len, 0, size - len);
}


#ifndef ERROR_DEVICE_HARDWARE_ERROR
#define ERROR_DEVICE_HARDWARE_ERROR      483L
#endif

BOOL my_ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, uint64_t pos, DWORD dwBytesPerSector/*=512*/)
{
	if (!ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, NULL)) {
		DWORD errorID = GetLastError();
		//判断错误类型，扇区CRC或致命性错误
		if (errorID == ERROR_CRC || errorID == ERROR_DEVICE_HARDWARE_ERROR) {
			LARGE_INTEGER liBeforePosition, liReSetFPFail;
			DWORD dwCache_Read = 0;//失败后重新读到的字节数
			DWORD dwReadBytes = 0;
			liBeforePosition.QuadPart = pos;

			void* ptr = lpBuffer;
			int read_cnt = nNumberOfBytesToRead / dwBytesPerSector;
			//将本次想要读取的对象分割，最小单位应为物理扇区大小
			for (int i = 0; i < read_cnt; ++i) {
				if (!ReadFile(hFile, ptr, dwBytesPerSector, &dwReadBytes, NULL)) {
					FillBuf_BadSector(ptr, dwBytesPerSector);
					//将文件指针重置为失败扇区的下一个
					liReSetFPFail.QuadPart = liBeforePosition.QuadPart + dwCache_Read + dwBytesPerSector;
					if (!SetFilePointerEx(hFile, liReSetFPFail, NULL, FILE_BEGIN)) {
						return FALSE;
					}
				}
				else {
					if (dwReadBytes == 0) {
						//读到末尾了
						break;
					}

					if (dwReadBytes != dwBytesPerSector) {
						SetFilePointerEx(hFile, liBeforePosition, NULL, FILE_BEGIN);
						SetLastError(ERROR_INVALID_BLOCK_LENGTH);
						return FALSE;
					}
				}

				ptr = (BYTE*)ptr + dwBytesPerSector;
				dwCache_Read += dwBytesPerSector;//记录已处理字节数
			}
			*lpNumberOfBytesRead = dwCache_Read;
			return TRUE;
		}
		else {
			return FALSE;
		}
	}
	return TRUE;
}

