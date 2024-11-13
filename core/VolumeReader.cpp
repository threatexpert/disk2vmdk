#include "pch.h"
#include "VolumeReader.h"
#include <winioctl.h>
#include <assert.h>
#include <shlwapi.h>
#include "osutils.h"
#include "ImageMaker.h"

CVolumeReader::CVolumeReader()
    : m_hVol(INVALID_HANDLE_VALUE)
    , m_Size(0)
    , m_pos(0)
    , m_dwOptions(0)
{
    _pVolBitMapBuf = NULL;
    _BitMapBytes = 0;
    _ClusterUnitSize = 0;
    _SectorsPerCluster = 0;
    _BytesPerSector = 0;
    _NumberOfFreeClusters = 0;
    _TotalNumberOfCluster = 0;
    _TotalNumberOfClusterInByte = 0;
    memset(_BootSector, 0, sizeof(_BootSector));
    _FSType = 0;
    m_1stClusterOffset = 0;
    _OnlyUsedSpace = FALSE;
    m_bOpenLater = FALSE;
    memset(&m_ft_beforeSnapshot, 0, sizeof(m_ft_beforeSnapshot));
    m_calcedSize = 0;
}

CVolumeReader::~CVolumeReader()
{
    Close();
}

DWORD CVolumeReader::getOptions()
{
    return m_dwOptions;
}

BOOL CVolumeReader::setOptions(DWORD dwOptions)
{
    m_dwOptions = dwOptions;
    return TRUE;
}

BOOL CVolumeReader::setSource(LPCWSTR lpszSrc, uint64_t nSrcSize, BOOL openLater)
{
    m_strVolName = lpszSrc;
    m_Size = nSrcSize;

    if (openLater) {
        m_bOpenLater = openLater;
        return TRUE;
    }

    return openSource();
}

std::wstring CVolumeReader::getSource(BOOL* pOpened)
{
    if (pOpened) {
        *pOpened = m_hVol != INVALID_HANDLE_VALUE;
    }
    return m_strVolName;
}

