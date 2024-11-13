#include "pch.h"
#include <Windows.h>
#include <atlconv.h>
#include <atltime.h>
#include "Server.h"
#include "..\core\json.hpp"
#include "..\core\osutils.h"
#include "..\misc\SecureLayer.h"
#include "langstr.h"

using json = nlohmann::ordered_json;
using std::wstring;
using std::string;
using std::list;

#define _CW2UTF8(x) CW2A(x.c_str(), CP_UTF8)

BOOL ENABLE_ZSTD = FALSE;
int ZSTD_CLEVEL = ZSTD_CLEVEL_DEFAULT;

CRequestHandler::CRequestHandler()
{
	m_pMgr = NULL;
	m_ptcp = NULL;
	m_bEnd = FALSE;
	m_FlagExit = FALSE;
	_ab._pflag = &m_FlagExit;
	m_hOpenedDisk = INVALID_HANDLE_VALUE;
	m_disk_pos = m_vol_pos = 0;
	m_pVR = NULL;
	m_pLog = NULL;
	_passwd_ref = "";
	m_id = 0;
	m_hThreadWriter = NULL;
	m_writer_error = 0;
}

CRequestHandler::~CRequestHandler()
{
	Close();
}

BOOL CRequestHandler::Init(CServer* pMgr, SOCKET s, const char* passwd_ref)
{
	socket_setbufsize(s, 1024 * 256);
	m_pMgr = pMgr;
	m_ptcp = new CInetTcp(s);
	m_ptcp->SetAborter(&_ab);
	m_pLog = pMgr;
	_passwd_ref = passwd_ref;

	CThread::Start();
	return TRUE;
}

void CRequestHandler::Close()
{
	m_FlagExit = TRUE;
	CThread::Wait();
	if (m_ptcp) {
		delete m_ptcp;
		m_ptcp = NULL;
	}
	if (m_hOpenedDisk != INVALID_HANDLE_VALUE) {
		CloseHandle(m_hOpenedDisk);
		m_hOpenedDisk = INVALID_HANDLE_VALUE;
	}
	if (m_pVR) {
		delete m_pVR;
		m_pVR = NULL;
	}
	StopThreadWriter();
}

static int RecvBlock(CInetTcp* ptcp, void* lpBuf, int nSize, int *pBytesRecvd, int timeout_sec /*= -1*/)
{
	int nRet;
	int nBytesRecvd = 0;
	*pBytesRecvd = 0;
	while (nBytesRecvd < nSize)
	{
		nRet = ptcp->Read((char*)lpBuf + nBytesRecvd, nSize - nBytesRecvd, timeout_sec);
		if (nRet <= 0)
		{
			return nRet;
		}
		nBytesRecvd += nRet;
		*pBytesRecvd = nBytesRecvd;
	}

	return nBytesRecvd;
}

