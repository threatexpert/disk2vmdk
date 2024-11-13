#include "pch.h"
#include "ImageMaker.h"
#include <winioctl.h>
#include <shlwapi.h>
#include <assert.h>
#include <atlstr.h>
#include "langstr.h"
#include "osutils.h"

std::wstring findVBoxManage()
{
    WCHAR szCurrDir[MAX_PATH];
    std::wstring path;

    DWORD dwRes = GetModuleFileNameW(NULL, szCurrDir, MAX_PATH);
    *wcsrchr(szCurrDir, '\\') = '\0';

    path = std::wstring(szCurrDir) + L"\\vbox32\\VBoxManage.exe";
    if (PathFileExistsW(path.c_str()))
        return path;
#ifdef _DEBUG
    path = std::wstring(szCurrDir) + L"\\..\\vbox32\\VBoxManage.exe";
    if (PathFileExistsW(path.c_str()))
        return path;
    path = std::wstring(szCurrDir) + L"\\..\\..\\vbox32\\VBoxManage.exe";
    if (PathFileExistsW(path.c_str()))
        return path;
    path = std::wstring(szCurrDir) + L"\\..\\..\\..\\vbox32\\VBoxManage.exe";
    if (PathFileExistsW(path.c_str()))
        return path;
#endif
    return L"";
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

CImageMaker::CImageMaker()
{
    m_nSourceSize = m_nSourceDataSpaceSize = 0;
    m_hDest = INVALID_HANDLE_VALUE;
    m_nDestTotalWritten = 0;
    m_nSPos = 0;
    m_dwOptions = 0;
    m_format = L"VMDK";
    m_pCB = NULL;
    m_dwBufferSize = 256 * 10240;
    m_exit_flag = 0;
    m_convertor_exit_code = -1;
    m_strVBoxManage = findVBoxManage();
    m_vboxm_proc_Terminated = FALSE;
    m_bVBoxmUsed = FALSE;
    m_vbox_libmode = FALSE;
    m_nVDCapacity = 0;
    m_nDestDataWritten = 0;
    m_WorkingMode = 0;
}

CImageMaker::~CImageMaker()
{
    CThread::Wait(INFINITE);
    if (m_hDest != INVALID_HANDLE_VALUE)
        CloseHandle(m_hDest);
    m_diskReader.Close();
}

BOOL CImageMaker::setVBoxManage(LPCWSTR lpszApp)
{
    if (!PathFileExistsW(lpszApp))
        return FALSE;
    m_strVBoxManage = lpszApp;
    return TRUE;
}

BOOL CImageMaker::setOptions(DWORD dwOptions)
{
    m_dwOptions = dwOptions;
    return TRUE;
}

BOOL CImageMaker::setSource(LPCWSTR lpszSrc, uint64_t nSrcSize)
{
    if (!m_diskReader.OpenLocal(lpszSrc)) {
        m_lasterr = LSTRW(RID_ReadSourceFaied);
        m_lasterr += GetLastErrorAsString();
        return FALSE;
    }
    m_strSource = lpszSrc;
    m_nSourceSize = nSrcSize;
    m_nVDCapacity = nSrcSize;
    m_ranges.setSize(nSrcSize);
    return TRUE;
}

BOOL CImageMaker::setSource(int diskno, uint64_t nSrcSize)
{
    WCHAR Drive[MAX_PATH];
    wsprintf(Drive, L"\\\\.\\PhysicalDrive%d", diskno);

    return setSource(Drive, nSrcSize);
}

BOOL CImageMaker::setRemoteSource(LPCWSTR lpszIP, int port, LPCWSTR lpszPasswd, int diskno, uint64_t nSrcSize)
{
    WCHAR Drive[MAX_PATH];
    wsprintf(Drive, L"\\\\.\\PhysicalDrive%d", diskno);
    m_diskReader.SetConnectionCallback(this);
    if (!m_diskReader.OpenRemote(lpszIP, port, lpszPasswd ,Drive)) {
        m_lasterr = LSTRW(RID_ReadSourceFaied);
        m_lasterr += GetLastErrorAsString();
        return FALSE;
    }
    m_strSource = lpszIP;
    m_strSource += Drive;
    m_nSourceSize = nSrcSize;
    m_nVDCapacity = nSrcSize;
    m_ranges.setSize(nSrcSize);
    return TRUE;
}

BOOL CImageMaker::setDest(LPCWSTR lpszDst, bool FormatBySuffix)
{
    if (PathFileExistsW(lpszDst)) {
        m_lasterr = LSTRW(RID_DestFileExist);
        return FALSE;
    }
    if (!lpszDst || !lpszDst[0]) {
        m_lasterr = LSTRW(RID_DestEmpty);
        return FALSE;
    }
    if (FormatBySuffix) {
        LPCWSTR pExt = PathFindExtensionW(lpszDst);
        if (!pExt || !pExt[0] || !setFormat(pExt+1)) {
         m_lasterr = LSTRW(RID_SuffixNotSupported);
            return FALSE;
        }
    }

    m_strDest = lpszDst;
    return TRUE;
}

BOOL CImageMaker::setBufferSize(DWORD nSize)
{
    m_dwBufferSize = nSize;
    return TRUE;
}

BOOL CImageMaker::addExcludeRange(uint64_t start, uint64_t end, int unit_size)
{
    if (m_ranges.getSize() == 0) {
        m_lasterr = LSTRW(RID_UnkSourceSize);
        return FALSE;
    }
    if (!m_ranges.insert(start*unit_size, end*unit_size, false)) {
        m_lasterr = LSTRW(RID_SetExcludeRangeError);
        return FALSE;
    }
    return TRUE;
}

BOOL CImageMaker::addRangeHandler(uint64_t start, uint64_t end, int unit_size, CRangeMgr::RangeHandler* pHandler)
{
    if (m_ranges.getSize() == 0) {
        m_lasterr = LSTRW(RID_UnkSourceSize);
        return FALSE;
    }
    if (!m_ranges.insert(start * unit_size, end * unit_size, true, pHandler)) {
        m_lasterr = LSTRW(RID_SetExcludeRangeError);
        return FALSE;
    }
    pHandler->SetExitFlag(&m_exit_flag);
    return TRUE;

}

BOOL CImageMaker::setCallback(IImageMakerCallback* pCB)
{
    m_pCB = pCB;
    return TRUE;
}

BOOL CImageMaker::setFormat(LPCWSTR lpszFmt)
{
    if (!_wcsicmp(lpszFmt, L"VDI") || !_wcsicmp(lpszFmt, L"VMDK") || !_wcsicmp(lpszFmt, L"VHD") || !_wcsicmp(lpszFmt, L"DD")) {
        m_format = lpszFmt;
        return TRUE;
    }
    return FALSE;
}

std::wstring CImageMaker::getFormat()
{
    return m_format;
}

BOOL CImageMaker::start()
{
    m_ranges.insert_nomore();
    int bufpool_count = 2;
    WCHAR szCmd[1024];
    std::wstring strCmd, opts;

    //VBoxManage.exe convertfromraw stdin "<SAVE TO>" --format VMDK <SIZE>
    if (m_dwOptions & (OptStandard | OptFixed | OptSplit2G)) {
        if (m_dwOptions & OptStandard) {
            opts += L"Standard";
        }
        if (m_dwOptions & OptFixed) {
            if (opts.size() > 0)
                opts += L",";
            opts += L"Fixed";
        }
        if (m_dwOptions & OptSplit2G) {
            if (opts.size() > 0)
                opts += L",";
            opts += L"Split2G";
        }
        opts = L" --variant " + opts;
    }

    if (_wcsicmp(m_format.c_str(), L"DD") == 0) {
        m_WorkingMode = Mode_DD;
    }
    else {
        bufpool_count = 10;
        uint64_t nAligned = 0;

        if (_wcsicmp(m_format.c_str(), L"VHD") == 0) {
            const int VHD_BLOCK_SIZE = 1024 * 1024 * 2; // 2M对齐，VHD因为对齐的问题，镜像容量可能会比原磁盘大一些。不对齐会导致末尾一点数据VDWrite写不进去
            if (m_nSourceSize % VHD_BLOCK_SIZE) {
                nAligned = VHD_BLOCK_SIZE - (m_nSourceSize % VHD_BLOCK_SIZE);
                m_nVDCapacity = m_nSourceSize + nAligned;
            }
        }

        if (nAligned) {
            if (!m_ranges.extend_range(nAligned, false)) {
                m_lasterr = L"extend range error";
                return FALSE;
            }
        }

        m_bVBoxmUsed = TRUE;
        if (m_vbox_libmode || !m_strVBoxManage.size()) //没有VBoxManage.exe就采用直接调用dll模式
            m_vbox_libmode = CVBoxSimple::InitLib();

        if (m_vbox_libmode) {
            m_WorkingMode = Mode_VD_Lib;
        }
        else {
            if (!PathFileExistsW(m_strVBoxManage.c_str()))
            {
                m_lasterr = LSTRW(RID_ConvertorNotFound);
                return FALSE;
            }
            wsprintf(szCmd, L"\"%s\" convertfromraw stdin \"%s\" --format %s %I64d",
                m_strVBoxManage.c_str(), m_strDest.c_str(), m_format.c_str(), m_nVDCapacity);

            strCmd = szCmd;
            if (opts.size()) {
                strCmd += opts;
            }

            if (!m_vboxm_proc.CreateChild(strCmd.c_str(), true)) {
                m_lasterr = LSTRW(RID_CreateWorkerProcessError);
                m_lasterr += GetLastErrorAsString();
                return FALSE;
            }
            m_vboxm_proc.EnablePushMode();
            m_WorkingMode = Mode_VD_Piped;
        }
    }

    if (!m_buffers_reader.init_pool(m_dwBufferSize, bufpool_count)) {
        m_lasterr = LSTRW(RID_InitBufferFailed);
        m_lasterr += GetLastErrorAsString();
        return FALSE;
    }

    if (!CThread::Start()) {
        m_lasterr = LSTRW(RID_CreateWorkerThreadError);
        m_lasterr += GetLastErrorAsString();
        return FALSE;
    }
    return TRUE;
}

//返回1 表示都正常退出了
//返回0 表示还有线程或子进程为退出
//返回-1 表示都退出了，但结果不正常
int CImageMaker::wait(DWORD dwMS)
{
    if (m_bVBoxmUsed) {
        if (m_vbox_libmode) {
            //等待读写线程退出
            if (CThread::GetHandle()) {
                if (!CThread::Wait(dwMS))
                    return 0;
                if (m_nDestTotalWritten == m_nVDCapacity)
                    m_convertor_exit_code = 0;
                else
                    m_convertor_exit_code = -2;
            }
        }
        else {
            //用了VBoxManage时，等待该进程退出
            if (m_vboxm_proc_Terminated)
                return -1;
            int ret = m_vboxm_proc.WaitForProcessExitCode2(dwMS, &m_convertor_exit_code);
            if (ret == 0)
                return 0;
            else if (ret > 0) {
                if (m_convertor_exit_code != 0) {
                    if (m_vboxm_proc.get_outputsize()) {
                        m_lasterr += L"\nconvertor error: ";
                        m_lasterr += (LPCWSTR)CA2W(m_vboxm_proc.get_output().c_str());
                    }
                }
            }
        }
    }
    else {
        //等待读写线程退出
        if (CThread::GetHandle()) {
            if (!CThread::Wait(dwMS))
                return 0;
            if (m_nDestTotalWritten == m_nVDCapacity)
                m_convertor_exit_code = 0;
            else
                m_convertor_exit_code = -2;
        }
    }
    return 1;
}

BOOL CImageMaker::stop()
{
    m_exit_flag = 1;
    m_diskReader.Abort();
    m_buffers_reader.Abort(true);
    m_buffers_data2write.Abort(true);
    m_vboxm_proc.Close();
    m_vboxm_proc_Terminated = TRUE;
    return TRUE;
}

BOOL CImageMaker::setDestReadOnly()
{
    DWORD dwAttrs = GetFileAttributesW(m_strDest.c_str());
    if (dwAttrs == INVALID_FILE_ATTRIBUTES)
        return FALSE;
    return SetFileAttributesW(m_strDest.c_str(), dwAttrs|FILE_ATTRIBUTE_READONLY);
}

void CImageMaker::EnableVBoxLibMode()
{
    m_vbox_libmode = TRUE;
}

DWORD CImageMaker::run()
{
    uint64_t pos = 0, range_size, pushed_size = 0, left = 0, blocksz = 0, cbR64;
    CRangeMgr::Range r;
    DWORD cbR;
    int errcode = -1;
    CBuffers::Buffer* buf = NULL;
    DWORD dwWriterTid;
    HANDLE hThreadWriter = NULL;
    BOOL bIsReadIssue = FALSE;
    BOOL bIsFreeSpace;

    m_lasterr = L"";

    if (m_WorkingMode == Mode_DD) {
        if (m_pCB)
            m_pCB->ImageMaker_BeforeStart(LSTRW(RID_MakingImageDD));
        m_hDest = CreateFileW(m_strDest.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ, NULL,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
            NULL);
        if (m_hDest == INVALID_HANDLE_VALUE) {
            m_lasterr = LSTRW(RID_CreateDestFailed);
            m_lasterr += GetLastErrorAsString();
            goto _END;
        }
    }
    else if (m_WorkingMode == Mode_VD_Lib) {
        if (m_pCB)
            m_pCB->ImageMaker_BeforeStart(LSTRW(RID_MakingImage));
        if (!m_vb_writer.CreateImage(CW2A(m_format.c_str(), CP_UTF8), CW2A(m_strDest.c_str(), CP_UTF8), m_nVDCapacity)) {
            m_lasterr = L"VBox::CreateImage Error";
            goto _END;
        }
    }
    else if (m_WorkingMode == Mode_VD_Piped) {
        if (0 != m_vboxm_proc.WaitForProcessExitCode2(1000, &cbR)) {
            if (m_vboxm_proc.get_outputsize()) {
                m_lasterr = L"VBox::Exiting Unexpected.";
                m_lasterr += CA2W(m_vboxm_proc.get_output().c_str());
            }
            goto _END;
        }
    }
    else {
        goto _END;

    }

    if (!m_ranges.calcSize(&m_nSourceDataSpaceSize)) {
        m_nSourceDataSpaceSize = m_nSourceSize;
    }
    assert(m_nSourceDataSpaceSize <= m_nSourceSize);

    if (m_pCB)
        m_pCB->ImageMaker_OnStart();

    hThreadWriter = CreateThread(0, 0, s_ThreadWriter, this, 0, &dwWriterTid);
    if (!hThreadWriter)
        goto _END;

    for (; ;) {
        if (m_exit_flag) {
            goto _END;
        }

        if (!m_ranges.getrange(pos, r)) {
            if (pos == m_ranges.getSize()) {
                errcode = 0;
                goto _PERFECT;
            }
            else {
                m_lasterr = LSTRW(RID_GetNextRangeError);
                goto _END_READ_ISSUE;
            }
        }

        range_size = r.end - r.start;
        if (r.include) {
            if (r.handler) {
                left = range_size;
                if (!r.handler->GetSize(&blocksz) || blocksz != range_size) {
                    assert(false);
                    m_lasterr = L"Unexpected data size from handler";
                    goto _END;
                }
                for (; left > 0;) {
                    buf = m_buffers_reader.Pop(INFINITE);
                    if (!buf || m_exit_flag)
                        goto _END;

                    blocksz = min(left, buf->buffer_size);
                    if (!r.handler->Read(buf->p, (DWORD)blocksz, &cbR64, &bIsFreeSpace)) {
                        m_lasterr = LSTRW(RID_ReadSourceFaied);
                        m_lasterr += GetLastErrorAsString();
                        goto _END_READ_ISSUE;
                    }
                    if (cbR64 == 0) {
                        //分区有剩余空间没读取出来，
                        //通常是分区的实际大小比可用的大小要大，多出来的部分在分区层读取不到了
                        //这里临时调整区间信息，将未读取的部分单独划分一个区间
                        range_size -= left;
                        if (!m_ranges.split_range(r.start, r.end, r.start + range_size, true, NULL)) {
                            m_lasterr = L"Adding range of remaining space of volume error";
                            goto _END_READ_ISSUE;
                        }
                        left = 0;
                        buf->data_length = 0;
                        buf->zero_length = 0;
                        m_buffers_reader.Push(buf);
                        buf = NULL;
                    }
                    else {
                        //handler 允许读取的长度比预期长度长，这种情况，长度表示数据全是0x00，缓冲区的预期长度数据也清0
                        if (bIsFreeSpace) {
                            if (cbR64 > blocksz && (DWORD)blocksz < buf->buffer_size)
                                memset((char*)buf->p + blocksz, 0, buf->buffer_size - (DWORD)blocksz);
                            buf->zero_length = cbR64;
                            if (cbR64 > left) {
                                assert(false);
                                m_lasterr = L"Unexpected data length read from handler";
                                goto _END_READ_ISSUE;
                            }
                        }
                        else {
                            buf->data_length = (DWORD)cbR64;
                        }
                        pushed_size += cbR64;
                        m_buffers_data2write.Push(buf);
                        buf = NULL;
                        left -= cbR64;
                    }
                }
            }
            else {
                if (!m_diskReader.SetPos(pos)) {
                    m_lasterr = LSTRW(RID_SetSourceFilePointerFaied);
                    m_lasterr += GetLastErrorAsString();
                    goto _END_READ_ISSUE;
                }

                left = range_size;
                if (!m_diskReader.NeedToRead(left)) {
                    goto _END_READ_ISSUE;
                }
                for (; left > 0;) {
                    buf = m_buffers_reader.Pop(INFINITE);
                    if (!buf || m_exit_flag)
                        goto _END;

                    blocksz = min(left, buf->buffer_size);
                    if (!m_diskReader.Read(buf->p, (DWORD)blocksz, &cbR, pos + (range_size - left))) {
                        m_lasterr = LSTRW(RID_ReadSourceFaied);
                        m_lasterr += GetLastErrorAsString();
                        goto _END_READ_ISSUE;
                    }
                    if (cbR == 0) {
                        m_lasterr = LSTRW(RID_UnexpectedEOF);
                        goto _END_READ_ISSUE;
                    }
                    pushed_size += cbR;
                    buf->data_length = cbR;
                    m_buffers_data2write.Push(buf);
                    buf = NULL;
                    left -= cbR;
                }
            }
        }
        else {
            buf = m_buffers_reader.Pop(INFINITE);
            if (!buf || m_exit_flag)
                goto _END;
            pushed_size += range_size;
            buf->zero_length = range_size;
            memset(buf->p, 0, buf->buffer_size);
            m_buffers_data2write.Push(buf);
            buf = NULL;
        }
        pos += range_size;
    }
_END_READ_ISSUE:
    bIsReadIssue = TRUE;
_END:
    m_exit_flag = 1;
    m_buffers_data2write.Abort(true);
    m_vboxm_proc.Close();
    m_vboxm_proc_Terminated = TRUE;
_PERFECT:
    m_diskReader.Shutdown();
    if (hThreadWriter) {
        m_buffers_data2write.Abort(m_exit_flag ? TRUE : FALSE);
        WaitForSingleObject(hThreadWriter, INFINITE);
        CloseHandle(hThreadWriter);
        if (!bIsReadIssue && m_writer_err.size()) {
            m_lasterr = m_writer_err;
            errcode = -3;
        }
    }
    //保存文件操作放在线程，防止界面卡住
    if (m_hDest != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hDest);
        m_hDest = INVALID_HANDLE_VALUE;
    }
    if (m_vbox_libmode) {
        m_vb_writer.Close();
    }
    setDestReadOnly();
    if (buf) {
        m_buffers_reader.Push(buf);
    }
    m_ranges.clear();
    if (m_pCB)
        m_pCB->ImageMaker_OnEnd(errcode);

    return 0;
}

