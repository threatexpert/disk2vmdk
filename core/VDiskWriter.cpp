//
// VDiskWriter.cpp ！ CVBoxSimple replacement
// High-performance sequential writer for VMDK, VHD, VDI dynamic disk images
//
// Design principle:
//   All data is appended sequentially.  Metadata is built in memory and
//   flushed once at Close().  Zero/sparse regions are simply not written,
//   keeping the output file small (dynamic sizing) while achieving near-raw
//   sequential write speed.
//
// VMDK: monolithicSparse ！ compatible with VMware, VirtualBox, QEMU
// VHD:  Dynamic           ！ compatible with Hyper-V, VirtualBox, QEMU
// VDI:  Dynamic           ！ compatible with VirtualBox
//

#include "pch.h"
#include "VDiskWriter.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <atlstr.h>   // for CA2W if needed

// ============================================================================
//  Helper: check if buffer is all zeros (SIMD-friendly for compiler)
// ============================================================================
static bool IsZeroBuf(const void* buf, size_t len)
{
    const uint64_t* p = (const uint64_t*)buf;
    size_t n8 = len / 8;
    for (size_t i = 0; i < n8; i++) {
        if (p[i]) return false;
    }
    const uint8_t* tail = (const uint8_t*)buf + n8 * 8;
    for (size_t i = 0; i < (len & 7); i++) {
        if (tail[i]) return false;
    }
    return true;
}

// ============================================================================
//  Constructor / Destructor
// ============================================================================

CVDiskWriter::CVDiskWriter()
    : m_hFile(INVALID_HANDLE_VALUE)
    , m_cbCapacity(0)
    , m_cbFilePos(0)
    , m_fmt(FMT_NONE)
    , m_vmdk_numGrains(0), m_vmdk_numGTs(0), m_vmdk_numGDEntries(0)
    , m_vmdk_gd(nullptr), m_vmdk_gt(nullptr)
    , m_vmdk_gdSectorOffset(0), m_vmdk_rgdSectorOffset(0)
    , m_vmdk_rgtSectorOffset(0), m_vmdk_gtSectorOffset(0)
    , m_vmdk_dataSectorOffset(0)
    , m_vmdk_nextGrainFileSector(0)
    , m_vmdk_writeBuf(nullptr), m_vmdk_writeBufUsed(0), m_vmdk_writeBufBaseGrain(0)
    , m_vmdk_grainBuf(nullptr), m_vmdk_grainBufUsed(0), m_vmdk_curGrain(0)
    , m_vhd_maxBATEntries(0), m_vhd_bat(nullptr)
    , m_vhd_dataStartOffset(0), m_vhd_nextBlockOffset(0)
    , m_vhd_blockBuf(nullptr), m_vhd_blockBufUsed(0), m_vhd_curBlock(0)
    , m_vdi_totalBlocks(0), m_vdi_allocatedBlocks(0), m_vdi_blockMap(nullptr)
    , m_vdi_blockMapOffset(0), m_vdi_dataOffset(0), m_vdi_nextBlockOffset(0)
    , m_vdi_blockBuf(nullptr), m_vdi_blockBufUsed(0), m_vdi_curBlock(0)
{
}

CVDiskWriter::~CVDiskWriter()
{
    Close();
    free(m_vmdk_gd);
    free(m_vmdk_gt);
    free(m_vmdk_writeBuf);
    free(m_vhd_bat);
    free(m_vhd_blockBuf);
    free(m_vdi_blockMap);
    free(m_vdi_blockBuf);
}

// ============================================================================
//  File I/O helpers
// ============================================================================

bool CVDiskWriter::FileWrite(const void* data, uint32_t len)
{
    const uint8_t* p = (const uint8_t*)data;
    while (len > 0) {
        DWORD cbW = 0;
        if (!WriteFile(m_hFile, p, len, &cbW, NULL)) {
            m_lasterr = L"WriteFile failed";
            return false;
        }
        p += cbW;
        len -= cbW;
        m_cbFilePos += cbW;
    }
    return true;
}

bool CVDiskWriter::FileWriteAt(uint64_t offset, const void* data, uint32_t len)
{
    if (!FileSetPos(offset)) return false;
    return FileWrite(data, len);
}

bool CVDiskWriter::FileSetPos(uint64_t pos)
{
    LARGE_INTEGER li;
    li.QuadPart = (LONGLONG)pos;
    if (!SetFilePointerEx(m_hFile, li, NULL, FILE_BEGIN)) {
        m_lasterr = L"SetFilePointerEx failed";
        return false;
    }
    m_cbFilePos = pos;
    return true;
}

bool CVDiskWriter::FilePad(uint64_t targetPos)
{
    if (m_cbFilePos >= targetPos) return true;
    uint8_t zeros[4096];
    memset(zeros, 0, sizeof(zeros));
    while (m_cbFilePos < targetPos) {
        uint32_t chunk = (uint32_t)min((uint64_t)sizeof(zeros), targetPos - m_cbFilePos);
        if (!FileWrite(zeros, chunk)) return false;
    }
    return true;
}

// ============================================================================
//  Public API: CreateImage
// ============================================================================

bool CVDiskWriter::CreateImage(const char* format, const char* dst_file, uint64_t cbCapacity)
{
    if (!_stricmp(format, "VMDK"))     m_fmt = FMT_VMDK;
    else if (!_stricmp(format, "VHD")) m_fmt = FMT_VHD;
    else if (!_stricmp(format, "VDI")) m_fmt = FMT_VDI;
    else {
        m_lasterr = L"Unsupported format";
        return false;
    }

    m_cbCapacity = cbCapacity;

    switch (m_fmt) {
    case FMT_VMDK: return vmdk_Create(dst_file, cbCapacity);
    case FMT_VHD:  return vhd_Create(dst_file, cbCapacity);
    case FMT_VDI:  return vdi_Create(dst_file, cbCapacity);
    default:       return false;
    }
}

// ============================================================================
//  Public API: Write ！ write data at logical disk offset
// ============================================================================

