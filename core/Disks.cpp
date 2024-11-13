#include "pch.h"
#include <inttypes.h>
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <time.h>
#include <winioctl.h>
#include <memory>
#include <string>
#include <list>
#include <atlbase.h>
#include <shlobj.h>
#include "human-readable.h"
#include "wmiutil.h"
#include "json.hpp"
using json = nlohmann::ordered_json;


const char* VolumeProtectionStatus_tostring(int t)
{
    switch (t)
    {
    case 0:
        return "no";
    case 1:
        return "unlocked";
    case 2:
        return "locked";
    default:
        return "";
    }
}
const wchar_t* VolumeProtectionStatus_tostringzh(int t)
{
    switch (t)
    {
    case 0:
        return L"无";
    case 1:
        return L"已解锁";
    case 2:
        return L"加密";
    default:
        return L"";
    }
}

static BOOL
_IsWindowsVersionOrGreater(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor)
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

static BOOL
_IsWindowsVistaOrGreater()
{
    return _IsWindowsVersionOrGreater(HIBYTE(_WIN32_WINNT_VISTA), LOBYTE(_WIN32_WINNT_VISTA), 0);
}

static  BOOL IsElevated() {

    if (!_IsWindowsVistaOrGreater()) {
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

int GetVolumeProtectionStatus(char drive_letter)
{
    //如果没有管理员权限，Win32_EncryptableVolume是没有权限调用的。直接返回-1.避免后续IWbemLocator::ConnectServer会卡住一会儿。
    if (!IsElevated()) {
        return -1;
    }
    CWMIUtil w;
    int ret;
    ret = w.Init(L"root\\CIMV2\\Security\\MicrosoftVolumeEncryption");
    if (ret != 0) {
        return -1;
    }

    wchar_t szFilter[] = L"DriveLetter='C:'";
    szFilter[13] = drive_letter;

    ret = w.ExecQuery(L"Win32_EncryptableVolume", szFilter);
    if (ret != 0) {
        return -1;
    }

    std::wstring val;
    HRESULT hr;
    if (!w.Next()) {
        return -1;
    }
    hr = w.Get(L"ProtectionStatus", val);
    if (!SUCCEEDED(ret)) {
        return -1;
    }
    return _wtoi(val.c_str());
}

void jsonstring_append(json& j, const char* p, const char* sep = NULL)
{
    if (!sep)
        sep = ",";
    if (j.get<std::string>().size() > 0)
        j = j.get<std::string>() + sep;
    j = j.get<std::string>() + p;
}

std::string string_rtrim(std::string s, const char* trim_ch)
{
    size_t end;
    end = s.find_last_not_of(trim_ch);
    std::string ret = (end == std::string::npos) ? "" : s.substr(0, end + 1);
    return ret;
}

std::string BusType_tostring(STORAGE_BUS_TYPE  BusType)
{
    enum {
        BusTypeFileBackedVirtual_ = BusTypeFileBackedVirtual,
        BusTypeSpaces_,
        BusTypeNvme_,
        BusTypeSCM_,
        BusTypeUfs_,
        BusTypeMax_
    };

    switch (BusType)
    {
    case BusTypeUnknown: return "Unknown";
    case BusTypeScsi: return "Scsi";
    case BusTypeAtapi: return "Atapi";
    case BusTypeAta: return "Ata";
    case BusType1394: return "1394";
    case BusTypeSsa: return "Ssa";
    case BusTypeFibre: return "Fibre";
    case BusTypeUsb: return "Usb";
    case BusTypeRAID: return "RAID";
    case BusTypeiScsi: return "iScsi";
    case BusTypeSas: return "Sas";
    case BusTypeSata: return "Sata";
    case BusTypeSd: return "Sd";
    case BusTypeMmc: return "Mmc";
    case BusTypeVirtual: return "Virtual";
    case BusTypeFileBackedVirtual: return "FileBackedVirtual";
    case BusTypeSpaces_: return "Spaces";
    case BusTypeNvme_: return "Nvme";
    case BusTypeSCM_: return "SCM";
    case BusTypeUfs_: return "Ufs";
    case BusTypeMax_: return "Max";
    default:
        return "";
    }
}

const char* MbrPartitionType_tostring(BYTE PartitionType, bool* phidden)
{
    bool hide = false;
    const char* type = "";
    switch (PartitionType)
    {
    case PARTITION_ENTRY_UNUSED: type = "unused"; break;
    case PARTITION_FAT_12: type = "FAT12"; break;
    case PARTITION_FAT_12 + 0x10: type = "FAT12"; hide = true; break;
    case PARTITION_XENIX_1: type = "Xenix 1"; break;
    case PARTITION_XENIX_2: type = "Xenix 2"; break;
    case PARTITION_FAT_16: type = "FAT16"; break;
    case PARTITION_FAT_16 + 0x10: type = "FAT12"; break;
    case PARTITION_EXTENDED: type = "Extended"; break;
    case PARTITION_HUGE: type = "Huge"; break;
    case PARTITION_HUGE + 0x10: type = "Huge"; hide = true; break;
    case PARTITION_IFS: type = "NTFS"; break;
    case PARTITION_IFS + 0x10: type = "NTFS"; hide = true; break;
    case PARTITION_OS2BOOTMGR: type = "OS2 Boot Manager"; break;
    case PARTITION_FAT32: type = "FAT32"; break;
    case PARTITION_FAT32 + 0x10: type = "FAT32"; hide = true; break;
    case PARTITION_FAT32_XINT13: type = "FAT32 Int13"; break;
    case PARTITION_FAT32_XINT13 + 0x10: type = "FAT32 Int13"; hide = true; break;
    case PARTITION_XINT13: type = "Int13"; break;
    case PARTITION_XINT13 + 0x10: type = "Int13"; hide = true; break;
    case PARTITION_XINT13_EXTENDED: type = "Int13 Extended"; break;
    case PARTITION_XINT13_EXTENDED + 0x10: type = "Int13 Extended"; hide = true; break;
    case PARTITION_PREP: type = "Prep"; break;
    case PARTITION_LDM: type = "LDM"; break;
    case PARTITION_UNIX: type = "Unix"; break;
    }

    if (phidden)
        *phidden = hide;
    return type;
}

int EnumPhysicalDrive(std::list<int>& ls)
{
    DWORD ret;
    int bufsize = 4096;
    auto buf = std::make_unique<WCHAR[]>(bufsize);

    while (TRUE) {
        ret = QueryDosDeviceW(NULL, buf.get(), bufsize);
        if (!ret) {
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                bufsize += 4096;
                buf = std::make_unique<WCHAR[]>(bufsize);
            }
            else {
                return 0;
            }
        }
        else {
            break;
        }
    }

    int count = 0;
    WCHAR* dev = buf.get();
    while (*dev) {
        size_t len = wcslen(dev);

        if (_wcsnicmp(dev, L"PhysicalDrive", 13) == 0) {
            int Index = _wtoi(dev + 13);
            ls.push_back(Index);
            count++;
        }
        dev += len + 1;
    }
    return count;
}

bool GetDriveProperty(HANDLE hDevice, json& res)
{
    STORAGE_PROPERTY_QUERY storagePropertyQuery;
    storagePropertyQuery.PropertyId = StorageDeviceProperty;
    storagePropertyQuery.QueryType = PropertyStandardQuery;
    STORAGE_DESCRIPTOR_HEADER storageDescriptorHeader;
    DISK_GEOMETRY_EX pdg = { 0 };
    DWORD junk = 0;

    res = {
        {"ProductId", ""},
        {"VendorId", ""},
        {"BusType", ""},
        {"SerialNumber", ""},
        {"DiskSize", 0},
        {"Cylinders", 0},
        {"TracksPerCylinder", 0},
        {"SectorsPerTrack", 0},
        {"BytesPerSector", 0},
    };

    if (!DeviceIoControl(hDevice,                       // device to be queried
        IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, // operation to perform
        NULL, 0,                       // no input buffer
        &pdg, sizeof(pdg),            // output buffer
        &junk,                         // # bytes returned
        (LPOVERLAPPED)NULL)) {          // synchronous I/O

        if (!DeviceIoControl(hDevice,
            IOCTL_DISK_GET_DRIVE_GEOMETRY,
            NULL, 0,
            &pdg.Geometry, sizeof(pdg.Geometry),
            &junk,
            (LPOVERLAPPED)NULL)) {
            return false;
        }
        pdg.DiskSize.QuadPart = pdg.Geometry.Cylinders.QuadPart * pdg.Geometry.TracksPerCylinder * pdg.Geometry.SectorsPerTrack * pdg.Geometry.BytesPerSector;
    }

    res["DiskSize"] = pdg.DiskSize.QuadPart;
    res["Cylinders"] = pdg.Geometry.Cylinders.QuadPart;
    res["TracksPerCylinder"] = pdg.Geometry.TracksPerCylinder;
    res["SectorsPerTrack"] = pdg.Geometry.SectorsPerTrack;
    res["BytesPerSector"] = pdg.Geometry.BytesPerSector;

    DWORD dwBytesReturned = 0;
    if (!DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &storagePropertyQuery, sizeof(STORAGE_PROPERTY_QUERY),
        &storageDescriptorHeader, sizeof(STORAGE_DESCRIPTOR_HEADER), &dwBytesReturned, NULL))
        return false;
    //allocate a suitable buffer
    const DWORD dwOutBufferSize = storageDescriptorHeader.Size;
    std::unique_ptr<BYTE[]> pOutBuffer{ new BYTE[dwOutBufferSize]{} };
    //call DeviceIoControl with the allocated buffer
    if (!DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &storagePropertyQuery, sizeof(STORAGE_PROPERTY_QUERY),
        pOutBuffer.get(), dwOutBufferSize, &dwBytesReturned, NULL))
        return false;

    STORAGE_DEVICE_DESCRIPTOR* pDeviceDescriptor = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(pOutBuffer.get());
    if (pDeviceDescriptor->VendorIdOffset && pDeviceDescriptor->VendorIdOffset != ~0) {
        res["VendorId"] = string_rtrim(reinterpret_cast<const char*>(pOutBuffer.get() + pDeviceDescriptor->VendorIdOffset), " ");
    }

    if (pDeviceDescriptor->ProductIdOffset && pDeviceDescriptor->ProductIdOffset != ~0) {
        res["ProductId"] = string_rtrim(reinterpret_cast<const char*>(pOutBuffer.get() + pDeviceDescriptor->ProductIdOffset), " ");
    }

    if (pDeviceDescriptor->SerialNumberOffset && pDeviceDescriptor->SerialNumberOffset != ~0) {
        res["SerialNumber"] = reinterpret_cast<const char*>(pOutBuffer.get() + pDeviceDescriptor->SerialNumberOffset);
    }
    res["BusType"] = BusType_tostring(pDeviceDescriptor->BusType);

    return true;
}