BOOL CVolumeReader::openSource()
{
    std::wstring volName, devpath;
    volName = m_strVolName;
    if (volName.size() && *volName.rbegin() == '\\') {
        volName.pop_back();
    }

    m_hVol = CreateFile(volName.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (m_hVol == INVALID_HANDLE_VALUE) {
        m_lasterr = L"Failed to open volume，";
        m_lasterr += GetLastErrorAsString();
        return FALSE;
    }

    if (m_dwOptions & OptOnlyUsedSpace) {  //只拷贝有在使用的磁盘空间
        //获取磁盘的Bitmap，Bitmap是以cluster为单位描述一块磁盘空间是否有被使用。用一个位的1或0描述是否被使用。
        //一个cluster的大小通常是格式化时的“分配单元大小”决定的。所以一个字节可以描述8个cluster单元的磁盘空间的使用状态。
        if (GetBitmap()) {
            //Bitmap的第一字节最低位描述第0个cluster的状态，然后按顺序去分析所有cluster状态
            //第0个cluster的偏移在NTFS下就是分区起始位置，这样按cluster单元大小可直接算出每个cluster在分区的绝对逻辑偏移，
            //而FAT的首个cluster不是在分区起始位置，需要从分区的起始512个字节的内容（BootSector）去分析首个cluster的偏移。
            if (GetFStype(_FSType)) {
                if (_FSType == FILESYSTEM_STATISTICS_TYPE_FAT) {
                    if (ReadBootSector()) {
                        if (ParseBootSector()) {
                            _OnlyUsedSpace = TRUE;
                        }
                    }
                    else {
                        return FALSE;
                    }
                }
                else {
                    _OnlyUsedSpace = TRUE;
                }
            }
            if (_OnlyUsedSpace) {
                if ( (m_dwOptions & (OptIgnorePagefile | OptIgnoreHiberfil | OptIgnoreSVI) )
                    || is_snapshot_time_valid()
                    ) {
                    UnmaskSomeFiles();
                }
            }
        }
        //如果获取Bitmap失败，就无法实现只拷贝有在使用的磁盘空间。但是函数还是返回TRUE，默认将拷贝分区的全部空间。
    }

    return TRUE;
}

BOOL CVolumeReader::GetSize(uint64_t* pSize)
{
    *pSize = m_Size;
    return TRUE;
}

BOOL CVolumeReader::SetPos(uint64_t pos)
{
    LARGE_INTEGER liPos, liNewPos;
    liPos.QuadPart = pos;
    if (!SetFilePointerEx(m_hVol, liPos, &liNewPos, FILE_BEGIN)) {
        return FALSE;
    }
    if (liNewPos.QuadPart != pos) {
        return FALSE;
    }
    m_pos = pos;
    return TRUE;
}

BOOL CVolumeReader::Read(void* lpBuffer, DWORD nNumberOfBytesToRead, uint64_t* lpNumberOfBytesRead, BOOL* pIsFreeSpace)
{
    BOOL bRes;
    DWORD dwReadBytes = 0;
    *lpNumberOfBytesRead = 0;
    *pIsFreeSpace = FALSE;
    if (!_OnlyUsedSpace) {
        //完整拷贝模式
        bRes = my_ReadFile(m_hVol, lpBuffer, nNumberOfBytesToRead, &dwReadBytes, m_pos, _BytesPerSector);
        if (bRes) {
            m_pos += dwReadBytes;
            *lpNumberOfBytesRead = dwReadBytes;
        }
        return bRes;
    }
    else {
        //只拷贝正使用的磁盘空间
        uint64_t left;
        if (m_pos == m_Size) {
            return TRUE;
        }
        if (m_pos < m_1stClusterOffset) {
            //fat分区的cluster有偏移的情况，首个cluster前面的空间全部要拷贝
            left = m_1stClusterOffset - m_pos;
            nNumberOfBytesToRead = (DWORD)min(left, nNumberOfBytesToRead);
            bRes = my_ReadFile(m_hVol, lpBuffer, nNumberOfBytesToRead, &dwReadBytes, m_pos, _BytesPerSector);
            if (bRes) {
                m_pos += dwReadBytes;
                *lpNumberOfBytesRead = dwReadBytes;
            }
            return bRes;
        }
        else {
            //GetContiguous根据bitmap信息，将空闲空间和已使用空间的区间按cluster单位取出来
            bool stat;
            int64_t offset_i, offset_j;
            int64_t lcn = (m_pos - m_1stClusterOffset) / _ClusterUnitSize;  //计算当前pos指针指向的cluster
            int64_t clusters = GetContiguous(lcn, stat);    //查询从lcn开始连续相同状态的cluster的数量， stat==true时表示相关cluster都是正使用的空间，否则就是空闲的空间。
            if (!clusters) {
                left = m_Size - m_pos;
                if (left) {
                 //所有Cluster都读取完了，如果还有剩下空间，尝试读取。（实际测试发现ReadFile从m_hVol读取不了，这部分的空间也许只能在PhysicalDrive层面读取）
                    nNumberOfBytesToRead = (DWORD)min(left, nNumberOfBytesToRead);
                    bRes = my_ReadFile(m_hVol, lpBuffer, nNumberOfBytesToRead, &dwReadBytes, m_pos, _BytesPerSector);
                    if (bRes) {
                        m_pos += dwReadBytes;
                        *lpNumberOfBytesRead = dwReadBytes;
                    }
                    return bRes;
                }
                assert(false);
                return FALSE;
            }
            offset_i = m_1stClusterOffset + lcn * _ClusterUnitSize; //lcn（logic cluster NO.）转换为在分区中按字节的偏移量
            offset_j = offset_i + clusters * _ClusterUnitSize;
            left = offset_j - m_pos;    //按字节计算尚未读取的相关cluster的长度
            if (stat) {
                //拷贝磁盘正使用的空间
                nNumberOfBytesToRead = (DWORD)min(left, nNumberOfBytesToRead);
                bRes = my_ReadFile(m_hVol, lpBuffer, nNumberOfBytesToRead, &dwReadBytes, m_pos, _BytesPerSector);
                if (bRes) {
                    m_pos += dwReadBytes;
                    *lpNumberOfBytesRead = dwReadBytes;
                }
                return bRes;
            }
            else {
                //返回空闲空间的长度
                *pIsFreeSpace = TRUE;
                *lpNumberOfBytesRead = left;
                nNumberOfBytesToRead = (DWORD)min(left, nNumberOfBytesToRead);
                //空闲空间的数据不从磁盘读取，直接缓冲区填0后输出，这样转换为vmdk时可以很好被压缩。
                memset(lpBuffer, 0, nNumberOfBytesToRead);
                m_pos += left;
                if (m_pos < m_Size) {
                    LARGE_INTEGER liPos, liNewPos;
                    liPos.QuadPart = left;  //调整指针位置，跳过空闲空间的长度
                    if (!SetFilePointerEx(m_hVol, liPos, &liNewPos, FILE_CURRENT)) {
                        return FALSE;
                    }
                    assert(liNewPos.QuadPart == m_pos);
                }
                return TRUE;
            }
        }
    }
}

void CVolumeReader::Close()
{
    if (m_hVol != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hVol);
        m_hVol = INVALID_HANDLE_VALUE;
    }
    if (_pVolBitMapBuf) {
        free(_pVolBitMapBuf);
        _pVolBitMapBuf = NULL;
    }
}

