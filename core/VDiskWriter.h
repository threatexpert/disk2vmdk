#pragma once

#include <stdint.h>
#include <windows.h>
#include <string>

//
// CVBoxSimple replacement ！ high-performance sequential virtual disk writer
// Supports: VMDK (monolithicSparse), VHD (Dynamic), VDI (Dynamic)
// 
// Drop-in interface compatible with CVBoxSimple:
//   CreateImage(format, dst_file, capacity)
//   Write(offset, data, size)      ！ write data at logical disk offset
//   WriteZero(offset, size)        ！ mark range as zero (sparse, not written to file)
//   Close()
//
// Key design: all grain/block data is appended sequentially to the file.
// Metadata (grain tables, BAT, block map) is maintained in memory and flushed
// once at Close(). This eliminates the random-IO metadata updates that make
// VBoxDDU's VMDK writer slow.
//

class CVDiskWriter
{
public:
    CVDiskWriter();
    virtual ~CVDiskWriter();

    // format: "VMDK", "VHD", or "VDI"  (case-insensitive)
    // dst_file: output file path (UTF-8)
    // cbCapacity: virtual disk size in bytes
    bool CreateImage(const char* format, const char* dst_file, uint64_t cbCapacity);

    // Write data at virtual disk offset.  Must be called in strictly
    // ascending offset order (sequential image creation).
    bool Write(uint64_t offDisk, const void* pvBuf, size_t cbBuf);

    // Mark a range as zero / sparse.  The data will NOT be written to the
    // file ！ the grain/block is left unallocated.  Must also be called in order.
    bool WriteZero(uint64_t offDisk, uint64_t cbLen);

    // Finalize: flush metadata, write footer, close file.
    bool Close();

    std::wstring lasterr() const { return m_lasterr; }

private:
    // --- common ---
    HANDLE   m_hFile;
    uint64_t m_cbCapacity;      // virtual disk size
    uint64_t m_cbFilePos;       // current physical file write position
    std::wstring m_lasterr;

    enum Format { FMT_NONE = 0, FMT_VMDK, FMT_VHD, FMT_VDI };
    Format   m_fmt;

    bool FileWrite(const void* data, uint32_t len);
    bool FileWriteAt(uint64_t offset, const void* data, uint32_t len);
    bool FilePad(uint64_t targetPos);        // pad with zeros to reach targetPos
    bool FileSetPos(uint64_t pos);

    // --- VMDK (monolithicSparse) ---
    //  Header(512) + Descriptor(~4KB) + padding to sector
    //  then GrainDirectory + GrainTables (pre-allocated at known size)
    //  then Grain data (appended sequentially, 64KB each)
    static const uint32_t VMDK_SECTOR_SIZE        = 512;
    static const uint32_t VMDK_GRAIN_SIZE_SECTORS  = 128;     // 128 sectors = 64KB
    static const uint32_t VMDK_GRAIN_SIZE          = 128*512; // 65536
    static const uint32_t VMDK_GT_ENTRIES          = 512;     // entries per grain table
    static const uint32_t VMDK_GT_SIZE_BYTES       = 512*4;   // 2048 bytes per GT
    static const uint32_t VMDK_GDE_SIZE            = 4;       // 32-bit offset

    uint32_t  m_vmdk_numGrains;
    uint32_t  m_vmdk_numGTs;           // number of grain tables
    uint32_t  m_vmdk_numGDEntries;     // = numGTs
    uint32_t* m_vmdk_gd;               // grain directory (sector offsets to GTs)
    uint32_t* m_vmdk_gt;               // flat array: all grain table entries
    uint64_t  m_vmdk_rgdSectorOffset;  // sector offset where redundant GD starts
    uint64_t  m_vmdk_gdSectorOffset;   // sector offset where primary GD starts
    uint64_t  m_vmdk_rgtSectorOffset;  // sector offset where redundant GTs start
    uint64_t  m_vmdk_gtSectorOffset;   // sector offset where primary GTs start
    uint64_t  m_vmdk_dataSectorOffset; // sector offset where grain data starts
    uint64_t  m_vmdk_nextGrainFileSector; // next sector to write a grain at

