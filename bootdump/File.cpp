#include "pch.h"
#include "File.h"
#include <atlpath.h>
#include <Windows.h>
#include <shlobj.h>
#include <winnls.h>
#include <shobjidl.h>
#include <objbase.h>
#include <objidl.h>
#include <shlguid.h>
#include <vector>
#include <string>
#include <Wincrypt.h>


LPCTSTR PathGetAbsolute(LPCTSTR in, LPTSTR out, int len)
{
	LPCTSTR in2 = in;
	if (in == out)
	{
		in2 = _tcsdup(in);
	}
	GetFullPathName(in2, len, out, NULL);
	if (in != in2)
	{
		free((void*)in2);
	}
	return out;
}

BOOL __Wow64DisableWow64FsRedirection(PVOID * OldValue)
{
	typedef BOOL WINAPI DEF_Wow64DisableWow64FsRedirection(
		_Out_ PVOID *OldValue
	);

	static DEF_Wow64DisableWow64FsRedirection *api = NULL;
	if (api == NULL) {
		HMODULE hK32 = GetModuleHandleA("kernel32.dll");
		*(void**)&api = GetProcAddress(hK32, "Wow64DisableWow64FsRedirection");
		if (api == NULL)
			return FALSE;
	}

	return api(OldValue);
}

BOOL __Wow64RevertWow64FsRedirection(PVOID OldValue)
{
	typedef BOOL WINAPI DEF_Wow64RevertWow64FsRedirection(
		_In_ PVOID OldValue
	);

	static DEF_Wow64RevertWow64FsRedirection *api = NULL;
	if (api == NULL) {
		HMODULE hK32 = GetModuleHandleA("kernel32.dll");
		*(void**)&api = GetProcAddress(hK32, "Wow64RevertWow64FsRedirection");
		if (api == NULL)
			return FALSE;
	}

	return api(OldValue);
}




HRESULT CreateLnk(LPCTSTR pszTargetfile, LPCTSTR pszTargetargs,
	LPCTSTR pszLinkfile, LPCTSTR pszDescription,
	int iShowmode, LPCTSTR pszCurdir,
	LPCTSTR pszIconfile, int iIconindex,
	int runasadmin)
{
	HRESULT       hRes;                  /* Returned COM result code */
	IShellLink*   pShellLink;            /* IShellLink object pointer */
	IPersistFile* pPersistFile;          /* IPersistFile object pointer */
	IShellLinkDataList* pdl;

	CoInitialize(NULL);
	hRes = E_INVALIDARG;
	if (
		(pszTargetfile != NULL) && (wcslen(pszTargetfile) > 0) &&
		(pszTargetargs != NULL) &&
		(pszLinkfile != NULL) && (wcslen(pszLinkfile) > 0) &&
		(pszDescription != NULL) &&
		(iShowmode >= 0) &&
		(pszCurdir != NULL) &&
		(pszIconfile != NULL) &&
		(iIconindex >= 0)
		)
	{
		hRes = CoCreateInstance(
			CLSID_ShellLink,     /* pre-defined CLSID of the IShellLink
									 object */
			NULL,                 /* pointer to parent interface if part of
									 aggregate */
			CLSCTX_INPROC_SERVER, /* caller and called code are in same
									 process */
			IID_IShellLink,      /* pre-defined interface of the
									 IShellLink object */
			(LPVOID*)&pShellLink);         /* Returns a pointer to the IShellLink
									 object */
		if (SUCCEEDED(hRes))
		{
			/* Set the fields in the IShellLink object */
			hRes = pShellLink->SetPath(pszTargetfile);
			hRes = pShellLink->SetArguments(pszTargetargs);
			if (wcslen(pszDescription) > 0)
			{
				hRes = pShellLink->SetDescription(pszDescription);
			}
			if (iShowmode > 0)
			{
				hRes = pShellLink->SetShowCmd(iShowmode);
			}
			if (wcslen(pszCurdir) > 0)
			{
				hRes = pShellLink->SetWorkingDirectory(pszCurdir);
			}
			if (wcslen(pszIconfile) > 0 && iIconindex >= 0)
			{
				CString strShort;
				DWORD dwLen = GetShortPathName(pszIconfile, strShort.GetBuffer(MAX_PATH), MAX_PATH);
				strShort.ReleaseBuffer();
				hRes = pShellLink->SetIconLocation(strShort, iIconindex);
			}

			if (runasadmin)
			{
				hRes = pShellLink->QueryInterface(IID_IShellLinkDataList, (void**)&pdl);
				if (SUCCEEDED(hRes)) {
					DWORD dwFlags = 0;
					hRes = pdl->GetFlags(&dwFlags);
					if (SUCCEEDED(hRes)) {
						if ((SLDF_RUNAS_USER & dwFlags) != SLDF_RUNAS_USER) {
							hRes = pdl->SetFlags(SLDF_RUNAS_USER | dwFlags);
						}
					}
					pdl->Release();
				}
			}

			/* Use the IPersistFile object to save the shell link */
			hRes = pShellLink->QueryInterface(
				IID_IPersistFile,         /* pre-defined interface of the
											  IPersistFile object */
				(LPVOID*)&pPersistFile);            /* returns a pointer to the
											  IPersistFile object */
			if (SUCCEEDED(hRes))
			{
#ifdef UNICODE
				hRes = pPersistFile->Save(pszLinkfile, TRUE);
#else
				WCHAR wszLinkfile[MAX_PATH];
				int           iWideCharsWritten;
				iWideCharsWritten = MultiByteToWideChar(CP_ACP, 0,
					pszLinkfile, -1,
					wszLinkfile, MAX_PATH);
				hRes = pPersistFile->Save(wszLinkfile, TRUE);
#endif
				pPersistFile->Release();
			}
			pShellLink->Release();
		}

	}
	CoUninitialize();
	return (hRes);
}