void CVolumeReader::SetSnapshotTime()
{
    GetSystemTimeAsFileTime(&m_ft_beforeSnapshot);
}

//计算实际需要从磁盘读取的量
BOOL CVolumeReader::CalcDataSize(uint64_t* pSize)
{
    if (!_OnlyUsedSpace)
    {
        //全空间模式不用计算
        *pSize = m_Size;
        return TRUE;
    }

    if (!_ClusterUnitSize) {
        return FALSE;
    }

    if (m_calcedSize) {
        *pSize = m_calcedSize;
        return TRUE;
    }

    bool stat;
    int64_t calcsz = 0;
    int64_t lcn = 0;

    while (1) {
        int64_t clusters = GetContiguous(lcn, stat);
        if (!clusters)
            break;
        if (stat) {
            calcsz += clusters * _ClusterUnitSize;
        }
        lcn += clusters;
    }
    
    m_calcedSize = m_1stClusterOffset + calcsz + (m_Size - _TotalNumberOfClusterInByte);
    assert(m_calcedSize <= m_Size);
    *pSize = m_calcedSize;
    return TRUE;
}


BOOL CVolumeReader::GetBitmap()
{
    BOOL bResult;
    STARTING_LCN_INPUT_BUFFER stStartLCN;
    DWORD dwBitMapSize;
    std::wstring volName;
    volName = m_strVolName;
    if (volName.size() && *volName.rbegin() != '\\') {
        volName += L"\\";
    }

    bResult = FALSE;
    if (!GetDiskFreeSpaceW(volName.c_str(), &_SectorsPerCluster, &_BytesPerSector, &_NumberOfFreeClusters, &_TotalNumberOfCluster)) {
        return FALSE;
    }
    //一个cluster由N个sector（扇区）组成。通常每个sector总是512字节
    _ClusterUnitSize = _SectorsPerCluster * _BytesPerSector;
    //根据GetDiskFreeSpaceW得到的磁盘总的cluster，以此来计算磁盘的有效空间，有效空间可能会比分配分区的大小略小。
    _TotalNumberOfClusterInByte = (uint64_t)_ClusterUnitSize * (uint64_t)_TotalNumberOfCluster;

    //准备查询从首个cluster开始的bitmap描述信息
    stStartLCN.StartingLcn.QuadPart = 0;
    //下面计算整个分区的Bitmap有多大。如果分区比较大，通常cluster数量就多，需要的Bitmap空间就多一些。
    //Bitmap是按位描述，因此一个Byte可以描述8个Cluster的使用状态。按_TotalNumberOfCluster/8再+1可以计算出需要的字节数，+1是保证足够，因为除以8如果有余数（即最后不足8个Cluster）就需要一个字节来描述它。
    dwBitMapSize = offsetof(VOLUME_BITMAP_BUFFER, Buffer) + _TotalNumberOfCluster / 8 + 1;
    _pVolBitMapBuf = malloc(dwBitMapSize);
    if (!_pVolBitMapBuf)
        return FALSE;

    bResult = DeviceIoControl(m_hVol,
        FSCTL_GET_VOLUME_BITMAP,
        &stStartLCN,
        sizeof(stStartLCN),
        _pVolBitMapBuf,
        dwBitMapSize,
        &dwBitMapSize,
        NULL);

    if (bResult) {
        _BitMapBytes = dwBitMapSize - offsetof(VOLUME_BITMAP_BUFFER, Buffer);
    }

    return (bResult);
}