bool CVDiskWriter::Write(uint64_t offDisk, const void* pvBuf, size_t cbBuf)
{
    const uint8_t* src = (const uint8_t*)pvBuf;

    while (cbBuf > 0) {
        switch (m_fmt) {
        case FMT_VMDK: {
            uint32_t grain = (uint32_t)(offDisk / VMDK_GRAIN_SIZE);
            uint32_t offInGrain = (uint32_t)(offDisk % VMDK_GRAIN_SIZE);
            uint32_t canWrite = VMDK_GRAIN_SIZE - offInGrain;
            uint32_t toWrite = (uint32_t)min((size_t)canWrite, cbBuf);

            // New grain?
            if (grain != m_vmdk_curGrain || m_vmdk_grainBufUsed == 0) {
                // Flush current grain slot into write buffer
                if (m_vmdk_grainBufUsed > 0 && m_vmdk_curGrain != grain) {
                    // Pad current grain to full size
                    if (m_vmdk_grainBufUsed < VMDK_GRAIN_SIZE)
                        memset(m_vmdk_grainBuf + m_vmdk_grainBufUsed, 0, VMDK_GRAIN_SIZE - m_vmdk_grainBufUsed);

                    if (IsZeroBuf(m_vmdk_grainBuf, VMDK_GRAIN_SIZE)) {
                        // Zero grain: must flush any accumulated non-zero grains first
                        if (m_vmdk_writeBufUsed > 0) {
                            if (!vmdk_FlushWriteBuf()) return false;
                        }
                        // Skip this grain (leave GT entry = 0 = unallocated)
                    }
                    else {
                        // Non-zero grain: record metadata and advance write buffer
                        m_vmdk_gt[m_vmdk_curGrain] = (uint32_t)(m_vmdk_nextGrainFileSector +
                            m_vmdk_writeBufUsed / VMDK_SECTOR_SIZE);
                        m_vmdk_writeBufUsed += VMDK_GRAIN_SIZE;

                        // Write buffer full? Flush
                        if (m_vmdk_writeBufUsed >= VMDK_WRITE_BUF_SIZE) {
                            if (!vmdk_FlushWriteBuf()) return false;
                        }
                    }
                    m_vmdk_grainBufUsed = 0;
                }
                m_vmdk_curGrain = grain;
                // Point grainBuf to the next slot in the write buffer
                m_vmdk_grainBuf = m_vmdk_writeBuf + m_vmdk_writeBufUsed;
                if (m_vmdk_grainBufUsed == 0 && offInGrain > 0) {
                    memset(m_vmdk_grainBuf, 0, offInGrain);
                }
                m_vmdk_grainBufUsed = offInGrain;
            }

            memcpy(m_vmdk_grainBuf + offInGrain, src, toWrite);
            if (offInGrain + toWrite > m_vmdk_grainBufUsed)
                m_vmdk_grainBufUsed = offInGrain + toWrite;

            // Full grain? Commit to write buffer
            if (m_vmdk_grainBufUsed == VMDK_GRAIN_SIZE) {
                if (IsZeroBuf(m_vmdk_grainBuf, VMDK_GRAIN_SIZE)) {
                    // Zero grain: flush accumulated, skip this grain
                    if (m_vmdk_writeBufUsed > 0) {
                        if (!vmdk_FlushWriteBuf()) return false;
                    }
                }
                else {
                    // Record metadata
                    m_vmdk_gt[m_vmdk_curGrain] = (uint32_t)(m_vmdk_nextGrainFileSector +
                        m_vmdk_writeBufUsed / VMDK_SECTOR_SIZE);
                    m_vmdk_writeBufUsed += VMDK_GRAIN_SIZE;

                    if (m_vmdk_writeBufUsed >= VMDK_WRITE_BUF_SIZE) {
                        if (!vmdk_FlushWriteBuf()) return false;
                    }
                }
                m_vmdk_grainBufUsed = 0;
            }

            src += toWrite;
            offDisk += toWrite;
            cbBuf -= toWrite;
            break;
        }

        case FMT_VHD: {
            uint32_t block = (uint32_t)(offDisk / VHD_BLOCK_SIZE);
            uint32_t offInBlock = (uint32_t)(offDisk % VHD_BLOCK_SIZE);
            uint32_t canWrite = VHD_BLOCK_SIZE - offInBlock;
            uint32_t toWrite = (uint32_t)min((size_t)canWrite, cbBuf);

            if (block != m_vhd_curBlock || m_vhd_blockBufUsed == 0) {
                if (m_vhd_blockBufUsed > 0 && m_vhd_curBlock != block) {
                    if (m_vhd_blockBufUsed < VHD_BLOCK_SIZE)
                        memset(m_vhd_blockBuf + m_vhd_blockBufUsed, 0, VHD_BLOCK_SIZE - m_vhd_blockBufUsed);
                    if (!IsZeroBuf(m_vhd_blockBuf, VHD_BLOCK_SIZE)) {
                        if (!vhd_WriteBlock(m_vhd_curBlock, m_vhd_blockBuf, VHD_BLOCK_SIZE))
                            return false;
                    }
                    m_vhd_blockBufUsed = 0;
                }
                m_vhd_curBlock = block;
                if (m_vhd_blockBufUsed == 0 && offInBlock > 0) {
                    memset(m_vhd_blockBuf, 0, offInBlock);
                    m_vhd_blockBufUsed = offInBlock;
                }
            }

            memcpy(m_vhd_blockBuf + offInBlock, src, toWrite);
            if (offInBlock + toWrite > m_vhd_blockBufUsed)
                m_vhd_blockBufUsed = offInBlock + toWrite;

            if (m_vhd_blockBufUsed == VHD_BLOCK_SIZE) {
                if (!IsZeroBuf(m_vhd_blockBuf, VHD_BLOCK_SIZE)) {
                    if (!vhd_WriteBlock(m_vhd_curBlock, m_vhd_blockBuf, VHD_BLOCK_SIZE))
                        return false;
                }
                m_vhd_blockBufUsed = 0;
            }

            src += toWrite;
            offDisk += toWrite;
            cbBuf -= toWrite;
            break;
        }

        case FMT_VDI: {
            uint32_t block = (uint32_t)(offDisk / VDI_BLOCK_SIZE);
            uint32_t offInBlock = (uint32_t)(offDisk % VDI_BLOCK_SIZE);
            uint32_t canWrite = VDI_BLOCK_SIZE - offInBlock;
            uint32_t toWrite = (uint32_t)min((size_t)canWrite, cbBuf);

            if (block != m_vdi_curBlock || m_vdi_blockBufUsed == 0) {
                if (m_vdi_blockBufUsed > 0 && m_vdi_curBlock != block) {
                    if (m_vdi_blockBufUsed < VDI_BLOCK_SIZE)
                        memset(m_vdi_blockBuf + m_vdi_blockBufUsed, 0, VDI_BLOCK_SIZE - m_vdi_blockBufUsed);
                    if (!IsZeroBuf(m_vdi_blockBuf, VDI_BLOCK_SIZE)) {
                        if (!vdi_WriteBlock(m_vdi_curBlock, m_vdi_blockBuf, VDI_BLOCK_SIZE))
                            return false;
                    }
                    m_vdi_blockBufUsed = 0;
                }
                m_vdi_curBlock = block;
                if (m_vdi_blockBufUsed == 0 && offInBlock > 0) {
                    memset(m_vdi_blockBuf, 0, offInBlock);
                    m_vdi_blockBufUsed = offInBlock;
                }
            }

            memcpy(m_vdi_blockBuf + offInBlock, src, toWrite);
            if (offInBlock + toWrite > m_vdi_blockBufUsed)
                m_vdi_blockBufUsed = offInBlock + toWrite;

            if (m_vdi_blockBufUsed == VDI_BLOCK_SIZE) {
                if (!IsZeroBuf(m_vdi_blockBuf, VDI_BLOCK_SIZE)) {
                    if (!vdi_WriteBlock(m_vdi_curBlock, m_vdi_blockBuf, VDI_BLOCK_SIZE))
                        return false;
                }
                m_vdi_blockBufUsed = 0;
            }

            src += toWrite;
            offDisk += toWrite;
            cbBuf -= toWrite;
            break;
        }

        default:
            return false;
        }
    }
    return true;
}