////
////

wchar_t* search_path_slash(const wchar_t* path, bool reverse)
{
	wchar_t ch;
	if (reverse)
	{
		const wchar_t* last = NULL;
		while (ch = *path)
		{
			if (ch == '\\' || ch == '/')
				last = path;
			++path;
		}
		return (wchar_t*)last;
	}
	else
	{
		while (ch = *path)
		{
			if (ch == '\\' || ch == '/')
				return (wchar_t*)path;
			++path;
		}
		return NULL;
	}
}

BOOL MakeDirs(const wchar_t* path)
{
	BOOL bRet = FALSE;
	wchar_t folder[MAX_PATH];
	wchar_t* end;
	DWORD err = 0;
	ZeroMemory(folder, MAX_PATH * sizeof(wchar_t));

	if (0 != wcsncpy_s(folder, MAX_PATH, path, MAX_PATH)) {
		SetLastError(ERROR_BUFFER_OVERFLOW);
		return FALSE;
	}

	end = search_path_slash(folder, true);
	if (!end) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	*end = '\0';

	//先尝试创建最后一个
	if (CreateDirectoryW(folder, NULL) || GetLastError() == ERROR_ALREADY_EXISTS)
		return TRUE;

	//从前面依次尝试创建
	end = search_path_slash(path, false);

	while (end != NULL)
	{
		if (0 != wcsncpy_s(folder, MAX_PATH, path, end - path + 1)) {
			SetLastError(ERROR_BUFFER_OVERFLOW);
			return FALSE;
		}

		if (!CreateDirectoryW(folder, NULL))
		{
			err = GetLastError();

			if (err != ERROR_ALREADY_EXISTS)
			{
				// do whatever handling you'd like
			}
		}
		end = search_path_slash(++end, false);
	}

	bRet = PathFileExistsW(folder);
	SetLastError(err);
	return bRet;
}


class Cfilelist
{
	std::vector<std::wstring> _ls;
	std::vector<std::wstring>::iterator _it;
	std::vector<FileItemAttrs> _attrs;
	BOOL m_bExit;
	BOOL* m_pAborter;
	BOOL m_bNeedAttrs;
public:

	Cfilelist(BOOL need_attrs) {
		m_bExit = FALSE;
		m_pAborter = &m_bExit;
		m_bNeedAttrs = need_attrs;

	}
	virtual ~Cfilelist() {

	}

	void setAborter(BOOL* p) {
		if (p)
			m_pAborter = p;
		else
			m_pAborter = &m_bExit;
	}