//按cluster序号查询该cluster后续连续相同状态的cluster的数量
int64_t CVolumeReader::GetContiguous(int64_t lcn, bool& stat)
{
    static BYTE BitShift[] = { 1, 2, 4, 8, 16, 32, 64, 128 }; //8个位掩码，方便从一个字节取出每个位

    VOLUME_BITMAP_BUFFER* bitMappings = (VOLUME_BITMAP_BUFFER*)_pVolBitMapBuf;
    int64_t i = lcn;
    bool v;
    if (i >= bitMappings->BitmapSize.QuadPart) {
        return 0;
    }
    stat = (bitMappings->Buffer[i / 8] & BitShift[i % 8]) ? 1 : 0;
    i += 1;
    for (; i < bitMappings->BitmapSize.QuadPart; i++) {

        v = (bitMappings->Buffer[i / 8] & BitShift[i % 8]) ? 1 : 0;
        if (v != stat)
            break;
    }
    return i - lcn;
}

BOOL CVolumeReader::ReadBootSector()
{
    DWORD cbR;
    BOOL bRes;
    LARGE_INTEGER liPos, liNewPos;
    bRes = ReadFile(m_hVol, _BootSector, sizeof(_BootSector), &cbR, NULL);
    if (!bRes) {
        return FALSE;
    }
    if (sizeof(_BootSector) != cbR) {
        SetLastError(ERROR_NO_MORE_ITEMS);
        return FALSE;
    }
    liPos.QuadPart = 0;
    if (!SetFilePointerEx(m_hVol, liPos, &liNewPos, FILE_BEGIN)) {
        return FALSE;
    }

    return TRUE;
}

BOOL CVolumeReader::ParseBootSector()
{
    //TODO
    uint64_t v1 = 0;
    int v3;
    v3 = *(uint16_t*)&_BootSector[0x16]; // Logical sectors per File Allocation Table??
    if (!*(uint16_t*)&_BootSector[0x16])
        v3 = *(uint32_t*)&_BootSector[0x24];
    if (*(uint16_t*)&_BootSector[0xB]) {
        v1 = *(uint16_t*)&_BootSector[0xB]   // sector size? 0x200(512)
            * (uint64_t)(*(uint16_t*)&_BootSector[0xE]// Count of reserved logical sectors
                + (32 * *(uint16_t*)&_BootSector[0x11] + *(uint16_t*)&_BootSector[0xB] - 1)// Maximum number of FAT12 or FAT16 root directory entries. 0 for FAT32
                / *(uint16_t*)&_BootSector[0xB]
                + v3 * (uint8_t)_BootSector[0x10]);// Number of File Allocation Tables. Almost always 2;
    }

    m_1stClusterOffset = v1;
    return TRUE;
}