DWORD __stdcall CImageMaker::s_ThreadWriter(void* p)
{
    CImageMaker* _this = (CImageMaker*)p;
    return _this->ThreadWriter();
}

DWORD CImageMaker::ThreadWriter()
{
    CBuffers::Buffer* buf = NULL;
    uint64_t left = 0, blocksz = 0;
    DWORD dwEC = -1;
    for (;;) {
        buf = m_buffers_data2write.Pop(INFINITE);
        if (!buf)
            break;
        assert(buf->data_length > 0 || buf->zero_length > 0);
        if (buf->zero_length) {
            left = buf->zero_length;
            for (; left > 0;) {
                if (m_exit_flag) {
                    goto _END;
                }
                if (m_hDest == INVALID_HANDLE_VALUE) {
                    if (m_vbox_libmode) {
                        blocksz = left;
                    }
                    else {
                        blocksz = min(left, buf->buffer_size);
                        if (!m_vboxm_proc.WriteAll(buf->p, (DWORD)blocksz)) {
                            m_writer_err = LSTRW(RID_WriteFileError);
                            m_writer_err += GetLastErrorAsString();
                            if (m_vboxm_proc.WaitForProcessExitCode2(2000, &dwEC) && m_vboxm_proc.get_outputsize())
                                m_writer_err += CA2W(m_vboxm_proc.get_output().c_str());
                            goto _ERROR;
                        }
                    }

                }
                else {
                    blocksz = min(left, buf->buffer_size);
                    if (!WriteFileAll(m_hDest, buf->p, (DWORD)blocksz)) {
                        m_writer_err = LSTRW(RID_WriteFileError);
                        m_writer_err += GetLastErrorAsString();
                        goto _ERROR;
                    }
                }
                left -= blocksz;
                m_nDestTotalWritten += blocksz;
                if (m_pCB)
                    m_pCB->ImageMaker_OnCopied(m_nDestTotalWritten, m_nVDCapacity, m_nDestDataWritten, m_nSourceDataSpaceSize);
            }
        }
        else {
            if (m_hDest == INVALID_HANDLE_VALUE) {
                if (m_vbox_libmode) {
                    if (!m_vb_writer.Write(m_nDestTotalWritten, buf->p, buf->data_length)) {
                        m_writer_err = L"写入镜像文件失败。";
                        m_writer_err += GetLastErrorAsString();
                        goto _ERROR;
                    }
                }
                else {
                    if (!m_vboxm_proc.WriteAll(buf->p, buf->data_length)) {
                        m_writer_err = LSTRW(RID_WriteFileError);
                        m_writer_err += GetLastErrorAsString();
                        if (m_vboxm_proc.WaitForProcessExitCode2(2000, &dwEC) && m_vboxm_proc.get_outputsize())
                            m_writer_err += CA2W(m_vboxm_proc.get_output().c_str());
                        goto _ERROR;
                    }
                }
            }
            else {
                if (!WriteFileAll(m_hDest, buf->p, buf->data_length)) {
                    m_writer_err = LSTRW(RID_WriteFileError);
                    m_writer_err += GetLastErrorAsString();
                    goto _ERROR;
                }
            }
            m_nDestTotalWritten += buf->data_length;
            m_nDestDataWritten += buf->data_length;
            if (m_pCB)
                m_pCB->ImageMaker_OnCopied(m_nDestTotalWritten, m_nVDCapacity, m_nDestDataWritten, m_nSourceDataSpaceSize);
        }
        buf->data_length = 0;
        buf->zero_length = 0;
        m_buffers_reader.Push(buf);
        buf = NULL;
    }
_ERROR:
    m_exit_flag = 1;
    m_buffers_reader.Abort(TRUE);
_END:
    if (buf) {
        buf->data_length = 0;
        buf->zero_length = 0;
        m_buffers_reader.Push(buf);
    }
    return 0;
}