// ============================================================================
//  Public API: WriteZero ！ mark a range as zero/sparse (skip writing)
// ============================================================================

bool CVDiskWriter::WriteZero(uint64_t offDisk, uint64_t cbLen)
{
    // Unified strategy for all formats (VMDK, VHD, VDI):
    //
    // Phase 1: Unaligned head ！ feed zeros via Write() so that any partial
    //          grain/block containing both data and zeros is correctly written.
    // Phase 2: Whole aligned units ！ flush pending buffer, then skip directly.
    //          Metadata entries stay 0 (unallocated = reads as zero). No IO.
    // Phase 3: Unaligned tail ！ feed zeros via Write() for the same reason as Phase 1.
    //
    // This is both correct (mixed grains/blocks handled) and fast (700GB of
    // aligned zeros skip instantly with no IO, no IsZeroBuf, no memset).

    uint32_t unitSize;
    switch (m_fmt) {
    case FMT_VMDK: unitSize = VMDK_GRAIN_SIZE; break;
    case FMT_VHD:  unitSize = VHD_BLOCK_SIZE;  break;
    case FMT_VDI:  unitSize = VDI_BLOCK_SIZE;  break;
    default:       return true;
    }

    // Phase 1: fill unaligned head via Write()
    uint32_t offInUnit = (uint32_t)(offDisk % unitSize);
    if (offInUnit > 0 && cbLen > 0) {
        uint64_t headLen = min(cbLen, (uint64_t)(unitSize - offInUnit));
        uint8_t zeros[65536];
        memset(zeros, 0, sizeof(zeros));
        while (headLen > 0) {
            size_t chunk = (size_t)min(headLen, (uint64_t)sizeof(zeros));
            if (!Write(offDisk, zeros, chunk)) return false;
            offDisk += chunk;
            cbLen -= chunk;
            headLen -= chunk;
        }
    }

    // Phase 2: skip whole aligned units (flush pending data first)
    uint64_t wholeUnits = cbLen / unitSize;
    if (wholeUnits > 0) {
        // Flush any pending partial buffer before skipping
        switch (m_fmt) {
        case FMT_VMDK:
            if (m_vmdk_grainBufUsed > 0) {
                memset(m_vmdk_grainBuf + m_vmdk_grainBufUsed, 0, VMDK_GRAIN_SIZE - m_vmdk_grainBufUsed);
                if (!IsZeroBuf(m_vmdk_grainBuf, VMDK_GRAIN_SIZE)) {
                    m_vmdk_gt[m_vmdk_curGrain] = (uint32_t)(m_vmdk_nextGrainFileSector +
                        m_vmdk_writeBufUsed / VMDK_SECTOR_SIZE);
                    m_vmdk_writeBufUsed += VMDK_GRAIN_SIZE;
                }
                m_vmdk_grainBufUsed = 0;
            }
            if (m_vmdk_writeBufUsed > 0) {
                if (!vmdk_FlushWriteBuf()) return false;
            }
            break;
        case FMT_VHD:
            if (m_vhd_blockBufUsed > 0) {
                memset(m_vhd_blockBuf + m_vhd_blockBufUsed, 0, VHD_BLOCK_SIZE - m_vhd_blockBufUsed);
                if (!IsZeroBuf(m_vhd_blockBuf, VHD_BLOCK_SIZE)) {
                    if (!vhd_WriteBlock(m_vhd_curBlock, m_vhd_blockBuf, VHD_BLOCK_SIZE)) return false;
                }
                m_vhd_blockBufUsed = 0;
            }
            break;
        case FMT_VDI:
            if (m_vdi_blockBufUsed > 0) {
                memset(m_vdi_blockBuf + m_vdi_blockBufUsed, 0, VDI_BLOCK_SIZE - m_vdi_blockBufUsed);
                if (!IsZeroBuf(m_vdi_blockBuf, VDI_BLOCK_SIZE)) {
                    if (!vdi_WriteBlock(m_vdi_curBlock, m_vdi_blockBuf, VDI_BLOCK_SIZE)) return false;
                }
                m_vdi_blockBufUsed = 0;
            }
            break;
        default: break;
        }

        // Skip ！ metadata entries stay 0 = unallocated = zero on read
        uint64_t skipBytes = wholeUnits * unitSize;
        offDisk += skipBytes;
        cbLen -= skipBytes;
    }

    // Phase 3: fill unaligned tail via Write()
    if (cbLen > 0) {
        uint8_t zeros[65536];
        memset(zeros, 0, sizeof(zeros));
        while (cbLen > 0) {
            size_t chunk = (size_t)min(cbLen, (uint64_t)sizeof(zeros));
            if (!Write(offDisk, zeros, chunk)) return false;
            offDisk += chunk;
            cbLen -= chunk;
        }
    }

    return true;
}

// ============================================================================
//  Public API: Close
// ============================================================================

bool CVDiskWriter::Close()
{
    if (m_hFile == INVALID_HANDLE_VALUE) return true;

    bool ok = true;
    switch (m_fmt) {
    case FMT_VMDK: ok = vmdk_Close(); break;
    case FMT_VHD:  ok = vhd_Close();  break;
    case FMT_VDI:  ok = vdi_Close();  break;
    default: break;
    }

    CloseHandle(m_hFile);
    m_hFile = INVALID_HANDLE_VALUE;
    return ok;
}


// ############################################################################
//
//   VMDK ！ monolithicSparse
//
// ############################################################################

#pragma pack(push, 1)
typedef struct {
    uint32_t magicNumber;       // 0x564D444B = "KDMV"
    uint32_t version;           // 1
    uint32_t flags;             // 3 = newLineTest | redundantGrainTable
    uint64_t capacity;          // in sectors
    uint64_t grainSize;         // in sectors (128 = 64KB)
    uint64_t descriptorOffset;  // sector offset
    uint64_t descriptorSize;    // in sectors
    uint32_t numGTEsPerGT;     // 512
    uint64_t rgdOffset;        // redundant grain directory offset (0 = none)
    uint64_t gdOffset;         // grain directory offset in sectors
    uint64_t overHead;         // sectors before grain data starts
    uint8_t  uncleanShutdown;  // 0
    char     singleEndLineChar;   // '\n'
    char     nonEndLineChar;      // ' '
    char     doubleEndLineChar1;  // '\r'
    char     doubleEndLineChar2;  // '\n'
    uint16_t compressAlgorithm;   // 0 = none
    uint8_t  pad[433];
} VMDK_SparseExtentHeader;
#pragma pack(pop)