BOOL CVolumeReader::GetFStype(int &type)
{
    DWORD cdR = 0;
    FILESYSTEM_STATISTICS fs_stat;
    if (DeviceIoControl(m_hVol, FSCTL_FILESYSTEM_GET_STATISTICS, NULL, 0, (void*)&fs_stat, sizeof(fs_stat), &cdR, NULL)
        || cdR >= sizeof(fs_stat))
    {
        type = fs_stat.FileSystemType;
        return TRUE;
    }
    return FALSE;
}


typedef LONG NTSTATUS;
typedef struct _IO_STATUS_BLOCK {
    union {
        NTSTATUS Status;
        PVOID    Pointer;
    };
    ULONG_PTR Information;
} IO_STATUS_BLOCK, * PIO_STATUS_BLOCK;
typedef
NTSTATUS WINAPI __NtFsControlFile(
    HANDLE           FileHandle,
    HANDLE           Event,
    void*            ApcRoutine,
    PVOID            ApcContext,
    PIO_STATUS_BLOCK IoStatusBlock,
    ULONG            FsControlCode,
    PVOID            InputBuffer,
    ULONG            InputBufferLength,
    PVOID            OutputBuffer,
    ULONG            OutputBufferLength
);

//把文件相关的cluster在Bitmap中置为0
uint64_t UnmaskBitMap(LPCWSTR lpszVolName, LPCWSTR lpszFilename, BYTE*pVBB)
{
    //TODO
    HANDLE hVolume = INVALID_HANDLE_VALUE; // r12
    RETRIEVAL_POINTERS_BUFFER* pRPB = NULL; // rsi
    NTSTATUS status; // eax v9 v10
    uint64_t sz = 0; // v18; // rbx
    HANDLE hFind = INVALID_HANDLE_VALUE;
    //ULONG FsControlCode; // [rsp+28h] [rbp-4E0h]
    ULONG bufOutSize; // [rsp+48h] [rbp-4C0h]
    LARGE_INTEGER startVcn; // [rsp+50h] [rbp-4B8h]
    DWORD v12;
    LARGE_INTEGER v13;
    DWORD v14;
    int64_t v15;
    uint64_t v16;
    IO_STATUS_BLOCK ioStatus; //int v23; // [rsp+58h] [rbp-4B0h]
    WIN32_FIND_DATAW FindFileData;
    std::wstring strFileName;

    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    __NtFsControlFile * pNtFsControlFile = (__NtFsControlFile*)GetProcAddress(hNtdll, "NtFsControlFile");
    strFileName = lpszVolName;
    if ((strFileName.size() && *strFileName.rbegin() != '\\') && (lpszFilename[0] != '\\'))
        strFileName += L"\\";
    strFileName += lpszFilename;

    hVolume = CreateFileW(strFileName.c_str(),
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ|FILE_SHARE_WRITE,
        0,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING|FILE_FLAG_BACKUP_SEMANTICS,
        NULL);
    if (hVolume != INVALID_HANDLE_VALUE)
    {
        startVcn.QuadPart = 0;
        pRPB = (RETRIEVAL_POINTERS_BUFFER*)malloc(0x20010);
        while (1)
        {
            pRPB->ExtentCount = 0;
            bufOutSize = 0x20010;
            status = pNtFsControlFile(
                hVolume,
                0,
                0,
                0,
                &ioStatus,
                FSCTL_GET_RETRIEVAL_POINTERS, //0x90073
                &startVcn,
                sizeof(startVcn),
                pRPB,
                bufOutSize);
            if (status)
            {
                if (status != 0xC0000011 && status != 0x80000005) //STATUS_BUFFER_OVERFLOW
                    break;
            }
            if (status == STATUS_PENDING)
            {
                WaitForSingleObject(hVolume, INFINITE);
                if (ioStatus.Status != 0 && ioStatus.Status != 0x80000005) // STATUS_SUCCESS, STATUS_BUFFER_OVERFLOW
                    goto _END;
            }
            v12 = 0;
            if (pRPB->ExtentCount)
            {
                v13 = startVcn;
                v14 = 0;
                do
                {
                    if (pRPB->Extents[v14].Lcn.QuadPart != -1)
                    {
                        v15 = 0;
                        if (pRPB->Extents[v14].NextVcn.QuadPart != v13.QuadPart)
                        {
                            do
                            {
                                v16 = v15++ + pRPB->Extents[v14].Lcn.QuadPart;
                                *(BYTE*)((v16 >> 3) + pVBB) &= ~(1 << (v16 & 7));
                            } while (v15 < pRPB->Extents[v14].NextVcn.QuadPart - v13.QuadPart);
                        }
                    }
                    v13 = pRPB->Extents[v14].NextVcn;
                    ++v12;
                    ++v14;
                    startVcn = v13;
                } while (v12 < pRPB->ExtentCount);
            }
            if (!status || status == 0xC0000011)
            {
                goto _END;
            }
            //0x80000005 STATUS_BUFFER_OVERFLOW的情况，将继续查询
        }
    }

    hFind = FindFirstFileW(strFileName.c_str(), &FindFileData);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        sz = FindFileData.nFileSizeLow + ((uint64_t)FindFileData.nFileSizeHigh << 32);
        FindClose(hFind);
    }