void CImageMaker::OnConnLog(LPCWSTR lpszText)
{
    if (m_pCB) {
        m_pCB->ImageMaker_OnRemoteConnLog(lpszText);
    }
}


/// <summary>
/// 
/// </summary>

CVolumeRangeHandler::CVolumeRangeHandler()
    : m_ref(1)
{

}

CVolumeRangeHandler::~CVolumeRangeHandler()
{
    Close();
}

long CVolumeRangeHandler::AddRef()
{
    return InterlockedIncrement(&m_ref);
}

long CVolumeRangeHandler::Release()
{
    long ret = InterlockedDecrement(&m_ref);
    if (ret == 0) {
        delete this;
    }
    return ret;
}

BOOL CVolumeRangeHandler::GetSize(uint64_t* pSize)
{
    return CVolumeReader::GetSize(pSize);
}

BOOL CVolumeRangeHandler::CalcDataSize(uint64_t* pSize)
{
    return CVolumeReader::CalcDataSize(pSize);
}

BOOL CVolumeRangeHandler::Read(void* lpBuffer, DWORD nNumberOfBytesToRead, uint64_t* lpNumberOfBytesRead, BOOL* pIsFreeSpace)
{
    return CVolumeReader::Read(lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, pIsFreeSpace);
}

void CVolumeRangeHandler::Close()
{
    CVolumeReader::Close();
}