bool CVDiskWriter::vmdk_Create(const char* dst_file, uint64_t cbCapacity)
{
    // Open output file
    m_hFile = CreateFileA(dst_file,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ, NULL,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        NULL);
    if (m_hFile == INVALID_HANDLE_VALUE) {
        m_lasterr = L"Cannot create VMDK file";
        return false;
    }

    uint64_t capacitySectors = cbCapacity / VMDK_SECTOR_SIZE;

    // Align capacity up to grain boundary (VMware requires this)
    if (capacitySectors % VMDK_GRAIN_SIZE_SECTORS) {
        capacitySectors = ((capacitySectors + VMDK_GRAIN_SIZE_SECTORS - 1) / VMDK_GRAIN_SIZE_SECTORS) * VMDK_GRAIN_SIZE_SECTORS;
    }

    // Calculate metadata sizes
    m_vmdk_numGrains = (uint32_t)((capacitySectors + VMDK_GRAIN_SIZE_SECTORS - 1) / VMDK_GRAIN_SIZE_SECTORS);
    m_vmdk_numGTs = (m_vmdk_numGrains + VMDK_GT_ENTRIES - 1) / VMDK_GT_ENTRIES;
    m_vmdk_numGDEntries = m_vmdk_numGTs;

    // Allocate in-memory metadata
    m_vmdk_gd = (uint32_t*)calloc(m_vmdk_numGDEntries, sizeof(uint32_t));
    m_vmdk_gt = (uint32_t*)calloc((size_t)m_vmdk_numGTs * VMDK_GT_ENTRIES, sizeof(uint32_t));
    m_vmdk_writeBuf = (uint8_t*)malloc(VMDK_WRITE_BUF_SIZE);
    if (!m_vmdk_gd || !m_vmdk_gt || !m_vmdk_writeBuf) {
        m_lasterr = L"Memory allocation failed";
        return false;
    }
    m_vmdk_writeBufUsed = 0;
    m_vmdk_writeBufBaseGrain = 0;
    m_vmdk_grainBuf = m_vmdk_writeBuf; // points into write buffer
    m_vmdk_grainBufUsed = 0;
    m_vmdk_curGrain = 0;

    // --- Layout (matches VBox/VMware convention) ---
    // Sector 0:         SparseExtentHeader (1 sector)
    // Sector 1..20:     Descriptor (20 sectors)
    // After desc:       Redundant GD  (rgd)
    // After rgd:        Redundant GTs (rgts)
    // After rgts:       Primary GD    (gd)
    // After gd:         Primary GTs   (gts)
    // After gts(aligned): Grain data

    uint64_t descriptorOffsetSec = 1;
    uint64_t descriptorSizeSec = 20;

    uint64_t gdSizeBytes = (uint64_t)m_vmdk_numGDEntries * 4;
    uint64_t gdSizeSec = (gdSizeBytes + VMDK_SECTOR_SIZE - 1) / VMDK_SECTOR_SIZE;
    uint64_t gtSizeBytes = (uint64_t)m_vmdk_numGTs * VMDK_GT_SIZE_BYTES;
    uint64_t gtSizeSec = (gtSizeBytes + VMDK_SECTOR_SIZE - 1) / VMDK_SECTOR_SIZE;

    // Redundant GD+GTs
    uint64_t rgdStartSec = descriptorOffsetSec + descriptorSizeSec;  // right after descriptor
    uint64_t rgtStartSec = rgdStartSec + gdSizeSec;
    // Primary GD+GTs
    uint64_t gdStartSec = rgtStartSec + gtSizeSec;
    uint64_t gtStartSec = gdStartSec + gdSizeSec;

    uint64_t overHeadSec = gtStartSec + gtSizeSec;
    // Align to grain boundary for data start
    overHeadSec = ((overHeadSec + VMDK_GRAIN_SIZE_SECTORS - 1) / VMDK_GRAIN_SIZE_SECTORS) * VMDK_GRAIN_SIZE_SECTORS;

    m_vmdk_rgdSectorOffset = rgdStartSec;
    m_vmdk_rgtSectorOffset = rgtStartSec;
    m_vmdk_gdSectorOffset = gdStartSec;
    m_vmdk_gtSectorOffset = gtStartSec;
    m_vmdk_dataSectorOffset = overHeadSec;
    m_vmdk_nextGrainFileSector = overHeadSec;

    // Populate GD entries: each GD entry points to the sector of its grain table
    // Both redundant and primary GD point to their respective GT regions
    for (uint32_t i = 0; i < m_vmdk_numGTs; i++) {
        m_vmdk_gd[i] = (uint32_t)(gtStartSec + (uint64_t)i * VMDK_GT_SIZE_BYTES / VMDK_SECTOR_SIZE);
    }

    // --- Write header ---
    VMDK_SparseExtentHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magicNumber = 0x564D444B; // "KDMV"
    hdr.version = 1;
    hdr.flags = 3; // bit0=newLineTest + bit1=redundantGrainTable (required by VMware)
    hdr.capacity = capacitySectors;
    hdr.grainSize = VMDK_GRAIN_SIZE_SECTORS;
    hdr.descriptorOffset = descriptorOffsetSec;
    hdr.descriptorSize = descriptorSizeSec;
    hdr.numGTEsPerGT = VMDK_GT_ENTRIES;
    hdr.rgdOffset = rgdStartSec;   // redundant grain directory
    hdr.gdOffset = gdStartSec;     // primary grain directory
    hdr.overHead = overHeadSec;
    hdr.uncleanShutdown = 0;
    hdr.singleEndLineChar = '\n';
    hdr.nonEndLineChar = ' ';
    hdr.doubleEndLineChar1 = '\r';
    hdr.doubleEndLineChar2 = '\n';
    hdr.compressAlgorithm = 0;

    m_cbFilePos = 0;
    if (!FileWrite(&hdr, sizeof(hdr))) return false;

    // --- Write descriptor ---
    char desc[10240];
    memset(desc, 0, sizeof(desc));

    // CHS geometry
    uint32_t cyl = (uint32_t)min(capacitySectors / (16 * 63), (uint64_t)16383);
    if (cyl == 0) cyl = 1;

    // Extract just filename from path for extent description
    const char* pName = strrchr(dst_file, '\\');
    if (!pName) pName = strrchr(dst_file, '/');
    if (pName) pName++; else pName = dst_file;

    // Generate random CID and UUID
    LARGE_INTEGER pc;
    QueryPerformanceCounter(&pc);
    uint32_t cid = (uint32_t)(pc.QuadPart ^ GetTickCount());
    // Format UUID from random data
    uint32_t u1 = cid ^ 0x578fd28a;
    uint16_t u2 = (uint16_t)(pc.QuadPart >> 16);
    uint16_t u3 = (uint16_t)(pc.QuadPart >> 32) | 0x4000; // version 4
    uint16_t u4 = (uint16_t)(pc.QuadPart >> 48) | 0x8000; // variant 1
    uint32_t u5a = (uint32_t)GetCurrentProcessId();
    uint16_t u5b = (uint16_t)(pc.QuadPart);

    int descLen = sprintf_s(desc, sizeof(desc),
        "# Disk DescriptorFile\n"
        "version=1\n"
        "CID=%08x\n"
        "parentCID=ffffffff\n"
        "createType=\"monolithicSparse\"\n"
        "\n"
        "# Extent description\n"
        "RW %llu SPARSE \"%s\"\n"
        "\n"
        "# The Disk Data Base\n"
        "#DDB\n"
        "\n"
        "ddb.adapterType = \"ide\"\n"
        "ddb.encoding = \"GBK\"\n"
        "ddb.geometry.cylinders = \"%u\"\n"
        "ddb.geometry.heads = \"16\"\n"
        "ddb.geometry.sectors = \"63\"\n"
        "ddb.uuid.image = \"%08x-%04x-%04x-%04x-%08x%04x\"\n"
        "ddb.uuid.modification = \"00000000-0000-0000-0000-000000000000\"\n"
        "ddb.uuid.parent = \"00000000-0000-0000-0000-000000000000\"\n"
        "ddb.uuid.parentmodification = \"00000000-0000-0000-0000-000000000000\"\n"
        "ddb.virtualHWVersion = \"4\"\n"
        "ddb.comment = \"disk2vmdk\"\n",
        cid,
        (unsigned long long)capacitySectors,
        pName,
        cyl,
        u1, u2, u3, u4, u5a, u5b);

    // pad descriptor to fill descriptor sectors (pad with \0, not \n)
    uint32_t descPadSize = (uint32_t)(descriptorSizeSec * VMDK_SECTOR_SIZE);
    // desc was memset to 0, so bytes beyond descLen are already \0

    if (!FileWrite(desc, descPadSize)) return false;

    // --- Write Redundant GD placeholder (zeros, will be rewritten at Close) ---
    if (!FilePad(m_cbFilePos + (uint32_t)(gdSizeSec * VMDK_SECTOR_SIZE))) return false;

    // --- Write Redundant GTs placeholder (zeros) ---
    if (!FilePad(m_cbFilePos + (uint32_t)(gtSizeSec * VMDK_SECTOR_SIZE))) return false;

    // --- Write Primary GD placeholder (zeros, will be rewritten at Close) ---
    if (!FilePad(m_cbFilePos + (uint32_t)(gdSizeSec * VMDK_SECTOR_SIZE))) return false;

    // --- Write Primary GTs placeholder (zeros) ---
    if (!FilePad(m_cbFilePos + (uint32_t)(gtSizeSec * VMDK_SECTOR_SIZE))) return false;

    // --- Pad to data start ---
    if (!FilePad(overHeadSec * VMDK_SECTOR_SIZE)) return false;

    return true;
}

