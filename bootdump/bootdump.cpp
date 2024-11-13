// cpylockedfiles.cpp : Defines the entry point for the console application.
//

#include "pch.h"
#include "../core/vssclient.h"
#include "../core/osutils.h"
#include "../core/Disks.h"
#include <atlbase.h>
#include <list>
#include <algorithm>
#include <iostream>
#include <fstream>
#include "../core/json.hpp"
#include "File.h"
#include "OlsApi.h"
#include "OlsIoctl.h"
#include "OlsDll.h"
#include "Driver.h"
#include "ring0drv.h"

using json = nlohmann::ordered_json;
										   
std::wstring MetaDir;
std::wstring basedir;

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


int detect_efi()
{
	GetFirmwareEnvironmentVariableA("", "{00000000-0000-0000-0000-000000000000}", NULL, 0);
	if (GetLastError() == ERROR_INVALID_FUNCTION) { // This.. is.. LEGACY BIOOOOOOOOS....
		//printf("Legacy");
		return 0;
	}
	else {
		//printf("UEFI");
		return 1;
	}
}

static BOOL _CopyDirCallback(void* param, LPCWSTR file, BOOL isDir, BOOL isOK)
{
	return TRUE;
}

/*
目的： 找到EFI分区，把其中的所有efi文件拷贝出来
*/
int dump_efi(json &disk)
{
	int i = 0;
	int diskindex = disk["index"];
	json property = disk["property"];
	json partitions = disk["partitions"];
	if (partitions.size() == 0)
	{
		return -1;
	}

	for (i = 0; i < (int)partitions.size(); i++)
	{
		json part = partitions[i];
		int number = part["number"];
		uint64_t length = part["length"];
		if (!length)
			continue;
		uint64_t offset = part["offset"];
		uint64_t length_free = part["free"];
		std::string style = part["style"].get<std::string>();
		std::string label = part["label"].get<std::string>();
		std::string mount = part["mount"].get<std::string>();
		std::string format = part["format"].get<std::string>();
		std::string volume = part["volume"].get<std::string>();
		int BytesPerSector = property["BytesPerSector"];

		if (style == "GPT" && format == "FAT32") {
			std::wstring efidir = (LPCWSTR)CA2W(volume.c_str(), CP_UTF8);
			std::wstring dstdir;
			efidir += L"EFI\\";
			dstdir = basedir + std::wstring(L"HD") + std::to_wstring(diskindex) + L"-" + std::to_wstring(number) + std::wstring(L"\\");
			if (PathFileExistsW(efidir.c_str())) {
				CopyDir(efidir.c_str(), dstdir.c_str(), _CopyDirCallback, NULL, L"*.efi");
			}
		}
	}

	return 0;
}

static bool WriteFileAll(HANDLE hFile, const void* data, DWORD len)
{
	DWORD cbW;
	while (len > 0)
	{
		if (!WriteFile(hFile, data, len, &cbW, NULL))
			return false;

		data = (char*)data + cbW;
		len -= cbW;
	}
	return true;
}