void CVolumeRangeHandler::SetExitFlag(BOOL* p)
{
}


///////////////////
//TODO: REMOTE

CRemoteVolumeRangeHandler::CRemoteVolumeRangeHandler()
    : m_ref(1)
{
    m_Size = 0;
    m_SizeUsed = -1;
    m_pos = 0;
    m_dwOptions = 0;
    m_isReady = FALSE;
}

CRemoteVolumeRangeHandler::~CRemoteVolumeRangeHandler()
{
    Close();
}

void CRemoteVolumeRangeHandler::SetConnectionCallback(IConnectionCallback* pConnCB)
{
    m_conn.SetCallback(pConnCB);
}

BOOL CRemoteVolumeRangeHandler::setOptions(DWORD dwOptions)
{
    m_dwOptions = dwOptions;
    return TRUE;
}

DWORD CRemoteVolumeRangeHandler::getOptions()
{
    return m_dwOptions;
}

BOOL CRemoteVolumeRangeHandler::OpenRemote(LPCWSTR lpszIP, int port, LPCWSTR lpszPasswd, LPCWSTR lpszSrc, uint64_t nSrcSize)
{
    m_Size = nSrcSize;
    m_remote_volname = lpszSrc;
    m_conn.setTarget(lpszIP, port, lpszPasswd);
    return TRUE;
}