bool CVDiskWriter::vmdk_FlushWriteBuf()
{
    if (m_vmdk_writeBufUsed == 0) return true;

    // Write all accumulated grains in one IO call
    if (!FileWrite(m_vmdk_writeBuf, m_vmdk_writeBufUsed)) return false;

    m_vmdk_nextGrainFileSector += m_vmdk_writeBufUsed / VMDK_SECTOR_SIZE;
    m_vmdk_writeBufUsed = 0;
    m_vmdk_grainBuf = m_vmdk_writeBuf;  // reset pointer to start of buffer
    return true;
}

bool CVDiskWriter::vmdk_Close()
{
    // Flush any pending partial grain
    if (m_vmdk_grainBufUsed > 0) {
        memset(m_vmdk_grainBuf + m_vmdk_grainBufUsed, 0, VMDK_GRAIN_SIZE - m_vmdk_grainBufUsed);
        if (!IsZeroBuf(m_vmdk_grainBuf, VMDK_GRAIN_SIZE)) {
            m_vmdk_gt[m_vmdk_curGrain] = (uint32_t)(m_vmdk_nextGrainFileSector +
                m_vmdk_writeBufUsed / VMDK_SECTOR_SIZE);
            m_vmdk_writeBufUsed += VMDK_GRAIN_SIZE;
        }
        m_vmdk_grainBufUsed = 0;
    }

    // Flush remaining write buffer
    if (m_vmdk_writeBufUsed > 0) {
        if (!vmdk_FlushWriteBuf()) return false;
    }

    // Build GD arrays: redundant GD points to redundant GTs, primary GD points to primary GTs
    // m_vmdk_gd currently points to primary GTs (set in vmdk_Create).
    // We also need a redundant GD pointing to redundant GTs.
    uint32_t* rgd = (uint32_t*)calloc(m_vmdk_numGDEntries, sizeof(uint32_t));
    if (!rgd) { m_lasterr = L"Memory allocation failed"; return false; }
    for (uint32_t i = 0; i < m_vmdk_numGTs; i++) {
        rgd[i] = (uint32_t)(m_vmdk_rgtSectorOffset + (uint64_t)i * VMDK_GT_SIZE_BYTES / VMDK_SECTOR_SIZE);
    }

    // --- Rewrite Redundant Grain Directory ---
    if (!FileWriteAt(m_vmdk_rgdSectorOffset * VMDK_SECTOR_SIZE,
                     rgd, m_vmdk_numGDEntries * 4))
    { free(rgd); return false; }

    // --- Rewrite Redundant Grain Tables (same content as primary) ---
    for (uint32_t i = 0; i < m_vmdk_numGTs; i++) {
        uint64_t rgtFileOffset = (uint64_t)rgd[i] * VMDK_SECTOR_SIZE;
        uint32_t* gtData = m_vmdk_gt + (size_t)i * VMDK_GT_ENTRIES;
        if (!FileWriteAt(rgtFileOffset, gtData, VMDK_GT_SIZE_BYTES))
        { free(rgd); return false; }
    }
    free(rgd);

    // --- Rewrite Primary Grain Directory ---
    if (!FileWriteAt(m_vmdk_gdSectorOffset * VMDK_SECTOR_SIZE,
                     m_vmdk_gd, m_vmdk_numGDEntries * 4))
        return false;

    // --- Rewrite Primary Grain Tables ---
    for (uint32_t i = 0; i < m_vmdk_numGTs; i++) {
        uint64_t gtFileOffset = (uint64_t)m_vmdk_gd[i] * VMDK_SECTOR_SIZE;
        uint32_t* gtData = m_vmdk_gt + (size_t)i * VMDK_GT_ENTRIES;
        if (!FileWriteAt(gtFileOffset, gtData, VMDK_GT_SIZE_BYTES))
            return false;
    }

    // Mark clean shutdown in header
    uint8_t clean = 0;
    if (!FileWriteAt(offsetof(VMDK_SparseExtentHeader, uncleanShutdown), &clean, 1))
        return false;

    return true;
}