DWORD CRequestHandler::run()
{
	int ret;
	drcmd_hdr hdr = { 0 };
	int nRecvd;
	char tmp[SIZEOF_IAMREADY];
	if (_passwd_ref && _passwd_ref[0]) {
		CSecLayer* secLayer = new CSecLayer();
		ret = m_ptcp->InsertSPI(secLayer);
		secLayer->Dereference();
		if (!ret) {
			goto _NETISSUE;
		}
		if (0 != secLayer->hello(_passwd_ref)) {
			goto _NETISSUE;
		}
	}
	if (!InetWriteAll(m_ptcp, IAMREADY, SIZEOF_IAMREADY, REQUEST_TIMEOUT_SEC)) {
		goto _NETISSUE;
	}
	ret = RecvBlock(m_ptcp, tmp, SIZEOF_IAMREADY, &nRecvd, REQUEST_TIMEOUT_SEC);
	if (ret <= 0) {
		goto _NETISSUE;
	}
	if (memcmp(tmp, IAMREADY, SIZEOF_IAMREADY) != 0) {
		log(hdr.cmd, -2000, LSTRW(RID_handshake_failed));
		goto _NETISSUE;
	}
	while (!m_FlagExit) {
		ret = RecvBlock(m_ptcp, &hdr, sizeof(hdr), &nRecvd, REQUEST_TIMEOUT_SEC);
		if (ret < 0) {
			break;
		}
		else if (ret == 0) {
			log(hdr.cmd, -2000, LSTRW(RID_timeout));
			goto _EXIT;
		}

		switch (hdr.cmd) {
		case drcmd_query_pc_info:
		{
			if (!cmd_get_pc_info(hdr.datalen)) {
				goto _NETISSUE;
			}
		}break;
		case drcmd_query_disk_info:
		{
			if (!cmd_get_disk_info(hdr.datalen)) {
				goto _NETISSUE;
			}
		}break;
		case drcmd_open_disk:
		{
			if (!cmd_open_disk(hdr.datalen)) {
				goto _NETISSUE;
			}
		}break;
		case drcmd_seek_disk:
		{
			if (!cmd_seek_disk(hdr.datalen)) {
				goto _NETISSUE;
			}
		}break;
		case drcmd_read_disk:
		{
			if (!cmd_read_disk(hdr.datalen)) {
				goto _NETISSUE;
			}
		}break;
		case drcmd_close_disk:
		{
			if (!cmd_close_disk(hdr.datalen)) {
				goto _NETISSUE;
			}
		}break;
		case drcmd_open_volume:
		{
			if (!cmd_open_volume(hdr.datalen)) {
				goto _NETISSUE;
			}
		}break;
		case drcmd_read_volume:
		{
			if (!cmd_read_volume(hdr.datalen)) {
				goto _NETISSUE;
			}
		}break;
		case drcmd_close_volume:
		{
			if (!cmd_close_volume(hdr.datalen)) {
				goto _NETISSUE;
			}
		}break;
		case drcmd_quit:
		{
			log(drcmd_quit, 0, LSTRW(RID_over));
			goto _EXIT;
		}break;
		default:
			goto _NETISSUE;
		}
	}
_NETISSUE:
	log(hdr.cmd, -2000, LSTRW(RID_netword_aborted));
_EXIT:
	m_pMgr->OnRequestEnd(this);
	return 0;
}

DWORD CRequestHandler::ThreadWriter()
{
	CBuffers::Buffer* buf = NULL;
	for (;;) {
		buf = m_buffers_data2write.Pop(INFINITE);
		if (!buf) {
			m_writer_error = -1;
			break;
		}
		if (m_FlagExit) {
			m_writer_error = -2;
			goto _END;
		}
		if (!InetWriteAll(m_ptcp, (char*)buf->p + buf_reserved_head_space - sizeof(drcmd_result), 
			buf->data_length, 120)) {
			m_writer_error = WSAGetLastError();
			if (m_writer_error == 0)
				m_writer_error = -3;
			goto _ERROR;
		}

		buf->data_length = 0;
		buf->zero_length = 0;
		m_buffers_reader.Push(buf);
		buf = NULL;
	}
_ERROR:
	m_buffers_reader.Abort(TRUE);
_END:
	if (buf) {
		buf->data_length = 0;
		buf->zero_length = 0;
		m_buffers_reader.Push(buf);
	}
	return 0;
}

void CRequestHandler::StopThreadWriter()
{
	if (m_hThreadWriter) {
		m_buffers_reader.Abort(true);
		m_buffers_data2write.Abort(true);
		WaitForSingleObject(m_hThreadWriter, INFINITE);
		CloseHandle(m_hThreadWriter);
		m_hThreadWriter = NULL;
		m_buffers_data2write.clear();
		m_buffers_data2write.ResetAbort();
	}
}