	void attr_from_wfd(FileItemAttrs* attr, const WIN32_FIND_DATAW* wfd)
	{
		if (!wfd)
		{
			memset(attr, 0, sizeof(*attr));
			return;
		}
		LARGE_INTEGER li;
		li.LowPart = wfd->nFileSizeLow;
		li.HighPart = wfd->nFileSizeHigh;
		attr->Size = li.QuadPart;
		attr->Attributes = wfd->dwFileAttributes;
		li.LowPart = wfd->ftCreationTime.dwLowDateTime;
		li.HighPart = wfd->ftCreationTime.dwHighDateTime;
		attr->DateCreated = li.QuadPart;
		li.LowPart = wfd->ftLastWriteTime.dwLowDateTime;
		li.HighPart = wfd->ftLastWriteTime.dwHighDateTime;
		attr->DateModified = li.QuadPart;
	}

	void dir(LPCWSTR lpszPath, const WIN32_FIND_DATAW*pWFD)
	{
		CStringW sDir = lpszPath;
		WIN32_FIND_DATAW wfd;
		FileItemAttrs attr;

		if (sDir.Right(1) != L"\\" && sDir.Right(1) != L"/")
			sDir += L"\\";

		_ls.push_back((LPCWSTR)sDir);
		if (m_bNeedAttrs)
		{
			attr_from_wfd(&attr, pWFD);
			_attrs.push_back(attr);
		}

		HANDLE hFind = FindFirstFileW(sDir + L"*",  &wfd);
		if (hFind == INVALID_HANDLE_VALUE) {
			return;
		}

		do {

			if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				if (wcscmp(wfd.cFileName, L".") == 0 || wcscmp(wfd.cFileName, L"..") == 0) {
					continue;
				}

				dir(sDir + wfd.cFileName, &wfd);
			}
			else {
				_ls.push_back((LPCWSTR)(sDir + wfd.cFileName));
				if (m_bNeedAttrs)
				{
					attr_from_wfd(&attr, &wfd);
					_attrs.push_back(attr);
				}
			}

		} while ( (!*m_pAborter) && FindNextFile(hFind, &wfd));
		FindClose(hFind);
	}

	void ls(LPCWSTR lpszPath, BOOL *pAborter = NULL)
	{
		setAborter(pAborter);

		_ls.clear();

		CStringW sPath = lpszPath;
		BOOL bFileExists = FALSE;
		WIN32_FIND_DATAW wfd;
		FileItemAttrs attr;
		BOOL bFolder = FALSE;

		if (sPath.Right(1) == L"\\" || sPath.Right(1) == L"/")
		{
			sPath.TrimRight('\\');
			sPath.TrimRight('/');
			bFolder = TRUE;
		}

		HANDLE hFind = FindFirstFileW(sPath, &wfd);
		if (hFind != INVALID_HANDLE_VALUE) {
			bFileExists = TRUE;
			FindClose(hFind);
		}

		if (bFileExists)
		{
			if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				dir(sPath, &wfd);
			}
			else if (!bFolder)
			{
				_ls.push_back((LPCWSTR)sPath);
				if (m_bNeedAttrs)
				{
					attr_from_wfd(&attr, &wfd);
					_attrs.push_back(attr);
				}
			}
		}
		_it = _ls.begin();
	}

	DWORD GetCount() {
		return (DWORD)_ls.size();
	}

	const wchar_t* GetFileItem(unsigned int index, DWORD *pLen, FileItemAttrs* attrs) {
		if (index >= _ls.size())
			return NULL;
		if (pLen)
			*pLen = (DWORD)_ls[index].size();
		if (attrs)
		{
			*attrs = _attrs[index];
		}
		return _ls[index].c_str();
	}

	BOOL DeleteFiles() {
		size_t total = _ls.size();
		size_t removed = 0;

		for (std::vector<std::wstring>::reverse_iterator it = _ls.rbegin(); it != _ls.rend(); it++)
		{
			BOOL bOK;
			if (*it->rbegin() == '\\')
			{
				bOK = RemoveDirectoryW(it->c_str());
			}
			else
			{
				bOK = DeleteFileW(it->c_str());
			}

			if (!bOK)
			{
				if (ERROR_FILE_NOT_FOUND == GetLastError())
					bOK = TRUE;
			}

			if (bOK)
			{
				it->clear();
				removed++;
			}
		}

		if (removed == total)
		{
			_ls.clear();
		}
		else
		{
			for (std::vector<std::wstring>::iterator it = _ls.begin(); it != _ls.end(); )
			{
				if (it->empty())
					it = _ls.erase(it);
				else
					it++;
			}
		}

		return _ls.size() == 0;
	}
};