// ############################################################################
//
//   VHD ！ Dynamic
//
// ############################################################################

#pragma pack(push, 1)
typedef struct {
    char     cookie[8];         // "conectix"
    uint32_t features;          // 0x00000002 (reserved, always set)
    uint32_t fileFormatVersion; // 0x00010000
    uint64_t dataOffset;        // offset to dynamic disk header
    uint32_t timeStamp;         // seconds since 2000-01-01 00:00:00 UTC
    char     creatorApp[4];     // "d2v "
    uint32_t creatorVersion;    // 0x00010000
    uint32_t creatorHostOS;     // 0x5769326B "Wi2k" (Windows)
    uint64_t originalSize;      // in bytes (big-endian)
    uint64_t currentSize;       // in bytes (big-endian)
    uint16_t cylinders;
    uint8_t  heads;
    uint8_t  sectorsPerTrack;
    uint32_t diskType;          // 3 = Dynamic
    uint32_t checksum;          // one's complement of sum of all bytes (excl. checksum)
    uint8_t  uniqueId[16];      // UUID
    uint8_t  savedState;
    uint8_t  reserved[427];
} VHD_Footer;

typedef struct {
    char     cookie[8];         // "cxsparse"
    uint64_t dataOffset;        // 0xFFFFFFFFFFFFFFFF (unused)
    uint64_t tableOffset;       // absolute offset to BAT
    uint32_t headerVersion;     // 0x00010000
    uint32_t maxTableEntries;   // number of blocks
    uint32_t blockSize;         // 2MB = 0x00200000
    uint32_t checksum;
    uint8_t  parentUniqueId[16];
    uint32_t parentTimeStamp;
    uint32_t reserved1;
    uint8_t  parentUnicodeName[512];
    uint8_t  parentLocator[8][24]; // 8 parent locator entries
    uint8_t  reserved2[256];
} VHD_DynDiskHeader;
#pragma pack(pop)

static uint64_t bswap64(uint64_t v) {
    return ((v & 0xFF) << 56) | ((v & 0xFF00) << 40) |
           ((v & 0xFF0000) << 24) | ((v & 0xFF000000ULL) << 8) |
           ((v >> 8) & 0xFF000000ULL) | ((v >> 24) & 0xFF0000) |
           ((v >> 40) & 0xFF00) | ((v >> 56) & 0xFF);
}
static uint32_t bswap32(uint32_t v) {
    return ((v >> 24) & 0xFF) | ((v >> 8) & 0xFF00) |
           ((v << 8) & 0xFF0000) | ((v << 24) & 0xFF000000);
}
static uint16_t bswap16(uint16_t v) {
    return (v >> 8) | (v << 8);
}

uint32_t CVDiskWriter::vhd_Checksum(const uint8_t* buf, uint32_t len)
{
    uint32_t sum = 0;
    for (uint32_t i = 0; i < len; i++)
        sum += buf[i];
    return ~sum;
}

// CHS geometry calculation per VHD spec (Microsoft)
static void vhd_CalcGeometry(uint64_t totalSectors, uint16_t* cyl, uint8_t* heads, uint8_t* spt)
{
    uint32_t ts = (uint32_t)min(totalSectors, (uint64_t)65535 * 16 * 255);
    uint32_t cth;

    if (ts >= 65535 * 16 * 63) {
        *spt = 255;
        *heads = 16;
        cth = ts / *spt;
    } else {
        *spt = 17;
        cth = ts / *spt;
        *heads = (uint8_t)((cth + 1023) / 1024);
        if (*heads < 4) *heads = 4;
        if (cth >= (*heads * 1024) || *heads > 16) {
            *spt = 31;
            *heads = 16;
            cth = ts / *spt;
        }
        if (cth >= (*heads * 1024)) {
            *spt = 63;
            *heads = 16;
            cth = ts / *spt;
        }
    }
    *cyl = (uint16_t)(cth / *heads);
    if (*cyl > 65535) *cyl = 65535;
}

bool CVDiskWriter::vhd_Create(const char* dst_file, uint64_t cbCapacity)
{
    m_hFile = CreateFileA(dst_file,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ, NULL,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        NULL);
    if (m_hFile == INVALID_HANDLE_VALUE) {
        m_lasterr = L"Cannot create VHD file";
        return false;
    }

    // Calculate BAT size
    m_vhd_maxBATEntries = (uint32_t)((cbCapacity + VHD_BLOCK_SIZE - 1) / VHD_BLOCK_SIZE);
    m_vhd_bat = (uint32_t*)malloc((size_t)m_vhd_maxBATEntries * 4);
    m_vhd_blockBuf = (uint8_t*)malloc(VHD_BLOCK_SIZE);
    if (!m_vhd_bat || !m_vhd_blockBuf) {
        m_lasterr = L"Memory allocation failed";
        return false;
    }
    // Initialize BAT: 0xFFFFFFFF = unallocated
    memset(m_vhd_bat, 0xFF, (size_t)m_vhd_maxBATEntries * 4);
    m_vhd_blockBufUsed = 0;
    m_vhd_curBlock = 0;

    // --- Layout ---
    // Offset 0:          Footer copy (512 bytes)
    // Offset 512:        Dynamic Disk Header (1024 bytes)
    // Offset 1536:       BAT (maxBATEntries * 4, rounded to 512)
    // After BAT:         Data Blocks (each: bitmap + 2MB data)
    // At end:            Footer (512 bytes)

    uint64_t batOffset = 512 + 1024;
    uint64_t batSize = (uint64_t)m_vhd_maxBATEntries * 4;
    uint64_t batSizePadded = ((batSize + VHD_SECTOR_SIZE - 1) / VHD_SECTOR_SIZE) * VHD_SECTOR_SIZE;

    m_vhd_dataStartOffset = batOffset + batSizePadded;
    m_vhd_nextBlockOffset = m_vhd_dataStartOffset;

    // --- Build & write Footer ---
    VHD_Footer footer;
    memset(&footer, 0, sizeof(footer));
    memcpy(footer.cookie, "conectix", 8);
    footer.features = bswap32(0x00000002);
    footer.fileFormatVersion = bswap32(0x00010000);
    footer.dataOffset = bswap64(512); // offset to dynamic header
    // Timestamp: seconds since 2000-01-01. Use 0 for simplicity.
    footer.timeStamp = 0;
    memcpy(footer.creatorApp, "d2v ", 4);
    footer.creatorVersion = bswap32(0x00010000);
    footer.creatorHostOS = bswap32(0x5769326B); // "Wi2k"
    footer.originalSize = bswap64(cbCapacity);
    footer.currentSize = bswap64(cbCapacity);

    uint64_t totalSectors = cbCapacity / VHD_SECTOR_SIZE;
    uint16_t cyl; uint8_t heads, spt;
    vhd_CalcGeometry(totalSectors, &cyl, &heads, &spt);
    footer.cylinders = bswap16(cyl);
    footer.heads = heads;
    footer.sectorsPerTrack = spt;
    footer.diskType = bswap32(3); // Dynamic

    // Generate a simple UUID from timestamp + capacity
    uint64_t uid1 = GetTickCount64();
    uint64_t uid2 = cbCapacity ^ 0xDEADBEEF12345678ULL;
    memcpy(footer.uniqueId, &uid1, 8);
    memcpy(footer.uniqueId + 8, &uid2, 8);

    footer.savedState = 0;

    // Calculate checksum (all bytes except the checksum field itself)
    footer.checksum = 0;
    footer.checksum = bswap32(vhd_Checksum((uint8_t*)&footer, sizeof(footer)));

    m_cbFilePos = 0;
    if (!FileWrite(&footer, sizeof(footer))) return false;

    // --- Build & write Dynamic Disk Header ---
    VHD_DynDiskHeader dynHdr;
    memset(&dynHdr, 0, sizeof(dynHdr));
    memcpy(dynHdr.cookie, "cxsparse", 8);
    dynHdr.dataOffset = bswap64(0xFFFFFFFFFFFFFFFFULL);
    dynHdr.tableOffset = bswap64(batOffset);
    dynHdr.headerVersion = bswap32(0x00010000);
    dynHdr.maxTableEntries = bswap32(m_vhd_maxBATEntries);
    dynHdr.blockSize = bswap32(VHD_BLOCK_SIZE);

    dynHdr.checksum = 0;
    dynHdr.checksum = bswap32(vhd_Checksum((uint8_t*)&dynHdr, sizeof(dynHdr)));

    if (!FileWrite(&dynHdr, sizeof(dynHdr))) return false;

    // --- Write BAT placeholder (all 0xFF = unallocated) ---
    // BAT entries are big-endian sector offsets
    if (!FileWrite(m_vhd_bat, (uint32_t)batSize)) return false;
    // Pad BAT to sector boundary
    if (batSize < batSizePadded)
        if (!FilePad(batOffset + batSizePadded)) return false;

    return true;
}

