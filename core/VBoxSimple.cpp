#include "pch.h"
#include "VBoxSimple.h"
#include <string>
#include <shlwapi.h>
#include <assert.h>


#define VBOXDDU_DECL(type) type
typedef void* PVBOXHDD;

typedef enum VDTYPE
{
    /** Invalid. */
    VDTYPE_INVALID = 0,
    /** HardDisk */
    VDTYPE_HDD,
    /** CD/DVD */
    VDTYPE_DVD,
    /** Floppy. */
    VDTYPE_FLOPPY
} VDTYPE;
typedef
VBOXDDU_DECL(int) _VDCreate(void* pVDIfsDisk, int enmType, PVBOXHDD* ppDisk);
typedef
VBOXDDU_DECL(int) _VDWrite(PVBOXHDD pDisk, uint64_t uOffset, const void* pvBuffer, size_t cbBuffer);
typedef
VBOXDDU_DECL(int) _VDClose(PVBOXHDD pDisk, bool fDelete);

typedef struct VDGEOMETRY
{
    /** Number of cylinders. */
    uint32_t    cCylinders;
    /** Number of heads. */
    uint32_t    cHeads;
    /** Number of sectors. */
    uint32_t    cSectors;
} VDGEOMETRY;

/** Pointer to disk geometry. */
typedef VDGEOMETRY* PVDGEOMETRY;
/** Pointer to constant disk geometry. */
typedef const VDGEOMETRY* PCVDGEOMETRY;

typedef GUID* PCRTUUID;

typedef void* PVDINTERFACE;

typedef
VBOXDDU_DECL(int) _VDCreateBase(void* pDisk, const char* pszBackend,
    const char* pszFilename, uint64_t cbSize,
    unsigned uImageFlags, const char* pszComment,
    PCVDGEOMETRY pPCHSGeometry,
    PCVDGEOMETRY pLCHSGeometry,
    PCRTUUID pUuid, unsigned uOpenFlags,
    PVDINTERFACE pVDIfsImage,
    PVDINTERFACE pVDIfsOperation);


#define RT_MIN min

/** Current VMDK image version. */
#define VMDK_IMAGE_VERSION          (0x0001)
/** No flags. */
#define VD_IMAGE_FLAGS_NONE                     (0)
/** Fixed image. */
#define VD_IMAGE_FLAGS_FIXED                    (0x10000)
/** Diff image. Mutually exclusive with fixed image. */
#define VD_IMAGE_FLAGS_DIFF                     (0x20000)
/** VMDK: Split image into 2GB extents. */
#define VD_VMDK_IMAGE_FLAGS_SPLIT_2G            (0x0001)
/** VMDK: Raw disk image (giving access to a number of host partitions). */
#define VD_VMDK_IMAGE_FLAGS_RAWDISK             (0x0002)
/** VMDK: stream optimized image, read only. */
#define VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED    (0x0004)
/** VMDK: ESX variant, use in addition to other flags. */
#define VD_VMDK_IMAGE_FLAGS_ESX                 (0x0008)
/** VDI: Fill new blocks with zeroes while expanding image file. Only valid
 * for newly created images, never set for opened existing images. */
#define VD_VDI_IMAGE_FLAGS_ZERO_EXPAND          (0x0100)

 /** Mask of valid image flags for VMDK. */
#define VD_VMDK_IMAGE_FLAGS_MASK            (   VD_IMAGE_FLAGS_FIXED | VD_IMAGE_FLAGS_DIFF | VD_IMAGE_FLAGS_NONE \
                                             |  VD_VMDK_IMAGE_FLAGS_SPLIT_2G | VD_VMDK_IMAGE_FLAGS_RAWDISK \
                                             | VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED | VD_VMDK_IMAGE_FLAGS_ESX)

/** Mask of valid image flags for VDI. */
#define VD_VDI_IMAGE_FLAGS_MASK             (VD_IMAGE_FLAGS_FIXED | VD_IMAGE_FLAGS_DIFF | VD_IMAGE_FLAGS_NONE | VD_VDI_IMAGE_FLAGS_ZERO_EXPAND)

/** Mask of all valid image flags for all formats. */
#define VD_IMAGE_FLAGS_MASK                 (VD_VMDK_IMAGE_FLAGS_MASK | VD_VDI_IMAGE_FLAGS_MASK)