BOOL DumpMBR(int diskno, LPCWSTR lszSaveTo)
{
	WCHAR Drive[MAX_PATH];
	wsprintf(Drive, L"\\\\.\\PhysicalDrive%d", diskno);

	BOOL bOK = FALSE;
	BYTE buf[4096];
	DWORD dwR;
	DWORD dwRSize = 0;
	DWORD dwNeedSize = 1024 * 1024;
	HANDLE hOut = INVALID_HANDLE_VALUE;
	HANDLE hIn = CreateFile(Drive,
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (hIn == INVALID_HANDLE_VALUE) {
		return FALSE;
	}

	hOut = CreateFile(lszSaveTo,
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (hOut == INVALID_HANDLE_VALUE) {
		goto _CLEAN;
	}

	while (dwRSize < dwNeedSize)
	{
		int blocksz = min(dwNeedSize - dwRSize, sizeof(buf));
		if (!ReadFile(hIn, buf, blocksz, &dwR, NULL)) {
			break;
		}
		if (dwR == 0) {
			break;
		}
		if (!WriteFileAll(hOut, buf, dwR)) {
			break;
		}
		dwRSize += dwR;
	}

	bOK = dwRSize == dwNeedSize;
_CLEAN:
	CloseHandle(hIn);
	if (hOut != INVALID_HANDLE_VALUE) {
		CloseHandle(hOut);
	}
	return bOK;
}

BOOL DumpVBR(LPCWSTR lpszVol, LPCWSTR lszSaveTo)
{
	std::wstring volName, devpath;
	volName = lpszVol;
	if (volName.size() && *volName.rbegin() == '\\') {
		volName.pop_back();
	}

	BOOL bOK = FALSE;
	BYTE buf[4096];
	DWORD dwR;
	DWORD dwRSize = 0;
	DWORD dwNeedSize = 1024 * 1024;
	HANDLE hOut = INVALID_HANDLE_VALUE;
	HANDLE hVol = CreateFile(volName.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (hVol == INVALID_HANDLE_VALUE) {
		return FALSE;
	}

	hOut = CreateFile(lszSaveTo,
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (hOut == INVALID_HANDLE_VALUE) {
		goto _CLEAN;
	}

	while (dwRSize < dwNeedSize)
	{
		int blocksz = min(dwNeedSize - dwRSize, sizeof(buf));
		if (!ReadFile(hVol, buf, blocksz, &dwR, NULL)) {
			break;
		}
		if (dwR == 0) {
			break;
		}
		if (!WriteFileAll(hOut, buf, dwR)) {
			break;
		}
		dwRSize += dwR;
	}

	bOK = dwRSize == dwNeedSize;
_CLEAN:
	CloseHandle(hVol);
	if (hOut != INVALID_HANDLE_VALUE) {
		CloseHandle(hOut);
	}
	return bOK;
}

/*
目的： 将磁盘的MBR dump出来，最多dump 1MB，如果是包含OS的磁盘，把系统分区的VBR也dump出来。
*/
int dump_mbr(json& disk)
{
	int i = 0;
	int diskindex = disk["index"];
	json property = disk["property"];
	json partitions = disk["partitions"];
	if (partitions.size() == 0)
	{
		return -1;
	}

	std::wstring dmpfile = basedir + std::wstring(L"HD") + std::to_wstring(diskindex) + std::wstring(L"\\MBR.bin");
	MakeDirs(dmpfile.c_str());
	if (!DumpMBR(diskindex, dmpfile.c_str())) {
		fprintf(stderr, "ERROR: dump MBR(HD%d) err=%d.\n", diskindex, GetLastError());
	}

	for (i = 0; i < (int)partitions.size(); i++)
	{
		json part = partitions[i];
		int number = part["number"];
		uint64_t length = part["length"];
		if (!length)
			continue;
		uint64_t offset = part["offset"];
		uint64_t length_free = part["free"];
		std::string style = part["style"].get<std::string>();
		std::string label = part["label"].get<std::string>();
		std::string mount = part["mount"].get<std::string>();
		std::string format = part["format"].get<std::string>();
		std::string volume = part["volume"].get<std::string>();
		int BytesPerSector = property["BytesPerSector"];

		std::wstring volumeW = (LPCWSTR)CA2W(volume.c_str(), CP_UTF8);
		std::wstring windll = volumeW + L"Windows\\System32\\ntdll.dll";
		dmpfile = basedir + std::wstring(L"HD") + std::to_wstring(diskindex) + L"-" + std::to_wstring(number) + std::wstring(L"\\VBR.bin");
		if (PathFileExistsW(windll.c_str())) {
			MakeDirs(dmpfile.c_str());
			if (!DumpVBR(volumeW.c_str(), dmpfile.c_str())) {
				fprintf(stderr, "ERROR: dump VBR(HD%d-%d) err=%d.\n", diskindex, number, GetLastError());
			}
		}
	}

	return 0;
}

bool dump_BiosFirmware2(LPCWSTR outdir, DWORD rom_size, void **data)
{
	bool result = false;
	BYTE* rom = (BYTE*)malloc(rom_size);
	DWORD chunksize = 4096;
	DWORD phyptr = 0xffffffff - chunksize + 1;
	DWORD read_total = 0;
	BYTE* readptr;
	*data = NULL;
	memset(rom, 0, rom_size);
	while (read_total < rom_size) {
		readptr = rom + rom_size - read_total - chunksize;
		if (!ReadPhysicalMemory(phyptr, readptr, chunksize, 1)) {
			if (read_total < 64*1024)
				goto _ERROR;
		}
		read_total += chunksize;
		phyptr -= chunksize;
	}
	*data = rom;
	rom = NULL;
	result = true;
_ERROR:
	if (rom)
		free(rom);
	return result;
}

int dump_BiosFirmware(json& j)
{
	j["result"] = false;
	std::wstring dstdir;
	dstdir = basedir + L"BIOS" + std::wstring(L"\\");
	if (!MakeDirs(dstdir.c_str())) {
		fprintf(stderr, "ERROR: make BiosFirmware dumpdir failed.\n");
		j["error"] = "make dumpdir failed";
		return 1;
	}
	TCHAR drvdir[MAX_PATH];
	std::wstring drvfile, drvfileX64;
	TCHAR* ptr;

	GetModuleFileName(NULL, drvdir, MAX_PATH);
	if ((ptr = _tcsrchr(drvdir, '\\')) != NULL)
	{
		*ptr = '\0';
	}

	ManageDriver(OLS_DRIVER_ID, _T(""), OLS_DRIVER_REMOVE);

	drvfileX64 = drvdir + std::wstring(L"\\") + OLS_DRIVER_FILE_NAME_WIN_NT_X64;
	if (0 != File_SetData(drvfileX64.c_str(), Ring0x64, sizeof(Ring0x64))) {
		j["error"] = "create WinRing0x64.sys failed";
		return 1;
	}
	drvfile = drvdir + std::wstring(L"\\") + OLS_DRIVER_FILE_NAME_WIN_NT;
	if (0 != File_SetData(drvfile.c_str(), Ring0x86, sizeof(Ring0x86))) {
		j["error"] = "create WinRing0.sys failed";
		DeleteFileW(drvfileX64.c_str());
		return 1;
	}

	if (!InitializeOls())
	{
		fprintf(stderr, "ERROR: InitializeOls()!!\n");
		j["error"] = "InitializeOls";
		DeleteFileW(drvfile.c_str());
		DeleteFileW(drvfileX64.c_str());
		return 1;
	}

	DWORD rom_size = 1024 * 1024 * 2; //大概率固件是2MB，或者小于2MB
	BYTE* rom_binary = NULL;
	std::wstring dstfile;
	dstfile = dstdir + L"bios.rom";

	if (dump_BiosFirmware2(dstdir.c_str(), rom_size, (void**)&rom_binary)) {
		if (0 == File_SetData(dstfile.c_str(), rom_binary, rom_size)) {
			j["result"] = true;
		}
		else {
			j["error"] = "create bios.rom failed";
		}
		free(rom_binary);
	}
	else {
		j["error"] = "ReadPhysicalMemory failed";
	}
	DeinitializeOls();
	DeleteFileW(drvfile.c_str());
	DeleteFileW(drvfileX64.c_str());
	return j["result"] ? 0 : -1;
}

int wmain(int argc, wchar_t* argv[])
{
	LPCWSTR outdir = NULL;
	BOOL flag_biosdump = FALSE;
	int a;
	for (a = 1; a < argc; a++) {
		if (_wcsicmp(argv[a], L"-biosdump") == 0) {
			flag_biosdump = TRUE;
		}
		else if (a+1 == argc){
			outdir = argv[a];	//output-dir必需是最后一个参数
		}
	}

	if (!outdir)
	{
		wprintf(L"Usage: bootdump.exe <-biosdump> <output-dir>\n");
		return 1;
	}

	//if (IsWow64()) {
	//	wprintf(L"err: do not run x86 version on x64 system\n");
	//	return 1;
	//}

	std::string text;
	json jdisks;
	json jHDs = {
	{"biosmode" , detect_efi() ? "UEFI" : "Legacy"},
	{"biosdump", {}},
	{"disks", {}}
	};

	int i = 0;
	text = enumPartitions();
	jdisks = json::parse(text);

	for (i = 0; i < (int)jdisks.size(); i++)
	{
		json disk = jdisks[i];
		json property = disk["property"];
		std::string BusType = property["BusType"].get<std::string>();
		if (BusType != "Usb") {
			//不包含USB接口上的磁盘
			jHDs["disks"] += disk;
		}
	}

	jdisks.clear();
	text = jHDs.dump(2);

	if (jHDs.size() == 0)
	{
		return 1;
	}

	MetaDir = outdir;
	PathRemoveBackslash_(&MetaDir[0]);
	MetaDir = MetaDir.c_str();
	basedir = MetaDir + L"\\Dumps\\boot\\";
	if (!MakeDirs(basedir.c_str())) {
		fprintf(stderr, "ERROR: make base dir failed.\n");
		return 1;
	}

	for (i = 0; i < (int)jHDs["disks"].size(); i++)
	{
		json disk = jHDs["disks"][i];
		json property = disk["property"];
		std::string style = disk["style"].get<std::string>();

		if (style == "GPT") {
			dump_efi(disk);
		}
		else {
			dump_mbr(disk);
		}
	}

	if (flag_biosdump) {
		dump_BiosFirmware(jHDs["biosdump"]);
	}
	else {
		jHDs["biosdump"]["result"] = false;
		jHDs["biosdump"]["error"] = "ignored";
	}

	std::ofstream savejson;
	savejson.open(basedir + L"boot.json");
	savejson << jHDs.dump(2);
	savejson.close();
	return 0;
}