bool CVDiskWriter::vhd_WriteBlock(uint32_t blockIndex, const void* data, uint32_t len)
{
    if (blockIndex >= m_vhd_maxBATEntries) {
        m_lasterr = L"VHD block index out of range";
        return false;
    }

    // Record BAT entry: sector offset of this block (big-endian)
    uint32_t sectorOffset = (uint32_t)(m_vhd_nextBlockOffset / VHD_SECTOR_SIZE);
    m_vhd_bat[blockIndex] = bswap32(sectorOffset);

    // Write sector bitmap (all 1s = all sectors present)
    uint8_t bitmap[VHD_BITMAP_SIZE];
    memset(bitmap, 0xFF, VHD_BITMAP_SIZE);
    if (!FileSetPos(m_vhd_nextBlockOffset)) return false;
    if (!FileWrite(bitmap, VHD_BITMAP_SIZE)) return false;

    // Write block data
    if (!FileWrite(data, len)) return false;

    m_vhd_nextBlockOffset = m_cbFilePos;
    return true;
}

bool CVDiskWriter::vhd_Close()
{
    // Flush pending partial block
    if (m_vhd_blockBufUsed > 0) {
        if (m_vhd_blockBufUsed < VHD_BLOCK_SIZE)
            memset(m_vhd_blockBuf + m_vhd_blockBufUsed, 0, VHD_BLOCK_SIZE - m_vhd_blockBufUsed);
        if (!IsZeroBuf(m_vhd_blockBuf, VHD_BLOCK_SIZE)) {
            if (!vhd_WriteBlock(m_vhd_curBlock, m_vhd_blockBuf, VHD_BLOCK_SIZE))
                return false;
        }
        m_vhd_blockBufUsed = 0;
    }

    // --- Rewrite BAT with actual offsets ---
    uint64_t batOffset = 512 + 1024;
    if (!FileWriteAt(batOffset, m_vhd_bat, m_vhd_maxBATEntries * 4))
        return false;

    // --- Write trailing Footer ---
    // Re-read the footer from offset 0 and append at end
    VHD_Footer footer;
    if (!FileSetPos(0)) return false;
    DWORD cbR;
    if (!ReadFile(m_hFile, &footer, sizeof(footer), &cbR, NULL) || cbR != sizeof(footer)) {
        m_lasterr = L"Failed to re-read VHD footer";
        return false;
    }

    // Seek to end and write footer copy
    if (!FileSetPos(m_vhd_nextBlockOffset)) return false;
    if (!FileWrite(&footer, sizeof(footer))) return false;

    return true;
}


// ############################################################################
//
//   VDI ！ Dynamic (VirtualBox native format)
//
// ############################################################################

#pragma pack(push, 1)
// VDI pre-header + header combined
typedef struct {
    // Pre-header (64 bytes)
    char     szFileInfo[64];    // "<<< Oracle VM VirtualBox Disk Image >>>\n"

    // Header signature + version
    uint32_t u32Signature;      // 0xBEDA107F
    uint32_t u32Version;        // 0x00010001 (1.1)

    // Header body
    uint32_t cbHeader;          // size of header body (400)
    uint32_t u32Type;           // 1 = Dynamic
    uint32_t fFlags;            // 0
    char     szComment[256];    // description
    uint32_t offBlocks;         // offset of block map in file
    uint32_t offData;           // offset of first data block in file
    uint32_t u32CylGeometry;    // legacy CHS
    uint32_t u32HeadGeometry;
    uint32_t u32SectGeometry;
    uint32_t cbSector;          // 512
    uint32_t u32Dummy;          // 0
    uint64_t cbDisk;            // virtual disk size in bytes
    uint32_t cbBlock;           // block size (1MB)
    uint32_t cbBlockExtra;      // 0
    uint32_t cBlocks;           // total number of blocks
    uint32_t cBlocksAllocated;  // number of allocated blocks
    uint8_t  uuidCreate[16];
    uint8_t  uuidModify[16];
    uint8_t  uuidLinkage[16];
    uint8_t  uuidParentModify[16];
    // pad to 400 bytes from u32Signature
    uint8_t  padding[56];       // zeros
} VDI_Header;
#pragma pack(pop)