_END:
    free(pRPB);
    if (hVolume != INVALID_HANDLE_VALUE)
        CloseHandle(hVolume);
    return sz;
}


//把System Volume Information中的一些文件UnmaskBitMap
uint64_t UnmaskBitMapSVIFiles(LPCWSTR lpszVolName, BYTE* pVBB, LPCWSTR lpszNamePattern, const FILETIME *ignoreAfter)
{
    //TODO
    uint64_t total_size; // rbx
    HANDLE v5; // rdi
    WIN32_FIND_DATAW FindFileData; // [rsp+20h] [rbp-698h]
    std::wstring strVol;
    std::wstring strFound;

    strVol = lpszVolName;
    if (strVol.size() && *strVol.rbegin() != '\\')
        strVol += L"\\";

    total_size = 0i64;
    v5 = FindFirstFileW((strVol + L"System Volume Information\\*").c_str() , &FindFileData);
    if (v5 != INVALID_HANDLE_VALUE)
    {
        do
        {
            if ( ( lpszNamePattern  && PathMatchSpecW(FindFileData.cFileName, lpszNamePattern) )
                || (ignoreAfter && CompareFileTime(&FindFileData.ftCreationTime, ignoreAfter) > 0)
                )
            {
                strFound = L"System Volume Information\\";
                strFound += FindFileData.cFileName;
                total_size += UnmaskBitMap(lpszVolName, strFound.c_str(), pVBB);
            }
        } while (FindNextFileW(v5, &FindFileData));
        FindClose(v5);
    }
    return total_size;
}


uint64_t CVolumeReader::UnmaskSomeFiles()
{
    uint64_t totalsz = 0;
    VOLUME_BITMAP_BUFFER* bitMappings = (VOLUME_BITMAP_BUFFER*)_pVolBitMapBuf;
    m_calcedSize = 0;
    if (m_dwOptions & OptIgnorePagefile)
        totalsz += UnmaskBitMap(m_strVolName.c_str(), L"Pagefile.sys", bitMappings->Buffer);
    if (m_dwOptions & OptIgnoreHiberfil)
        totalsz += UnmaskBitMap(m_strVolName.c_str(), L"Hiberfil.sys", bitMappings->Buffer);
    if (m_dwOptions & OptIgnoreSVI)
        totalsz += UnmaskBitMapSVIFiles(m_strVolName.c_str(), bitMappings->Buffer, L"{*", is_snapshot_time_valid() ? &m_ft_beforeSnapshot : NULL);
    else if (is_snapshot_time_valid()) {
        totalsz += UnmaskBitMapSVIFiles(m_strVolName.c_str(), bitMappings->Buffer, NULL, &m_ft_beforeSnapshot);
    }
    return totalsz;
}

bool CVolumeReader::is_snapshot_time_valid() {
    return (m_ft_beforeSnapshot.dwHighDateTime || m_ft_beforeSnapshot.dwLowDateTime);
}