std::wstring CRemoteVolumeRangeHandler::lasterr()
{
    return m_lasterr;
}

long CRemoteVolumeRangeHandler::AddRef()
{
    return InterlockedIncrement(&m_ref);
}

long CRemoteVolumeRangeHandler::Release()
{
    long ret = InterlockedDecrement(&m_ref);
    if (ret == 0) {
        delete this;
    }
    return ret;
}

BOOL CRemoteVolumeRangeHandler::GetSize(uint64_t* pSize)
{
    *pSize = m_Size;
    return TRUE;
}

BOOL CRemoteVolumeRangeHandler::CalcDataSize(uint64_t* pSize)
{
    int ret;
    m_isReady = FALSE;
RETRY:
    if (!m_conn.Retry()) {
        return FALSE;
    }
    ret = m_conn.OpenVolume(m_remote_volname.c_str(), m_dwOptions, m_pos, m_Size, &m_SizeUsed);
    if (ret != 0) {
        m_isReady = FALSE;
        if (!isneterror(ret)) {
            return FALSE;
        }
        goto RETRY;
    }
    m_conn.CloseVolume();
    m_conn.Quit();
    if (m_SizeUsed == -1) {
        return FALSE;
    }
    *pSize = m_SizeUsed;
    return TRUE;
}