/** Default image flags. */
#define VD_IMAGE_FLAGS_DEFAULT              (VD_IMAGE_FLAGS_NONE)

#define VD_OPEN_FLAGS_NORMAL        0


static HMODULE hVBoxDDU = NULL;
static bool init_VBoxDDU = false;
static _VDCreate* VDCreate;
static _VDCreateBase* VDCreateBase;
static _VDWrite* VDWrite;
static _VDClose* VDClose;


std::wstring SearchVBoxLibDir()
{
    WCHAR szCurrDir[MAX_PATH];
    std::wstring path;
    std::wstring vbd;
    DWORD dwRes = GetModuleFileNameW(NULL, szCurrDir, MAX_PATH);
    *wcsrchr(szCurrDir, '\\') = '\0';

#ifdef _WIN64
    vbd = L"vbox64";
#else
    vbd = L"vbox32";
#endif

    path = std::wstring(szCurrDir) + L"\\" + vbd;
    if (PathFileExistsW(path.c_str()))
        return path;
#ifdef _DEBUG
    path = std::wstring(szCurrDir) + L"\\..\\" + vbd;
    if (PathFileExistsW(path.c_str()))
        return path;
    path = std::wstring(szCurrDir) + L"\\..\\..\\" + vbd;
    if (PathFileExistsW(path.c_str()))
        return path;
    path = std::wstring(szCurrDir) + L"\\..\\..\\..\\" + vbd;
    if (PathFileExistsW(path.c_str()))
        return path;
#endif
    return L"";
}

bool CVBoxSimple::InitLib()
{
    if (init_VBoxDDU)
        return true;
    std::wstring libdir = SearchVBoxLibDir();
    SetDllDirectoryW(libdir.c_str());
    hVBoxDDU = LoadLibraryW(L"VBoxDDU.dll");
    SetDllDirectoryW(NULL);

    if (!hVBoxDDU) {
        return false;
    }
    *(void**)&VDCreate = GetProcAddress(hVBoxDDU, "VDCreate");
    *(void**)&VDCreateBase = GetProcAddress(hVBoxDDU, "VDCreateBase");
    *(void**)&VDWrite = GetProcAddress(hVBoxDDU, "VDWrite");
    *(void**)&VDClose = GetProcAddress(hVBoxDDU, "VDClose");

    init_VBoxDDU = VDCreate && VDCreateBase && VDWrite && VDClose;
    return init_VBoxDDU;
}

CVBoxSimple::CVBoxSimple()
	: _pDisk(NULL)
{
}

CVBoxSimple::~CVBoxSimple()
{
	Close();
}

bool CVBoxSimple::CreateImage(const char* format, const char* dst_file, uint64_t cbFile)
{
    unsigned uImageFlags = VD_IMAGE_FLAGS_NONE;
    PCRTUUID pUuid = NULL;
    char pszComment[256] = "disk2vmdk";
    int rc;
    rc = VDCreate(NULL, VDTYPE_HDD, (void**)&_pDisk);
    if (rc < 0) {
        return false;
    }
    VDGEOMETRY PCHS, LCHS;
    PCHS.cCylinders = (unsigned int)RT_MIN(cbFile / 512 / 16 / 63, 16383);
    PCHS.cHeads = 16;
    PCHS.cSectors = 63;
    LCHS.cCylinders = 0;
    LCHS.cHeads = 0;
    LCHS.cSectors = 0;
    rc = VDCreateBase(_pDisk, format, dst_file, cbFile,
        uImageFlags, pszComment, &PCHS, &LCHS, pUuid,
        VD_OPEN_FLAGS_NORMAL, NULL, NULL);
    if (rc < 0) {
        return false;
    }

    return true;
}

bool CVBoxSimple::CreateVMDK(const char* dst_file, uint64_t cbFile)
{
    return CreateImage("VMDK", dst_file, cbFile);
}

bool CVBoxSimple::Write(uint64_t offFile, const void* pvBuf, size_t size)
{
#ifdef _DEBUG
    assert((offFile % 512) == 0);
#endif
    int rc = VDWrite(_pDisk, offFile, pvBuf, size);
    if (rc < 0) {
        return false;
    }
    return true;
}

bool CVBoxSimple::Close()
{
	int rc = 0;
	if (_pDisk) {
        rc = VDClose(_pDisk, false);
		_pDisk = NULL;
	}
	return rc >= 0;
}