bool GetDriveProperty(int Index, json& res) {
    WCHAR Drive[MAX_PATH];

    wsprintf(Drive, L"\\\\.\\PhysicalDrive%d", Index);
    //get a handle to the first physical drive
    HANDLE h = CreateFileW(Drive, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    //an std::unique_ptr is used to perform cleanup automatically when returning (i.e. to avoid code duplication)
    std::unique_ptr<std::remove_pointer<HANDLE>::type, void(*)(HANDLE)> hDevice{ h, [](HANDLE handle) {CloseHandle(handle); } };
    //initialize a STORAGE_PROPERTY_QUERY data structure (to be used as input to DeviceIoControl)

    return GetDriveProperty(hDevice.get(), res);
}

char GetVolumeMountPoint(WCHAR* volName)
{
    for (WCHAR x = 'C'; x <= 'Z'; x++) {
        WCHAR drivepath[4] = { x, ':', '\\', 0 };
        WCHAR szVol[MAX_PATH];
        if (GetVolumeNameForVolumeMountPointW(drivepath, szVol, MAX_PATH)) {
            if (_wcsicmp(volName, szVol) == 0) {
                return (char)drivepath[0];
            }
        }
    }
    return 0;

    //DWORD charCount = MAX_PATH;
    //WCHAR* mp = NULL, * mps = NULL;
    //bool success;
    //char letter = 0;

    //while (true)
    //{
    //    mps = new WCHAR[charCount];
    //    success = GetVolumePathNamesForVolumeNameW(volName, mps, charCount, &charCount) != 0;
    //    if (success || GetLastError() != ERROR_MORE_DATA)
    //        break;
    //    delete[] mps;
    //    mps = NULL;
    //}
    //if (success)
    //{
    //    for (mp = mps; mp[0] != '\0'; mp += wcslen(mp)) {
    //        if (mp[1] == ':') {
    //            letter = (char)mp[0];
    //        }
    //    }
    //}
    //delete[] mps;
    //return letter;
}

bool findVolume(WCHAR* volName, int diskno, long long offs, long long len)
{
    HANDLE vol;
    bool success;

    vol = FindFirstVolume(volName, MAX_PATH); //I'm cheating here! I only know volName is MAX_PATH long because I wrote so in enumPartitions findVolume call
    success = vol != INVALID_HANDLE_VALUE;
    while (success)
    {
        //We are now enumerating volumes. In order for this function to work, we need to get partitions that compose this volume
        HANDLE volH;
        PVOLUME_DISK_EXTENTS vde;
        DWORD bret;

        volName[wcslen(volName) - 1] = '\0'; //For this CreateFile, volume must be without trailing backslash
        volH = CreateFile(volName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
        volName[wcslen(volName)] = '\\';
        if (volH != INVALID_HANDLE_VALUE)
        {
            bret = sizeof(VOLUME_DISK_EXTENTS) + 256 * sizeof(DISK_EXTENT);
            vde = (PVOLUME_DISK_EXTENTS)malloc(bret);
            if (DeviceIoControl(volH, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, (void*)vde, bret, &bret, NULL))
            {
                for (DWORD i = 0; i < vde->NumberOfDiskExtents; i++)
                    if (vde->Extents[i].DiskNumber == diskno &&
                        vde->Extents[i].StartingOffset.QuadPart == offs &&
                        vde->Extents[i].ExtentLength.QuadPart == len)
                    {
                        free(vde);
                        CloseHandle(volH);
                        FindVolumeClose(vol);
                        return true;
                    }
            }
            free(vde);
            CloseHandle(volH);
        }

        success = FindNextVolume(vol, volName, MAX_PATH) != 0;
    }
    FindVolumeClose(vol);
    return false;
}

bool GetVolumeInfo(WCHAR* volName, json& res)
{
    WCHAR volumeName[MAX_PATH + 1] = { 0 };
    WCHAR fileSystemName[MAX_PATH + 1] = { 0 };
    DWORD serialNumber = 0;
    DWORD maxComponentLen = 0;
    DWORD fileSystemFlags = 0;
    if (GetVolumeInformationW(volName, volumeName, ARRAYSIZE(volumeName), &serialNumber, &maxComponentLen, &fileSystemFlags, fileSystemName, ARRAYSIZE(fileSystemName)))
    {
        if (volumeName[0])
            res["label"] = (LPCSTR)CW2A(volumeName, CP_UTF8);
        if (fileSystemName[0])
            res["format"] = (LPCSTR)CW2A(fileSystemName, CP_UTF8);
        return true;
    }
    return false;
}

//http://velisthoughts.blogspot.com/2012/02/enumerating-and-using-partitions-and.html

void enumPartitions(json& res)
{
    static const GUID PARTITION_BASIC_DATA_GUID = { 0xEBD0A0A2L, 0xB9E5, 0x4433, { 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7 } };
    static const GUID PARTITION_ENTRY_UNUSED_GUID = { 0 };
    static const GUID PARTITION_SYSTEM_GUID = { 0xc12a7328L, 0xf81f, 0x11d2, { 0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b } };
    static const GUID PARTITION_MSFT_RESERVED_GUID = { 0xe3c9e316L, 0x0b5c, 0x4db8, { 0x81, 0x7d, 0xf9, 0x2d, 0xf0, 0x02, 0x15, 0xae } };
    static const GUID PARTITION_MSFT_RECOVERY_GUID = { 0xde94bba4L, 0x06d1, 0x4d40, { 0xa1, 0x6a, 0xbf, 0xd5, 0x01, 0x79, 0xd6, 0xac } };

    std::list<int> drvls;
    EnumPhysicalDrive(drvls);
    drvls.sort();
    PDRIVE_LAYOUT_INFORMATION_EX partitions;
    DWORD partitionsSize = sizeof(DRIVE_LAYOUT_INFORMATION_EX) + 127 * sizeof(PARTITION_INFORMATION_EX);
    partitions = (PDRIVE_LAYOUT_INFORMATION_EX)malloc(partitionsSize);
    memset(partitions, 0, partitionsSize);
    DWORD dwBytesReturned;
    char szWindir[MAX_PATH];
    GetWindowsDirectoryA(szWindir, MAX_PATH);

    for (auto i : drvls)
    {
        WCHAR drive[MAX_PATH];
        WCHAR volume[MAX_PATH] = L"";

        wsprintf(drive, L"\\\\.\\PhysicalDrive%d", i);

        HANDLE h = CreateFile(drive, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
        if (h == INVALID_HANDLE_VALUE)
            continue;

        json disk = {
            {"index", i},
            {"property", nullptr},
            {"style", ""},
            {"partitions", {}}
        };

        GetDriveProperty(h, disk["property"]);

        if (DeviceIoControl(h, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, NULL, 0, partitions, partitionsSize, &dwBytesReturned, NULL))
        {
            switch (partitions->PartitionStyle)
            {
            case PARTITION_STYLE_MBR: disk["style"] = "MBR"; break;
            case PARTITION_STYLE_GPT: disk["style"] = "GPT"; break;
            case PARTITION_STYLE_RAW: disk["style"] = "RAW"; break;
            default: disk["style"] = "unknown"; break;
            }
            for (int iPart = 0; iPart < (int)partitions->PartitionCount; iPart++)
            {
                if (partitions->PartitionStyle == PARTITION_STYLE_MBR) {
                    if (partitions->PartitionEntry[iPart].Mbr.PartitionType == PARTITION_ENTRY_UNUSED) {
                        continue;
                    }
                }
                if (0 == partitions->PartitionEntry[iPart].PartitionLength.QuadPart
                    || 0 == partitions->PartitionEntry[iPart].PartitionNumber)
                    continue;

                bool bHidden = false;
                json jpart = {
                    {"number", (int)partitions->PartitionEntry[iPart].PartitionNumber},
                    {"disk_index", i},
                    {"style",""},
                    {"attr", ""},
                    {"offset" , 0},
                    {"length" , 0},
                    {"free",0},
                    {"volume" , ""},
                    {"format" , ""},
                    {"label" , ""},
                    {"mount", ""},
                    {"bitlocker", -1},
                    {"SectorsPerCluster", 0},
                    {"BytesPerSector", 0},
                    {"NumberOfFreeClusters", 0},
                    {"TotalNumberOfCluster", 0}
                };

                if (partitions->PartitionEntry[iPart].PartitionStyle == PARTITION_STYLE_MBR)
                {
                    jpart["style"] = "MBR";

                    if (partitions->PartitionEntry[iPart].Mbr.BootIndicator) {
                        jsonstring_append(jpart["attr"], "boot");
                    }
                    if (partitions->PartitionEntry[iPart].Mbr.RecognizedPartition)
                    {
                        jpart["format"] = MbrPartitionType_tostring(partitions->PartitionEntry[iPart].Mbr.PartitionType, &bHidden);
                    }
                }
                else if (partitions->PartitionEntry[iPart].PartitionStyle == PARTITION_STYLE_GPT)
                {
                    jpart["style"] = "GPT";
                    if (partitions->PartitionEntry[iPart].Gpt.PartitionType != PARTITION_BASIC_DATA_GUID) {
                        if (partitions->PartitionEntry[iPart].Gpt.Name[0]) {
                            jpart["label"] = (LPCSTR)CW2A(partitions->PartitionEntry[iPart].Gpt.Name, CP_UTF8);
                        }
                        else if (partitions->PartitionEntry[iPart].Gpt.PartitionType == PARTITION_SYSTEM_GUID) {
                            jpart["label"] = "EFI system partition.";
                        }
                        else if (partitions->PartitionEntry[iPart].Gpt.PartitionType == PARTITION_MSFT_RESERVED_GUID) {
                            jpart["label"] = "Microsoft reserved partition";
                        }
                        else if (partitions->PartitionEntry[iPart].Gpt.PartitionType == PARTITION_MSFT_RECOVERY_GUID) {
                            jpart["label"] = "Microsoft recovery partition";
                        }
                    }
                    if (partitions->PartitionEntry[iPart].Gpt.Attributes & GPT_BASIC_DATA_ATTRIBUTE_HIDDEN) {
                        bHidden = true;
                    }
                    if (partitions->PartitionEntry[iPart].Gpt.Attributes & GPT_BASIC_DATA_ATTRIBUTE_READ_ONLY) {
                        jsonstring_append(jpart["attr"], "readonly");
                    }
                    if (partitions->PartitionEntry[iPart].Gpt.Attributes & GPT_BASIC_DATA_ATTRIBUTE_SHADOW_COPY) {
                        jsonstring_append(jpart["attr"], "shadowcopy");
                    }
                }
                else if (partitions->PartitionEntry[iPart].PartitionStyle == PARTITION_STYLE_RAW)
                {
                    jpart["style"] = "RAW";
                }
                else
                {
                    continue;
                }

                if (bHidden) {
                    jsonstring_append(jpart["attr"], "hidden");
                }
                jpart["offset"] = partitions->PartitionEntry[iPart].StartingOffset.QuadPart;
                jpart["length"] = partitions->PartitionEntry[iPart].PartitionLength.QuadPart;

                if (!bHidden)
                {
                    if (findVolume(volume, i, partitions->PartitionEntry[iPart].StartingOffset.QuadPart, partitions->PartitionEntry[iPart].PartitionLength.QuadPart))
                    {
                        jpart["volume"] = (LPCSTR)CW2A(volume, CP_UTF8);
                        GetVolumeInfo(volume, jpart);
                        ULARGE_INTEGER lFreeBytes = { 0 };
                        char drivepath[3] = "C:";
                        drivepath[0] = (char)GetVolumeMountPoint(volume);
                        if (drivepath[0]) {
                            jpart["mount"] = drivepath;
                            if (GetDiskFreeSpaceExA(drivepath, NULL, NULL, &lFreeBytes)) {
                                jpart["free"] = lFreeBytes.QuadPart;
                            }

                            jpart["bitlocker"] = GetVolumeProtectionStatus(drivepath[0]);

                            if (_strnicmp(drivepath, szWindir, 2) == 0) {
                                //当前分区是系统盘
                                jpart["ospart"] = true;
                            }
                        }
                        else {
                            if (GetDiskFreeSpaceExW(volume, NULL, NULL, &lFreeBytes)) {
                                jpart["free"] = lFreeBytes.QuadPart;
                            }
                        }

                        DWORD SectorsPerCluster = 0;
                        DWORD BytesPerSector = 0;
                        DWORD NumberOfFreeClusters = 0;
                        DWORD TotalNumberOfCluster = 0;
                        if (GetDiskFreeSpaceA(drivepath, &SectorsPerCluster, &BytesPerSector, &NumberOfFreeClusters, &TotalNumberOfCluster)) {
                            jpart["SectorsPerCluster"] = SectorsPerCluster;
                            jpart["BytesPerSector"] = BytesPerSector;
                            jpart["NumberOfFreeClusters"] = NumberOfFreeClusters;
                            jpart["TotalNumberOfCluster"] = TotalNumberOfCluster;
                        }
                    }
                }
                disk["partitions"] += jpart;
            }
            CloseHandle(h);
        }
        res += disk;
    }
    free(partitions);
}

std::string enumPartitions()
{
    json res = json::array();
    enumPartitions(res);
    //OutputDebugStringA(res.dump(2).c_str());

    return res.dump(2);
}