void* GetFileList(BOOL* pAborter, const wchar_t* path, DWORD* pCount, BOOL need_attrs /*= FALSE*/)
{
	Cfilelist* fl = new Cfilelist(need_attrs);
	fl->ls(path, pAborter);
	*pCount = fl->GetCount();
	return fl;
}

const wchar_t* GetFileListItem(void* p, DWORD index, DWORD* pLen, FileItemAttrs* attrs/* = NULL*/)
{
	Cfilelist* fl = (Cfilelist*)p;
	return fl->GetFileItem(index, pLen, attrs);
}

DWORD GetFileListItemCount(void* p)
{
	Cfilelist* fl = (Cfilelist*)p;
	return fl->GetCount();
}

void FreeFileList(void* p)
{
	Cfilelist* fl = (Cfilelist*)p;
	delete fl;
}

BOOL RemoveFileList(void* p)
{
	Cfilelist* fl = (Cfilelist*)p;
	return fl->DeleteFiles();
}

BOOL DeleteAllFiles(const wchar_t* path)
{
	Cfilelist* fl = new Cfilelist(FALSE);
	fl->ls(path, NULL);
	BOOL bOK = fl->DeleteFiles();
	delete fl;
	return bOK;
}

BOOL CopyFileTime(LPCWSTR lpSrc, LPCWSTR lpDest)
{
	BOOL bOK = FALSE;
	HANDLE hFileSrc = 0, hFileDest = 0;
	FILETIME lpCreationTime;
	FILETIME lpLastAccessTime;
	FILETIME lpLastWriteTime;

	hFileSrc = CreateFileW(lpSrc,
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (hFileSrc == INVALID_HANDLE_VALUE) {
		goto error;
	}
	hFileDest = CreateFileW(lpDest,
		FILE_WRITE_ATTRIBUTES,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (hFileDest == INVALID_HANDLE_VALUE) {
		goto error;
	}
	if (!GetFileTime(hFileSrc, &lpCreationTime, &lpLastAccessTime, &lpLastWriteTime))
		goto error;
	if (!SetFileTime(hFileDest, &lpCreationTime, &lpLastAccessTime, &lpLastWriteTime))
		goto error;

	bOK = TRUE;
error:
	if (hFileSrc != INVALID_HANDLE_VALUE)
		CloseHandle(hFileSrc);
	if (hFileDest != INVALID_HANDLE_VALUE)
		CloseHandle(hFileDest);
	return bOK;
}


BOOL CopyDir(LPCWSTR from, LPCWSTR to, CopyDirCallback *cb, void *param, LPCWSTR wildcard /*= NULL*/)
{
	BOOL bRet = FALSE;
	void* fl;
	const wchar_t* pSrcPath = NULL;
	DWORD nSrcPathLen;
	CString strPath;
	DWORD nCount = 0;
	DWORD nCopied = 0;
	CString strBaseName;
	CString strDestDir = to;
	CString strDestFull;

	PathGetAbsolute(from, strPath.GetBuffer(MAX_PATH), MAX_PATH);
	strPath.ReleaseBuffer();
	strPath.TrimRight(L"\\");
	strBaseName = PathFindFileNameW(strPath);

	PathAddBackslashW(strDestDir.GetBuffer(MAX_PATH));
	strDestDir.ReleaseBuffer();

	fl = GetFileList(NULL, strPath, &nCount);
	for (DWORD n = 0; n < nCount; n++)
	{
		LPCWSTR lpSubPath;
		BOOL isDir = FALSE, isOK;
		pSrcPath = GetFileListItem(fl, n, &nSrcPathLen);
		if (pSrcPath[nSrcPathLen - 1] == '\\') {
			isDir = TRUE;
		}
		lpSubPath = pSrcPath + strPath.GetLength() - strBaseName.GetLength();
		strDestFull = strDestDir + lpSubPath;

		if (wildcard == NULL && isDir) {
			isOK = MakeDirs(strDestFull);
			if (isOK)
				nCopied++;
			if (!cb(param, strDestFull, TRUE, isOK))
				break;
		}
		else {
			if (wildcard) {
				if (!PathMatchSpecW(pSrcPath, wildcard))
					continue;
				MakeDirs(strDestFull);
			}
			isOK = CopyFileW(pSrcPath, strDestFull, FALSE);
			if (isOK) {
				nCopied++;
				CopyFileTime(pSrcPath, strDestFull);
			}
			if (!cb(param, pSrcPath, TRUE, isOK))
				break;
		}
	}

	bRet = nCopied == nCount;

	FreeFileList(fl);
	return bRet;
}

BOOL TouchFile_Unicode(LPCWSTR lpszFilename)
{
	BOOL bOK = FALSE;
	FILE *stream = NULL;
	if (_wfopen_s(&stream, lpszFilename, L"r") == ENOENT)
	{
		if (_wfopen_s(&stream, lpszFilename, L"w, ccs=UTF-16LE") == 0)
		{
			bOK = TRUE;
		}
	}
	if (stream != NULL) fclose(stream);
	return bOK;
}

LONGLONG FileTime_To_POSIX(FILETIME ft)
{
	LARGE_INTEGER date, adjust;
	date.HighPart = ft.dwHighDateTime;
	date.LowPart = ft.dwLowDateTime;

	adjust.QuadPart = 11644473600000 * 10000;
	date.QuadPart -= adjust.QuadPart;
	return date.QuadPart / 10000000;
}

void PathRemoveBackslash_(LPTSTR lpszPath, size_t len)
{
	size_t npath = len;
	if (npath == -1)
		npath = _tcslen(lpszPath);
	while (npath > 0 && ( lpszPath[npath-1] == '\\' || lpszPath[npath - 1] == '/') )
	{
		lpszPath[npath - 1] = '\0';
		npath--;
	}
}


BOOL GetFileTimePOSIX(LPCTSTR lpszPath, LONGLONG* ftC, LONGLONG* ftA, LONGLONG* ftM)
{
	BOOL ret = FALSE;
	HANDLE hFile = INVALID_HANDLE_VALUE, hFind = INVALID_HANDLE_VALUE;
	WIN32_FIND_DATA wfd;

	hFile = CreateFile(lpszPath,
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS,
		NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		if (GetLastError() != ERROR_FILE_NOT_FOUND)
			return FALSE;
		TCHAR szPath2[MAX_PATH];
		size_t npath = _tcslen(lpszPath);
		if (!npath || npath >= MAX_PATH)
			return FALSE;
		_tcsncpy_s(szPath2, lpszPath, MAX_PATH);
		PathRemoveBackslash_(szPath2, npath);

		hFind = FindFirstFile(szPath2, &wfd);
		if (hFind == INVALID_HANDLE_VALUE)
			return FALSE;
	}
	else
	{
		if (!GetFileTime(hFile, &wfd.ftCreationTime, &wfd.ftLastAccessTime, &wfd.ftLastWriteTime))
			goto error;
	}

	if (ftC)
		*ftC = FileTime_To_POSIX(wfd.ftCreationTime);
	if (ftA)
		*ftA = FileTime_To_POSIX(wfd.ftLastAccessTime);
	if (ftM)
		*ftM = FileTime_To_POSIX(wfd.ftLastWriteTime);

	ret = TRUE;
error:
	if (hFile != INVALID_HANDLE_VALUE)
		CloseHandle(hFile);
	if (hFind != INVALID_HANDLE_VALUE)
		FindClose(hFind);
	return ret;
}


BOOL GetFileTime(LPCTSTR lpszPath, FILETIME* ftC, FILETIME* ftA, FILETIME* ftW)
{
	BOOL ret = FALSE;
	HANDLE hFile = INVALID_HANDLE_VALUE, hFind = INVALID_HANDLE_VALUE;
	FILETIME CreationTime;
	FILETIME LastAccessTime;
	FILETIME LastWriteTime;
	WIN32_FIND_DATA wfd;

	hFile = CreateFile(lpszPath,
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS,
		NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		if (GetLastError() != ERROR_FILE_NOT_FOUND)
			return FALSE;

		TCHAR szPath2[MAX_PATH];
		size_t npath = _tcslen(lpszPath);
		if (!npath || npath >= MAX_PATH)
			return FALSE;
		_tcsncpy_s(szPath2, lpszPath, MAX_PATH);
		PathRemoveBackslash_(szPath2, npath);

		hFind = FindFirstFile(szPath2, &wfd);
		if (hFind == INVALID_HANDLE_VALUE)
			return FALSE;

		if (ftC)
			*ftC = wfd.ftCreationTime;
		if (ftA)
			*ftA = wfd.ftLastAccessTime;
		if (ftW)
			*ftW = wfd.ftLastWriteTime;
	}
	else
	{
		if (!GetFileTime(hFile, ftC ? ftC : &CreationTime, ftA ? ftA : &LastAccessTime, ftW ? ftW : &LastWriteTime))
			goto error;
	}
	ret = TRUE;
error:
	if (hFile != INVALID_HANDLE_VALUE)
		CloseHandle(hFile);
	if (hFind != INVALID_HANDLE_VALUE)
		FindClose(hFind);
	return ret;
}


enum HashType
{
	HashSha1, HashMd5//, HashSha256
};

BOOL GetHash(const void* data, const size_t data_size, HashType hashType, unsigned char result[64])
{
	HCRYPTPROV hProv = NULL;

	if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
		return FALSE;
	}

	BOOL hash_ok = FALSE;
	HCRYPTPROV hHash = NULL;
	switch (hashType) {
	case HashSha1: hash_ok = CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash); break;
	case HashMd5: hash_ok = CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash); break;
	//case HashSha256: hash_ok = CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash); break;
	}

	if (!hash_ok) {
		CryptReleaseContext(hProv, 0);
		return FALSE;
	}

	if (!CryptHashData(hHash, static_cast<const BYTE*>(data), (DWORD)data_size, 0)) {
		CryptDestroyHash(hHash);
		CryptReleaseContext(hProv, 0);
		return FALSE;
	}

	DWORD cbHashSize = 0, dwCount = sizeof(DWORD);
	if (!CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE*)& cbHashSize, &dwCount, 0)
		|| cbHashSize > 64) {
		CryptDestroyHash(hHash);
		CryptReleaseContext(hProv, 0);
		return FALSE;
	}

	if (!CryptGetHashParam(hHash, HP_HASHVAL, result, &cbHashSize, 0)) {
		CryptDestroyHash(hHash);
		CryptReleaseContext(hProv, 0);
		return FALSE;
	}

	CryptDestroyHash(hHash);
	CryptReleaseContext(hProv, 0);
	return TRUE;
}

