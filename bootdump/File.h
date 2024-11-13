#pragma once


LPCTSTR PathGetAbsolute(LPCTSTR in, LPTSTR out, int len);

BOOL __Wow64DisableWow64FsRedirection(
	_Out_ PVOID *OldValue
);
BOOL __Wow64RevertWow64FsRedirection(
	_In_ PVOID OldValue
);

HRESULT CreateLnk(LPCTSTR pszTargetfile, LPCTSTR pszTargetargs,
	LPCTSTR pszLinkfile, LPCTSTR pszDescription,
	int iShowmode, LPCTSTR pszCurdir,
	LPCTSTR pszIconfile, int iIconindex, int runasadmin);

BOOL MakeDirs(const wchar_t* path);
wchar_t* search_path_slash(const wchar_t* path, bool reverse);

struct FileItemAttrs {
	long long Size;
	long long DateModified;
	long long DateCreated;
	long Attributes;
};
void* GetFileList(BOOL* pAborter, const wchar_t* path, DWORD *pCount, BOOL need_attrs=FALSE);
const wchar_t* GetFileListItem(void *p, DWORD index, DWORD *pLen, FileItemAttrs *attrs = NULL);
DWORD GetFileListItemCount(void *p);
void FreeFileList(void* p);
BOOL RemoveFileList(void *p);
BOOL DeleteAllFiles(const wchar_t* path);

typedef BOOL CopyDirCallback(void *param, LPCWSTR file, BOOL isDir, BOOL isOK);
BOOL CopyDir(LPCWSTR from, LPCWSTR to, CopyDirCallback* cb, void* param, LPCWSTR wildcard = NULL);

BOOL CopyFileTime(LPCWSTR lpSrc, LPCWSTR lpDest);
BOOL TouchFile_Unicode(LPCWSTR lpszFilename);
LONGLONG FileTime_To_POSIX(FILETIME ft);
BOOL GetFileTimePOSIX(LPCTSTR lpszPath, LONGLONG*ftC, LONGLONG*ftA, LONGLONG*ftM);
BOOL GetMd5Sum(const void* data, const size_t data_size, char result[33]);

int File_GetData(LPCTSTR lpFile, LPVOID lpBuf, DWORD* lpdwSize);
int File_SetData(LPCTSTR lpFile, LPCVOID lpBuf, DWORD dwSize);

BOOL GetFileTime(LPCTSTR lpszPath, FILETIME* ftC, FILETIME* ftA, FILETIME* ftW);
void PathRemoveBackslash_(LPTSTR lpszPath, size_t len=-1);