    bool vmdk_Create(const char* dst_file, uint64_t cbCapacity);
    bool vmdk_FlushWriteBuf();          // flush accumulated grains to file in one IO
    bool vmdk_Close();

    // Large write buffer: accumulates multiple grains for a single FileWrite
    // VMDK_WRITE_BUF_GRAINS grains = 2MB batch per IO call
    static const uint32_t VMDK_WRITE_BUF_GRAINS   = 32;    // 32 x 64KB = 2MB
    static const uint32_t VMDK_WRITE_BUF_SIZE      = VMDK_WRITE_BUF_GRAINS * VMDK_GRAIN_SIZE;
    uint8_t*  m_vmdk_writeBuf;          // 2MB write buffer
    uint32_t  m_vmdk_writeBufUsed;      // bytes used in write buffer
    uint32_t  m_vmdk_writeBufBaseGrain; // grain index of first grain in buffer

    // Per-grain tracking within the current write buffer
    uint8_t*  m_vmdk_grainBuf;          // points into m_vmdk_writeBuf for current grain
    uint32_t  m_vmdk_grainBufUsed;      // bytes used in current grain slot
    uint32_t  m_vmdk_curGrain;          // grain index being filled

    // --- VHD (Dynamic) ---
    //  Copy of Footer(512) + DynHeader(1024) + BAT + Data Blocks + Footer(512)
    //  Each data block: SectorBitmap(ceil(blockSectors/8) rounded to 512) + data
    static const uint32_t VHD_SECTOR_SIZE         = 512;
    static const uint32_t VHD_BLOCK_SIZE           = 2*1024*1024; // 2MB
    static const uint32_t VHD_SECTORS_PER_BLOCK    = VHD_BLOCK_SIZE / VHD_SECTOR_SIZE;
    static const uint32_t VHD_BITMAP_SECTORS       = 1; // ceil(4096 bits / 8 / 512) = 1 sector for 2MB block
    static const uint32_t VHD_BITMAP_SIZE          = VHD_BITMAP_SECTORS * VHD_SECTOR_SIZE;

    uint32_t  m_vhd_maxBATEntries;
    uint32_t* m_vhd_bat;
    uint64_t  m_vhd_dataStartOffset;    // file offset where first data block begins
    uint64_t  m_vhd_nextBlockOffset;    // next file offset to write a block at

    // write buffer for accumulating partial-block data
    uint8_t*  m_vhd_blockBuf;
    uint32_t  m_vhd_blockBufUsed;
    uint32_t  m_vhd_curBlock;

    bool vhd_Create(const char* dst_file, uint64_t cbCapacity);
    bool vhd_WriteBlock(uint32_t blockIndex, const void* data, uint32_t len);
    bool vhd_Close();
    static uint32_t vhd_Checksum(const uint8_t* buf, uint32_t len);

    // --- VDI (Dynamic) ---
    //  VDI Header (pre-header 72 + header body 400 = 512 aligned to 512)
    //  Block Map (array of uint32_t, one per block)
    //  Data Blocks (1MB each, appended sequentially)
    static const uint32_t VDI_BLOCK_SIZE           = 1*1024*1024; // 1MB
    static const uint32_t VDI_SECTOR_SIZE          = 512;
    static const uint32_t VDI_HEADER_SIZE          = 512;  // total header size (padded)

    uint32_t  m_vdi_totalBlocks;
    uint32_t  m_vdi_allocatedBlocks;
    uint32_t* m_vdi_blockMap;
    uint64_t  m_vdi_blockMapOffset;      // file offset of block map
    uint64_t  m_vdi_dataOffset;          // file offset where first data block begins
    uint64_t  m_vdi_nextBlockOffset;

    // write buffer for accumulating partial-block data
    uint8_t*  m_vdi_blockBuf;
    uint32_t  m_vdi_blockBufUsed;
    uint32_t  m_vdi_curBlock;

    bool vdi_Create(const char* dst_file, uint64_t cbCapacity);
    bool vdi_WriteBlock(uint32_t blockIndex, const void* data, uint32_t len);
    bool vdi_Close();
};