void CRequestHandler::log(int cmd, int err, LPCWSTR lpszDetails)
{
	if (m_pLog) {
		CStringW strLine;
		CStringW t = CTime::GetCurrentTime().Format(L"%Y-%m-%d %H:%M:%S ");
		strLine.Format(LSTRW(RID_logpref), (LPCTSTR)t, m_id, lpszDetails);
		m_pLog->req_log_output(cmd, err, strLine);
	}
}

bool CRequestHandler::reply(BYTE cmd, BYTE err, BYTE datatype, const void* data, uint64_t datalen)
{
	drcmd_result result;
	result.cmd = cmd;
	result.err = err;
	result.datatype = datatype;
	result.datalen = datalen;

	if (!InetWriteAll(m_ptcp, &result, sizeof(result), 120)) {
		return false;
	}
	if (data && datalen) {
		if (!InetWriteAll(m_ptcp, data, (int)datalen, 120)) {
			return false;
		}
	}
	return true;
}

bool CRequestHandler::reply_diskdata(BYTE cmd, BYTE err, BYTE datatype, uint64_t datalen, CBuffers::Buffer* &data)
{
	if (m_writer_error) {
		return false;
	}
	assert(buf_reserved_head_space >= sizeof(drcmd_result));
	drcmd_result* presult = (drcmd_result*)((char*)data->p + buf_reserved_head_space - sizeof(drcmd_result));
	presult->cmd = cmd;
	presult->err = err;
	presult->datatype = datatype;
	presult->datalen = datalen;
	if (datatype & drDT_zeroblock)
		data->data_length = sizeof(drcmd_result);
	else
		data->data_length = sizeof(drcmd_result) + (DWORD)datalen;
	m_buffers_data2write.Push(data);
	data = NULL;
	return true;
}

bool CRequestHandler::cmd_get_pc_info(int reqlen)
{
	if (reqlen != 0) {
		return false;
	}
	std::string result = get_pcinfo();
	log(drcmd_query_pc_info, 0, LSTRW(RID_query_pc_info));

	return reply(drcmd_query_pc_info, 0, drDT_json, &result[0], (int)result.size());
}

bool CRequestHandler::cmd_get_disk_info(int reqlen)
{
	if (reqlen != 0) {
		return false;
	}

	std::string result = enumPartitions();
	log(drcmd_query_disk_info, 0, LSTRW(RID_drcmd_query_disk_info));

	return reply(drcmd_query_disk_info, 0, drDT_json, &result[0], (int)result.size());
}