BOOL GetMd5Sum(const void* data, const size_t data_size, char result[33])
{
	unsigned char out[64];
	memset(out, 0, sizeof(out));

	if (!GetHash(data, data_size, HashMd5, out))
		return FALSE;

	for (int i = 0; i < 16; i++) {
		sprintf_s(result + i * 2, 3, "%.2x", out[i]);
	}

	return TRUE;
}

int File_GetData(LPCTSTR lpFile, LPVOID lpBuf, DWORD* lpdwSize)
{
	int ret = -1;
	DWORD filesize, total, bytesRead;

	HANDLE hFile = CreateFile(lpFile,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		return -1;
	}

	do
	{

		filesize = GetFileSize(hFile, NULL);

		if (lpBuf == NULL)
		{
			*lpdwSize = filesize;
			ret = 0;
			break;
		}

		if (*lpdwSize < filesize)
		{
			ret = -2;
			break;
		}

		SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
		total = 0;

		while (total < filesize)
		{
			if (!ReadFile(hFile, (char*)lpBuf + total, filesize-total, &bytesRead, NULL))
				break;
			total += bytesRead;
		}

		*lpdwSize = total;
		ret = 0;
		break;
	} while (FALSE);

	CloseHandle(hFile);
	return ret;
}

int File_SetData(LPCTSTR lpFile, LPCVOID lpBuf, DWORD dwSize)
{
	int ret = -1;

	HANDLE hFile = CreateFile(lpFile,
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,
		0,
		NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		return -1;
	}
	DWORD cbW;
	char* data = (char*)lpBuf;
	while (dwSize > 0)
	{
		if (!WriteFile(hFile, data, dwSize, &cbW, NULL))
		{
			CloseHandle(hFile);
			return -2;
		}

		data = (char*)data + cbW;
		dwSize -= cbW;
	}
	CloseHandle(hFile);
	return 0;
}