BOOL CRemoteVolumeRangeHandler::Read(void* lpBuffer, DWORD nNumberOfBytesToRead, uint64_t* lpNumberOfBytesRead, BOOL* pIsFreeSpace)
{
    int ret;
    *lpNumberOfBytesRead = 0;
    *pIsFreeSpace = FALSE;
    if (m_pos == m_Size) {
        return TRUE;
    }
    else if (m_pos > m_Size) {
        m_isReady = FALSE;
        return FALSE;
    }

RETRY:
    if (!m_isReady) {
        if (!m_conn.Retry()) {
            return FALSE;
        }
        ret = m_conn.OpenVolume(m_remote_volname.c_str(), m_dwOptions, m_pos, m_Size, &m_SizeUsed);
        if (ret != 0) {
            if (!isneterror(ret)) {
                return FALSE;
            }
            goto RETRY;
        }
        ret = m_conn.ReadVolume(m_Size - m_pos);
        if (ret != 0) {
            if (!isneterror(ret)) {
                return FALSE;
            }
            goto RETRY;
        }
        m_isReady = TRUE;
    }

    ret = m_conn.ReadVolumeData(lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, pIsFreeSpace);
    if (ret != 0) {
        m_isReady = FALSE;
        if (!isneterror(ret)) {
            return FALSE;
        }
        goto RETRY;
    }
    m_pos += *lpNumberOfBytesRead;
    if (m_pos > m_Size) {
        m_isReady = FALSE;
        return FALSE;
    }
    return TRUE;
}