bool CVDiskWriter::vdi_Create(const char* dst_file, uint64_t cbCapacity)
{
    m_hFile = CreateFileA(dst_file,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ, NULL,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        NULL);
    if (m_hFile == INVALID_HANDLE_VALUE) {
        m_lasterr = L"Cannot create VDI file";
        return false;
    }

    m_vdi_totalBlocks = (uint32_t)((cbCapacity + VDI_BLOCK_SIZE - 1) / VDI_BLOCK_SIZE);
    m_vdi_allocatedBlocks = 0;

    m_vdi_blockMap = (uint32_t*)malloc((size_t)m_vdi_totalBlocks * 4);
    m_vdi_blockBuf = (uint8_t*)malloc(VDI_BLOCK_SIZE);
    if (!m_vdi_blockMap || !m_vdi_blockBuf) {
        m_lasterr = L"Memory allocation failed";
        return false;
    }
    // 0xFFFFFFFF = unallocated
    memset(m_vdi_blockMap, 0xFF, (size_t)m_vdi_totalBlocks * 4);
    m_vdi_blockBufUsed = 0;
    m_vdi_curBlock = 0;

    // --- Layout ---
    // Offset 0:    Header (512 bytes, padded)
    // After hdr:   Block Map (totalBlocks * 4, rounded to 512)
    // After map:   Data blocks (1MB each, appended)

    uint32_t headerTotalSize = 512; // pre-header(64) + signature(4) + version(4) + body(400) = 472, pad to 512

    m_vdi_blockMapOffset = headerTotalSize;
    uint64_t blockMapSize = (uint64_t)m_vdi_totalBlocks * 4;
    uint64_t blockMapSizePadded = ((blockMapSize + VDI_SECTOR_SIZE - 1) / VDI_SECTOR_SIZE) * VDI_SECTOR_SIZE;

    m_vdi_dataOffset = m_vdi_blockMapOffset + blockMapSizePadded;
    m_vdi_nextBlockOffset = m_vdi_dataOffset;

    // --- Build & write header ---
    VDI_Header hdr;
    memset(&hdr, 0, sizeof(hdr));

    // Pre-header file info string
    const char* fileInfo = "<<< Oracle VM VirtualBox Disk Image >>>\n";
    strncpy_s(hdr.szFileInfo, sizeof(hdr.szFileInfo), fileInfo, _TRUNCATE);

    hdr.u32Signature = 0xBEDA107F;
    hdr.u32Version = 0x00010001; // version 1.1

    hdr.cbHeader = 400;  // size of header body from u32Type onward
    hdr.u32Type = 1;     // Dynamic
    hdr.fFlags = 0;

    const char* comment = "Created by disk2vmdk";
    strncpy_s(hdr.szComment, sizeof(hdr.szComment), comment, _TRUNCATE);

    hdr.offBlocks = (uint32_t)m_vdi_blockMapOffset;
    hdr.offData = (uint32_t)m_vdi_dataOffset;

    // CHS geometry
    uint64_t totalSectors = cbCapacity / 512;
    hdr.u32CylGeometry = (uint32_t)min(totalSectors / (16 * 63), (uint64_t)16383);
    if (hdr.u32CylGeometry == 0) hdr.u32CylGeometry = 1;
    hdr.u32HeadGeometry = 16;
    hdr.u32SectGeometry = 63;
    hdr.cbSector = 512;

    hdr.cbDisk = cbCapacity;
    hdr.cbBlock = VDI_BLOCK_SIZE;
    hdr.cbBlockExtra = 0;
    hdr.cBlocks = m_vdi_totalBlocks;
    hdr.cBlocksAllocated = 0; // will be updated at Close

    // Simple UUIDs
    uint64_t uid = GetTickCount64();
    memcpy(hdr.uuidCreate, &uid, 8);
    uid ^= cbCapacity;
    memcpy(hdr.uuidCreate + 8, &uid, 8);
    memcpy(hdr.uuidModify, hdr.uuidCreate, 16);

    m_cbFilePos = 0;
    // Write header - ensure exactly 512 bytes
    if (!FileWrite(&hdr, min(sizeof(hdr), (size_t)headerTotalSize))) return false;
    if (sizeof(hdr) < headerTotalSize) {
        if (!FilePad(headerTotalSize)) return false;
    }

    // --- Write block map placeholder ---
    if (!FileWrite(m_vdi_blockMap, (uint32_t)blockMapSize)) return false;
    if (blockMapSize < blockMapSizePadded)
        if (!FilePad(m_vdi_blockMapOffset + blockMapSizePadded)) return false;

    return true;
}

bool CVDiskWriter::vdi_WriteBlock(uint32_t blockIndex, const void* data, uint32_t len)
{
    if (blockIndex >= m_vdi_totalBlocks) {
        m_lasterr = L"VDI block index out of range";
        return false;
    }

    // Record in block map: this block's index in the data area
    m_vdi_blockMap[blockIndex] = m_vdi_allocatedBlocks;
    m_vdi_allocatedBlocks++;

    if (!FileSetPos(m_vdi_nextBlockOffset)) return false;
    if (!FileWrite(data, len)) return false;

    m_vdi_nextBlockOffset = m_cbFilePos;
    return true;
}

bool CVDiskWriter::vdi_Close()
{
    // Flush pending partial block
    if (m_vdi_blockBufUsed > 0) {
        if (m_vdi_blockBufUsed < VDI_BLOCK_SIZE)
            memset(m_vdi_blockBuf + m_vdi_blockBufUsed, 0, VDI_BLOCK_SIZE - m_vdi_blockBufUsed);
        if (!IsZeroBuf(m_vdi_blockBuf, VDI_BLOCK_SIZE)) {
            if (!vdi_WriteBlock(m_vdi_curBlock, m_vdi_blockBuf, VDI_BLOCK_SIZE))
                return false;
        }
        m_vdi_blockBufUsed = 0;
    }

    // --- Rewrite block map ---
    if (!FileWriteAt(m_vdi_blockMapOffset, m_vdi_blockMap, m_vdi_totalBlocks * 4))
        return false;

    // --- Update header: cBlocksAllocated ---
    uint32_t offsetAllocated = 64 + 4 + 4   // pre-header + sig + ver
        + 4 + 4 + 4 + 256 + 4 + 4 + 4 + 4 + 4 + 4 + 4 + 8 + 4 + 4
        + 4; // cBlocksAllocated is right after cBlocks
    // More precisely: offset of cBlocksAllocated in VDI_Header
    // Let's compute: szFileInfo(64) + sig(4) + ver(4) + cbHeader(4) + u32Type(4) + fFlags(4)
    //   + szComment(256) + offBlocks(4) + offData(4) + CHS(4+4+4) + cbSector(4) + dummy(4)
    //   + cbDisk(8) + cbBlock(4) + cbBlockExtra(4) + cBlocks(4) + cBlocksAllocated(4)
    //   = 64+4+4 +4+4+4+256+4+4+4+4+4+4+4+8+4+4+4 = 394  => offset of cBlocksAllocated = 394
    uint32_t offAllocField = (uint32_t)offsetof(VDI_Header, cBlocksAllocated);
    if (!FileWriteAt(offAllocField, &m_vdi_allocatedBlocks, 4))
        return false;

    return true;
}