bool CRequestHandler::cmd_open_disk(int reqlen)
{
	drcmd_open_disk_request open;
	int ret, nRecvd;

	if (reqlen == 0 || reqlen > sizeof(open)) {
		return false;
	}

	memset(&open, 0, sizeof(open));
	ret = RecvBlock(m_ptcp, &open, reqlen, &nRecvd, REQUEST_TIMEOUT_SEC);
	if (ret <= 0) {
		return false;
	}
	if (m_hOpenedDisk != INVALID_HANDLE_VALUE) {
		CloseHandle(m_hOpenedDisk);
		m_hOpenedDisk = INVALID_HANDLE_VALUE;
		assert(false);
	}
	json j = {
		{"code", 0},
		{"msg", ""}
	};
	std::string result;
	m_openedDiskName = open.name;
	m_hOpenedDisk = CreateFileA(m_openedDiskName.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (m_hOpenedDisk == INVALID_HANDLE_VALUE) {
		j["code"] = GetLastError();
		j["msg"] = (LPCSTR)_CW2UTF8(GetErrorAsString(j["code"]));
		result = j.dump(2);
		m_err.Format(LSTRW(RID_open_disk_failed), m_openedDiskName.c_str(), GetErrorAsString(j["code"]).c_str());
		log(drcmd_open_disk, 0, m_err);
		return reply(drcmd_open_disk, drerr_general, drDT_json, &result[0], (int)result.size());
	}
	else {
		LARGE_INTEGER liPos, liNewPos;
		m_disk_pos = open.pos;
		if (open.pos) {
			liPos.QuadPart = open.pos;
			if (!SetFilePointerEx(m_hOpenedDisk, liPos, &liNewPos, FILE_BEGIN) || liNewPos.QuadPart != open.pos) {
				j["code"] = GetLastError();
				j["msg"] = (LPCSTR)_CW2UTF8(std::wstring(LSTRW(RID_seek_failed)));
				result = j.dump(2);
				CloseHandle(m_hOpenedDisk);
				m_hOpenedDisk = INVALID_HANDLE_VALUE;
				log(drcmd_open_disk, 0, LSTRW(RID_open_disk_seek_failed));
				return reply(drcmd_open_disk, drerr_general, drDT_json, &result[0], (int)result.size());
			}
		}
		j["msg"] = "ok";
		result = j.dump(2);
		m_err.Format(LSTRW(RID_open_disk_ok), m_openedDiskName.c_str());
		log(drcmd_open_disk, 0, m_err);
		return reply(drcmd_open_disk, 0, drDT_json, &result[0], (int)result.size());
	}
}

bool CRequestHandler::cmd_seek_disk(int reqlen)
{
	drcmd_seek_request seekdisk;
	int ret, nRecvd;

	if (reqlen != sizeof(seekdisk)) {
		return false;
	}
	if (m_hOpenedDisk == INVALID_HANDLE_VALUE) {
		return false;
	}
	memset(&seekdisk, 0, sizeof(seekdisk));
	ret = RecvBlock(m_ptcp, &seekdisk, reqlen, &nRecvd, REQUEST_TIMEOUT_SEC);
	if (ret <= 0) {
		return false;
	}
	json j = {
		{"code", 0},
		{"msg", ""}
	};
	std::string result;
	LARGE_INTEGER liPos, liNewPos;
	liPos.QuadPart = seekdisk.pos;
	if (!SetFilePointerEx(m_hOpenedDisk, liPos, &liNewPos, FILE_BEGIN) || liNewPos.QuadPart != seekdisk.pos) {
		j["code"] = GetLastError();
		j["msg"] = (LPCSTR)_CW2UTF8(std::wstring(LSTRW(RID_seek_failed)));
		result = j.dump(2);
		m_err.Format(LSTRW(RID_seek_at_failed), seekdisk.pos);
		log(drcmd_seek_disk, 0, m_err);
		return reply(drcmd_seek_disk, drerr_general, drDT_json, &result[0], (int)result.size());
	}
	else {
		m_disk_pos = seekdisk.pos;
		j["msg"] = "ok";
		result = j.dump(2);
		m_err.Format(LSTRW(RID_seek_at_ok), seekdisk.pos);
		log(drcmd_seek_disk, 0, m_err);
		return reply(drcmd_seek_disk, 0, drDT_json, &result[0], (int)result.size());
	}
}

bool CRequestHandler::cmd_read_disk(int reqlen)
{
	drcmd_read_request readdisk;
	int ret, nRecvd;

	if (reqlen != sizeof(readdisk)) {
		return false;
	}
	if (m_hOpenedDisk == INVALID_HANDLE_VALUE) {
		return false;
	}

	memset(&readdisk, 0, sizeof(readdisk));
	ret = RecvBlock(m_ptcp, &readdisk, reqlen, &nRecvd, REQUEST_TIMEOUT_SEC);
	if (ret <= 0) {
		return false;
	}
	json j = {
		{"code", 0},
		{"msg", ""}
	};
	std::string result;
	result = j.dump(2);
	if (!reply(drcmd_read_disk, 0, drDT_json, &result[0], (int)result.size())) {
		return false;
	}
	log(drcmd_read_disk, 0, LSTRW(RID_drcmd_read_disk));

	m_writer_error = 0;
	m_buffers_reader.clear();
	m_buffers_reader.init_pool(buf_reserved_head_space + (DWORD)ZSTD_compressBound(read_chunk_maxsize), 4);
	m_hThreadWriter = CreateThread(0, 0, sThreadWriter, this, 0, 0);

	uint64_t sent = 0;
	DWORD dwRead;
	DWORD dwChunk;
	CBuffers::Buffer* buf = NULL;
	char* pBuf_diskdata;
	size_t zret;
	unsigned iserr;
	while (sent < readdisk.size) {
		if (_ab.aborted()) {
			goto _ERROR;
		}

		buf = m_buffers_reader.Pop(INFINITE);
		if (!buf || m_FlagExit) {
			goto _ERROR;
		}
		
		pBuf_diskdata = (char*)(buf->p) + buf_reserved_head_space; //预留个报头空间，但给ReadFile读磁盘数据的缓冲区地址需要对齐
		dwChunk = (DWORD) min(read_chunk_maxsize, readdisk.size - sent);

		if (ENABLE_ZSTD) {
			m_zbuf.reserve(dwChunk);
			//&m_zbuf[0]的地址有不对齐的风险
			ret = my_ReadFile(m_hOpenedDisk, &m_zbuf[0], dwChunk, &dwRead, m_disk_pos);
			if (!ret) {
				goto _READISSUE;
			}
			if (dwRead == 0) {
				if (!reply_diskdata(drcmd_read_disk, 0, drDT_diskdata, 0, buf)) {
					goto _ERROR;
				}
				break;
			}
			zret = ZSTD_compress(pBuf_diskdata, ZSTD_compressBound(read_chunk_maxsize), &m_zbuf[0], dwRead, ZSTD_CLEVEL);
			iserr = ZSTD_isError(zret);
			if (iserr)
				goto _ERROR;
			if (!reply_diskdata(drcmd_read_disk, 0, drDT_diskdata | drDT_zstd, zret, buf)) {
				goto _ERROR;
			}
		}
		else {
			ret = my_ReadFile(m_hOpenedDisk, pBuf_diskdata, dwChunk, &dwRead, m_disk_pos);
			if (!ret) {
				goto _READISSUE;
			}
			if (dwRead == 0) {
				if (!reply_diskdata(drcmd_read_disk, 0, drDT_diskdata, 0, buf)) {
					goto _ERROR;
				}
				break;
			}
			if (!reply_diskdata(drcmd_read_disk, 0, drDT_diskdata, dwRead, buf)) {
				goto _ERROR;
			}
		}

		m_disk_pos += dwRead;
		sent += dwRead;
	}
	m_buffers_data2write.Abort(FALSE);
	StopThreadWriter();
	return true;
_READISSUE:
	{
		std::wstring lasterr = GetErrorAsString(::GetLastError());
		StopThreadWriter();
		json j = { {"code", GetLastError()}, {"msg", ""} };
		result = j.dump(2);
		reply(drcmd_read_disk, drerr_general, drDT_json, &result[0], result.size());
		m_err.Format(LSTRW(RID_drcmd_read_disk_error), lasterr.c_str());
		log(drcmd_read_disk, 0, m_err);
	}
_ERROR:
	if (buf) {
		m_buffers_reader.Push(buf);
	}
	StopThreadWriter();
	return false;
}

bool CRequestHandler::cmd_close_disk(int reqlen)
{
	if (reqlen != 0) {
		return false;
	}
	log(drcmd_close_disk, 0, LSTRW(RID_drcmd_close_disk));
	if (m_hOpenedDisk != INVALID_HANDLE_VALUE) {
		CloseHandle(m_hOpenedDisk);
		m_hOpenedDisk = INVALID_HANDLE_VALUE;
	}
	json j = {
		{"code", 0},
		{"msg", "ok"}
	};
	std::string result;
	result = j.dump(2);
	return reply(drcmd_close_disk, 0, drDT_json, &result[0], (int)result.size());
}

bool CRequestHandler::cmd_open_volume(int reqlen)
{
	drcmd_open_volume_request open;
	int ret, nRecvd;

	if (reqlen == 0 || reqlen > sizeof(open)) {
		return false;
	}

	memset(&open, 0, sizeof(open));
	ret = RecvBlock(m_ptcp, &open, reqlen, &nRecvd, REQUEST_TIMEOUT_SEC);
	if (ret <= 0) {
		return false;
	}

	json j = {
	{"code", 0},
	{"msg", ""}
	};
	std::string result;
	if (m_pVR) {
		delete m_pVR;
		m_pVR = NULL;
	}
	m_pVR = new CVolumeReader();
	m_pVR->setOptions(open.dwOptions);
	if (!m_pVR->setSource(CA2W(open.name), open.size, FALSE)) {
		j["code"] = GetLastError();
		j["msg"] = (LPCSTR)_CW2UTF8(std::wstring(LSTRW(RID_open_vol_failed)));
		delete m_pVR;
		m_pVR = NULL;
		result = j.dump(2);
		log(drcmd_open_volume, 0, LSTRW(RID_open_vol_failed));
		return reply(drcmd_open_volume, drerr_general, drDT_json, &result[0], (int)result.size());
	}
	else
	{
		if (open.pos) {
			if (!m_pVR->SetPos(open.pos)) {
				j["code"] = GetLastError();
				j["msg"] = (LPCSTR)_CW2UTF8(std::wstring(LSTRW(RID_volume_seek_failed)));
				delete m_pVR;
				m_pVR = NULL;
				result = j.dump(2);
				log(drcmd_open_volume, 0, LSTRW(RID_volume_seek_failed));
				return reply(drcmd_open_volume, drerr_general, drDT_json, &result[0], (int)result.size());
			}
			m_vol_pos = open.pos;
		}
		m_err.Format(LSTRW(RID_volume_open_at), open.name, m_vol_pos);
		log(drcmd_open_volume, 0, m_err);
		j["msg"] = "ok";
		uint64_t calcSz = 0;
		if (m_pVR->CalcDataSize(&calcSz)) {
			j["CalcDataSize"] = calcSz;
		}
		result = j.dump(2);
		return reply(drcmd_open_volume, 0, drDT_json, &result[0], (int)result.size());
	}
}

bool CRequestHandler::cmd_read_volume(int reqlen)
{
	drcmd_read_request readreq;
	int ret, nRecvd;

	if (reqlen != sizeof(readreq)) {
		return false;
	}
	if (!m_pVR) {
		return false;
	}

	memset(&readreq, 0, sizeof(readreq));
	ret = RecvBlock(m_ptcp, &readreq, reqlen, &nRecvd, REQUEST_TIMEOUT_SEC);
	if (ret <= 0) {
		return false;
	}
	json j = {
	{"code", 0},
	{"msg", ""}
	};
	std::string result;
	result = j.dump(2);
	if (!reply(drcmd_read_volume, 0, drDT_json, &result[0], (int)result.size())) {
		return false;
	}
	m_err.Format(LSTRW(RID_volume_read));
	log(drcmd_read_volume, 0, m_err);

	m_writer_error = 0;
	m_buffers_reader.clear();
	m_buffers_reader.init_pool(buf_reserved_head_space + (DWORD)ZSTD_compressBound(read_chunk_maxsize), 4);
	m_hThreadWriter = CreateThread(0, 0, sThreadWriter, this, 0, 0);

	uint64_t sent = 0;
	uint64_t bytesRead;
	DWORD dwChunk;
	BOOL isFreeSpace;
	CBuffers::Buffer* buf = NULL;
	char* pBuf_diskdata;
	size_t zret;
	unsigned iserr;
	while (sent < readreq.size) {
		if (_ab.aborted()) {
			return false;
		}
		buf = m_buffers_reader.Pop(INFINITE);
		if (!buf || m_FlagExit) {
			goto _ERROR;
		}

		pBuf_diskdata = (char*)(buf->p) + buf_reserved_head_space;
		dwChunk = (DWORD) min(read_chunk_maxsize, readreq.size - sent);

		if (ENABLE_ZSTD) {
			m_zbuf.reserve(dwChunk);
			ret = m_pVR->Read(&m_zbuf[0], dwChunk, &bytesRead, &isFreeSpace);
			if (!ret) {
				goto _READISSUE;
			}
			if (!bytesRead) {
				if (!reply_diskdata(drcmd_read_volume, 0, drDT_diskdata, 0, buf)) {
					goto _ERROR;
				}
				break;
			}
			if (isFreeSpace) {
				if (!reply_diskdata(drcmd_read_volume, 0, drDT_diskdata | drDT_zeroblock, bytesRead, buf)) {
					goto _ERROR;
				}
			}
			else {
				zret = ZSTD_compress(pBuf_diskdata, ZSTD_compressBound(read_chunk_maxsize), &m_zbuf[0], (size_t)bytesRead, ZSTD_CLEVEL);
				iserr = ZSTD_isError(zret);
				if (iserr)
					goto _ERROR;
				if (!reply_diskdata(drcmd_read_volume, 0, drDT_diskdata | drDT_zstd, zret, buf)) {
					goto _ERROR;
				}
			}
		}
		else {
			ret = m_pVR->Read(pBuf_diskdata, dwChunk, &bytesRead, &isFreeSpace);
			if (!ret) {
				goto _READISSUE;
			}
			if (!bytesRead) {
				if (!reply_diskdata(drcmd_read_volume, 0, drDT_diskdata, 0, buf)) {
					goto _ERROR;
				}
				break;
			}
			if (isFreeSpace) {
				if (!reply_diskdata(drcmd_read_volume, 0, drDT_diskdata | drDT_zeroblock, bytesRead, buf)) {
					goto _ERROR;
				}
			}
			else {
				if (!reply_diskdata(drcmd_read_volume, 0, drDT_diskdata, bytesRead, buf)) {
					goto _ERROR;
				}
			}
		}
		m_vol_pos += bytesRead;
		sent += bytesRead;
	}
	m_buffers_data2write.Abort(FALSE);
	StopThreadWriter();
	return true;
_READISSUE:
	{
		std::wstring lasterr = GetErrorAsString(::GetLastError());
		StopThreadWriter();
		json j = { {"code", GetLastError()}, {"msg", ""} };
		result = j.dump(2);
		reply(drcmd_read_volume, drerr_general, drDT_json, &result[0], result.size());
	}
_ERROR:
	if (buf) {
		m_buffers_reader.Push(buf);
	}
	StopThreadWriter();
	return false;
}

bool CRequestHandler::cmd_close_volume(int reqlen)
{
	if (reqlen != 0) {
		return false;
	}
	m_err.Format(LSTRW(RID_volume_close));
	log(drcmd_close_volume, 0, m_err);
	if (m_pVR) {
		delete m_pVR;
		m_pVR = NULL;
	}
	json j = {
		{"code", 0},
		{"msg", "ok"}
	};
	std::string result;
	result = j.dump(2);
	return reply(drcmd_close_volume, 0, drDT_json, &result[0], (int)result.size());
}


///////////////////////////////////////

CServer::CServer()
{
	m_listen = INVALID_SOCKET;
	m_FlagExit = FALSE;
	m_single_session_Only = FALSE;
	m_hExitEvent = CreateEvent(0, TRUE, 0, 0);
	m_hCleanEvent = CreateEvent(0, 0, 0, 0);
	memset(m_passwd, 0, sizeof(m_passwd));
	m_id = 0;
}

CServer::~CServer()
{
	Close();
}

BOOL CServer::Init(const char* bindIP, int port, const char* passwd)
{
	strcpy_s(m_passwd, passwd);
	if (!bindIP || !bindIP[0])
		bindIP = "0.0.0.0";
	m_listen = socket_createListener(inet_addr(bindIP), port, 32, false, true);
	if (m_listen == INVALID_SOCKET) {
		return FALSE;
	}
	CThread::Start();
	return TRUE;
}

void CServer::Close()
{
	m_FlagExit = TRUE;
	if (m_hExitEvent) {
		SetEvent(m_hExitEvent);
	}
	CThread::Wait();
	if (m_listen != INVALID_SOCKET) {
		closesocket(m_listen);
		m_listen = INVALID_SOCKET;
	}
	if (m_hExitEvent) {
		CloseHandle(m_hExitEvent);
		m_hExitEvent = NULL;
	}
	if (m_hCleanEvent) {
		CloseHandle(m_hCleanEvent);
		m_hCleanEvent = NULL;
	}
}

void CServer::OnRequestEnd(CRequestHandler* req)
{
	req->m_bEnd = TRUE;
	SetEvent(m_hCleanEvent);
}

void CServer::req_log_output(int cmd, int err, LPCWSTR lpszDetails)
{
}

void CServer::CleanRequests(BOOL all)
{
	for (std::list<CRequestHandler*>::iterator it = m_reqs.begin(); it != m_reqs.end(); ) {
		CRequestHandler* req = *it;
		if (req->m_bEnd) {
			it = m_reqs.erase(it);
			delete req;
		}
		else {
			if (all) {
				req->Close();
				it = m_reqs.erase(it);
				delete req;
			}
			else {
				it++;
			}
		}
	}
}

DWORD CServer::run()
{
	//int ret;
	SOCKET sClient;
	sockaddr_in addr;
	int addrlen;
	char fromip[128];
	HANDLE hWaits[3];
	HANDLE hwse = WSACreateEvent();
	DWORD dwEvents;
	WSAEventSelect(m_listen, hwse, FD_ACCEPT);

	while (!m_FlagExit) {
		hWaits[0] = m_hExitEvent;
		hWaits[1] = hwse;
		hWaits[2] = m_hCleanEvent;
		dwEvents = WSAWaitForMultipleEvents(_countof(hWaits), hWaits, FALSE, WSA_INFINITE, FALSE);
		if (dwEvents == WSA_WAIT_TIMEOUT) {
			continue;
		}
		else if (dwEvents == WSA_WAIT_EVENT_0) {
			break;
		}
		else if (dwEvents == WSA_WAIT_EVENT_0 + 1) {
			WSANETWORKEVENTS NetworkEvents;
			if (WSAEnumNetworkEvents(m_listen, hwse, &NetworkEvents) == SOCKET_ERROR) {
				break;
			}
			if (NetworkEvents.lNetworkEvents & FD_ACCEPT) {
				addrlen = sizeof(addr);
				sClient = WSAAccept(m_listen, (sockaddr*)&addr, &addrlen, NULL, 0);
				if (sClient == SOCKET_ERROR) {
					if (WSAGetLastError() == WSAEWOULDBLOCK)
						continue;
					break;
				}
				++m_id;
				memset(fromip, 0, sizeof(fromip));
				util_inet_ntop(addr.sin_family, &addr.sin_addr, fromip, _countof(fromip));
				if (!OnNewClientConnected(fromip, ntohs(addr.sin_port))) {
					closesocket(sClient);
					continue;
				}

				if (m_single_session_Only)
					CleanRequests(TRUE);

				CRequestHandler* req = new CRequestHandler();
				req->m_id = m_id;
				m_reqs.push_back(req);
				if (!req->Init(this, sClient, m_passwd)) {
					m_reqs.remove(req);
					delete req;
				}
			}
		}
		else if (dwEvents == WSA_WAIT_EVENT_0 + 2) {
			CleanRequests(FALSE);
		}

	}
	WSACloseEvent(hwse);
	CleanRequests(TRUE);
	return 0;
}