void CRemoteVolumeRangeHandler::Close()
{
    if (m_isReady) {
        m_conn.CloseVolume();
        m_conn.Quit();
        m_isReady = FALSE;
    }
}

void CRemoteVolumeRangeHandler::SetExitFlag(BOOL* p)
{
    m_conn._pflag = p;
}

/////////////


CDiskReader::CDiskReader()
{
    m_hSource = INVALID_HANDLE_VALUE;
    m_bRemoteMode = FALSE;
    m_needread = m_setpos = 0;
    m_isReady = FALSE;
}

CDiskReader::~CDiskReader()
{
    Close();
}

BOOL CDiskReader::OpenLocal(LPCWSTR lpszName)
{
    m_hSource = CreateFileW(lpszName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    return m_hSource != INVALID_HANDLE_VALUE;
}

BOOL CDiskReader::OpenRemote(LPCWSTR lpszIP, int port, LPCWSTR lpszPasswd, LPCWSTR lpszName)
{
    m_bRemoteMode = TRUE;
    m_remote_diskname = lpszName;
    m_conn.setTarget(lpszIP, port, lpszPasswd);
    return TRUE;
}

BOOL CDiskReader::NeedToRead(uint64_t size)
{
    if (!m_bRemoteMode) {
        return TRUE;
    }
    int ret;
    m_needread = size;
RETRY:
    if (!m_isReady) {
        if (!m_conn.Retry()) {
            return FALSE;
        }
        ret = m_conn.OpenDisk(m_remote_diskname.c_str());
        if (ret != 0) {
            if (!isneterror(ret)) {
                return FALSE;
            }
            goto RETRY;
        }
        m_isReady = TRUE;
    }

    ret = m_conn.ReadDisk(m_needread);
    if (ret != 0) {
        m_isReady = FALSE;
        if (!isneterror(ret)) {
            return FALSE;
        }
        goto RETRY;
    }
    return TRUE;
}

BOOL CDiskReader::Read(LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, uint64_t pos)
{
    if (!m_bRemoteMode) {
        return my_ReadFile(m_hSource, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, pos);
    }

    int ret;
RETRY:
    if (!m_isReady) {
        if (!m_conn.Retry()) {
            return FALSE;
        }
        ret = m_conn.OpenDisk(m_remote_diskname.c_str());
        if (ret != 0) {
            if (!isneterror(ret)) {
                return FALSE;
            }
            goto RETRY;
        }
        ret = m_conn.ReadDiskStart(pos, m_needread - (pos - m_setpos));
        if (ret != 0) {
            if (!isneterror(ret)) {
                return FALSE;
            }
            goto RETRY;
        }
        m_isReady = TRUE;
    }

    ret = m_conn.ReadDiskData(lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead);
    if (ret != 0) {
        m_isReady = FALSE;
        if (!isneterror(ret)) {
            return FALSE;
        }
        goto RETRY;
    }
    return TRUE;
}

BOOL CDiskReader::SetPos(uint64_t pos)
{
    m_setpos = pos;
    if (!m_bRemoteMode) {
        LARGE_INTEGER liPos, liNewPos;
        liPos.QuadPart = pos;
        return SetFilePointerEx(m_hSource, liPos, &liNewPos, FILE_BEGIN);
    }

    int ret;
RETRY:
    if (!m_isReady) {
        if (!m_conn.Retry()) {
            return FALSE;
        }
        ret = m_conn.OpenDisk(m_remote_diskname.c_str());
        if (ret != 0) {
            if (!isneterror(ret)) {
                return FALSE;
            }
            goto RETRY;
        }
        m_isReady = TRUE;
    }

    ret = m_conn.SeekDisk(pos);
    if (ret != 0) {
        m_isReady = FALSE;
        if (!isneterror(ret)) {
            return FALSE;
        }
        goto RETRY;
    }
    return TRUE;
}

void CDiskReader::Abort()
{
    m_conn.Abort();
}

void CDiskReader::Shutdown()
{
    if (!m_bRemoteMode) {
        return;
    }
    if (m_isReady) {
        if (0 == m_conn.CloseDisk()) {
            m_conn.Quit();
        }
        m_isReady = FALSE;
    }
}

void CDiskReader::Close()
{
    if (m_bRemoteMode) {
        m_conn.Close();
        m_isReady = FALSE;
        return;
    }
    else {
        if (m_hSource != INVALID_HANDLE_VALUE) {
            CloseHandle(m_hSource);
            m_hSource = INVALID_HANDLE_VALUE;
        }
    }
}

void CDiskReader::SetConnectionCallback(IConnectionCallback* pConnCB)
{
    m_conn.SetCallback(pConnCB);
}
