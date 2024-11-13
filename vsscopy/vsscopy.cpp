// cpylockedfiles.cpp : Defines the entry point for the console application.
//

#include "pch.h"
#include "../core/vssclient.h"
#include "../core/osutils.h"
#include <list>
#include <algorithm>

bool copy(FILE* src, FILE* dst)
{
	BYTE buf[4096];
	size_t n;

	while (1) {
		n = fread(buf, 1, sizeof(buf), src);
		if (n) {
			if (1 != fwrite(buf, n, 1, dst)) {
				return false;
			}
		}
		else {
			break;
		}
	}
	return true;
}

int wmain(int argc, wchar_t* argv[])
{
	if (argc < 2)
	{
		wprintf(L"Usage: vsscopy.exe <output-dir> file1 file2 ...\n");
		return 1;
	}
	if (!IsElevated()) {
		wprintf(L"err: not elevated\n");
		return 1;
	}

	if (IsWow64()) {
		wprintf(L"err: do not run x86 version on x64 system\n");
		return 1;
	}

	std::wstring outputdir = argv[1];
	std::list<std::wstring> filelist;
	std::list<std::wstring> volumelist;
	int i;
	for (i = 2; i < argc; i++) {
		filelist.push_back(argv[i]);
	}

	int res;
	Cvssclient vssc;
	WCHAR szDrive[4] = L"C:\\";
	std::wstring devpath;
	res = vssc.Init();
	if (!res) {
		wprintf(L"err: vssc init, hr=0x%.8x\n", vssc.LastErr());
		return -1;
	}

	for (auto& f : filelist) {
		szDrive[0] = f[0];
		if (std::find(volumelist.begin(), volumelist.end(), szDrive) == volumelist.end()) {
			res = vssc.IsVolumeSupported(szDrive);
			if (!res) {
				wprintf(L"err: volume %s is not supported VSS, hr=0x%.8x\n", szDrive, vssc.LastErr());
				return -1;
			}
			volumelist.push_back(szDrive);
		}
	}

	printf("Starting Snapshot...\n");
	res = vssc.StartSnapshot();
	if (!res) {
		printf("err: vssc StartSnapshot, hr=0x%.8x\n", vssc.LastErr());
		return -1;
	}

	for (auto& f : volumelist) {
		res = vssc.AddVolume(f.c_str(), FALSE);
		if (!res) {
			wprintf(L"err: add volume %s, hr=0x%.8x\n", f.c_str(), vssc.LastErr());
			return -1;
		}
	}

	printf("Making Snapshot...\n");
	res = vssc.MakeSnapshot();
	if (!res) {
		printf("err: vssc MakeSnapshot, hr=0x%.8x\n", vssc.LastErr());
		return -1;
	}
	int copied = 0;
	i = 0;
	for (auto& f : filelist) {
		i += 1;
		wprintf(L"Coping File: %s ...", f.c_str());
		szDrive[0] = f[0];
		res = vssc.GetVolumeSnapshotDevice(szDrive, devpath);
		if (!res) {
			wprintf(L"err.\n");
		}
		else {
			std::wstring srcname;
			std::wstring dstname;
			srcname = devpath + &f[2];

			dstname = outputdir;
			if (dstname[dstname.size() - 1] != '\\')
				dstname += L"\\";
			dstname += std::to_wstring(i) + L"-";
			dstname += PathFindFileNameW(f.c_str());

			FILE* src = NULL, *dst = NULL; 
			src = _wfopen(srcname.c_str(), L"rb");
			if (src) {
				dst = _wfopen(dstname.c_str(), L"wb");
				if (dst) {
					if (copy(src, dst)) {
						wprintf(L"ok.\n");
						copied++;
					}
				}
				else {
					wprintf(L"failed to save.\n");
				}
			}
			else {
				wprintf(L"failed to open.\n");
			}
		}
	}
	wprintf(L"Cleaning Snapshots...\n");
	vssc.DeleteSnapshots();

	return copied == (int)filelist.size() ? 0 : -2;
}



