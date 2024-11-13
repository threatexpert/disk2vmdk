#pragma once

#include <string>

std::wstring GetErrorAsString(DWORD errorMessageID);
std::wstring GetLastErrorAsString();
std::wstring fromHResult(HRESULT h);
BOOL IsWow64();

#ifndef _versionhelpers_H_INCLUDED_
BOOL IsWindows10OrGreater();
BOOL IsWindows7OrGreater();
BOOL IsWindowsVistaOrGreater();
BOOL IsWindowsVersionOrGreater(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor);
BOOL IsWindowsXPSP2OrGreater();
BOOL IsWindowsXPOrGreater();
BOOL IsWindows2003OrGreater();
BOOL IsWindowsServer();
#endif //_versionhelpers_H_INCLUDED_

BOOL IsElevated();
BOOL Is64BitWindows();
LPCTSTR GetArch();
LPCTSTR GetAppArch();
bool GetFileVersionString(LPCWSTR path, WCHAR* version, size_t maxlen, BOOL only2);
bool GetAppVersionString(WCHAR* version, size_t maxlen);

BOOL EnableBackupPrivilege();

BOOL my_ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, uint64_t pos, DWORD dwBytesPerSector = 512);
