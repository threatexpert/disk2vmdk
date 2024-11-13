#include "pch.h"
#include "Cconnection.h"
#include <string>
#include <atlstr.h>
#include <atlconv.h>
#include "../misc/netutils.h"
#include "../misc/SecureLayer.h"
#include "../core/json.hpp"
using json = nlohmann::ordered_json;
#include "../agent/idata.h"
#include "langstr.h"

static BOOL gInitWinsock = FALSE;

bool issyserror(int err)
{
	if (err <= -4000 && err > -5000)
		return true;
	return false;
}

bool isneterror(int err)
{
	if (err <= -1000 && err > -2000)
		return true;
	return false;
}

Cconnection::Cconnection()
{
	if (!gInitWinsock) {
		socket_init_ws32();
		gInitWinsock = TRUE;
	}
	m_FlagExit = FALSE;
	_pflag = &m_FlagExit;
	m_conn = NULL;
	m_port = 0;
	last_errcode = 0;
	m_pCB = NULL;
	m_retry_count = 0;
}

Cconnection::~Cconnection()
{
	Close();
}

void Cconnection::SetCallback(IConnectionCallback* pCB)
{
	m_pCB = pCB;
}

void Cconnection::Close()
{
	if (m_conn) {
		m_conn->Dereference();
		m_conn = NULL;
	}
}

bool Cconnection::Retry()
{
	m_retry_count++;
	if (m_retry_count > 1) {
		Sleep(1000);
	}
	while (!*_pflag) {
		Close();
		LogFmt(LSTRW(RID_ConnectingHostPort), m_ip.c_str(), m_port);
		if (Connect()) {
			return true;
		}
		Log(LSTRW(RID_Failed_retry_5_seconds_later));
		for (int i = 0; !*_pflag && i < 5; i++) {
			Sleep(1000);
		}
	}
	return false;
}

void Cconnection::setTarget(LPCTSTR lpszIP, int port, LPCTSTR lpszPasswd)
{
	m_ip = CW2A(lpszIP);
	m_port = port;
	m_passwd = CW2A(lpszPasswd);
}

bool Cconnection::Connect()
{
	if (m_conn) {
		assert(false);
		return false;
	}
	m_conn = new CInetTcp();
	m_conn->SetAborter(this);
	LogFmt(LSTRW(RID_ConnectingHostPort), m_ip.c_str(), m_port);
	if (!m_conn->Connect(m_ip.c_str(), m_port, 15)) {
		return false;
	}
	if (m_passwd.size()) {
		CSecLayer* secLayer = new CSecLayer();
		if (!m_conn->InsertSPI(secLayer)) {
			secLayer->Dereference();
			return false;
		}
		if (0 != secLayer->hello(m_passwd.c_str())) {
			secLayer->Dereference();
			return false;
		}
		secLayer->Dereference();
	}
	socket_setbufsize(m_conn->GetSocketHandle(), 1024 * 256);
	if (!InetWriteAll(m_conn, IAMREADY, SIZEOF_IAMREADY, 15))
		return false;
	char tmp[SIZEOF_IAMREADY];
	if (!InetReadAll(m_conn, tmp, SIZEOF_IAMREADY, 15)) {
		return false;
	}
	if (memcmp(tmp, IAMREADY, SIZEOF_IAMREADY) != 0) {
		return false;
	}
	return true;
}

void Cconnection::Abort()
{
	*_pflag = TRUE;
}

int Cconnection::GetPCInfo(std::string& result)
{
	Log(LSTRW(RID_drcmd_query_pc_info));
	return RequestJ(drcmd_query_pc_info, 0, 0, result, 1024 * 1024 * 16);
}

int Cconnection::GetDiskInfo(std::string& result)
{
	Log(LSTRW(RID_drcmd_query_disk_info));
	return RequestJ(drcmd_query_disk_info, 0, 0, result, 1024 * 1024 * 16);
}

int Cconnection::Quit()
{
	drcmd_hdr hdr;
	hdr.cmd = drcmd_quit;
	hdr.datalen = 0;

	if (!InetWriteAll(m_conn, &hdr, sizeof(hdr), 15)) {
		return -1001;
	}
	m_conn->Close();
	return 0;
}

int Cconnection::OpenDisk(LPCTSTR lpszName)
{
	drcmd_open_disk_request req;
	req.pos = 0;
	strcpy_s(req.name, CW2A(lpszName));
	std::string result;
	LogFmt(LSTRW(RID_drcmd_open_disk), lpszName);
	return RequestJ(drcmd_open_disk, &req, sizeof(req), result);
}

int Cconnection::SeekDisk(uint64_t pos)
{
	drcmd_seek_request req;
	req.pos = pos;

	std::string result;
	Log(LSTRW(RID_drcmd_seek_disk));
	return RequestJ(drcmd_seek_disk, &req, sizeof(req), result);
}

int Cconnection::ReadDisk(uint64_t size)
{
	drcmd_read_request req;
	req.size = size;

	std::string result;
	Log(LSTRW(RID_drcmd_read_disk));
	return RequestJ(drcmd_read_disk, &req, sizeof(req), result);
}

int Cconnection::ReadDiskStart(uint64_t pos, uint64_t size)
{
	int ret;
	ret = SeekDisk(pos);
	if (ret != 0)
		return ret;
	ret = ReadDisk(size);
	if (ret != 0)
		return ret;
	return 0;
}

int Cconnection::ReadDiskData(void* buf, uint32_t size, DWORD* pBytesRead)
{
	*pBytesRead = 0;
	drcmd_result res;
	if (!InetReadAll(m_conn, &res, sizeof(res), 120)) {
		return -1001;
	}
	if (res.cmd != drcmd_read_disk) {
		return -2001;
	}
	if (res.err != 0) {
		if (res.datatype != drDT_json) {
			return -2001;
		}
		if (res.datalen > read_chunk_maxsize) {
			return -2001;
		}
		if (!res.datalen) {
			SetLastError(ERROR_READ_FAULT);
			return -4001;
		}
		std::string errjs;
		errjs.reserve((size_t)res.datalen);
		errjs.resize((size_t)res.datalen);
		if (!InetReadAll(m_conn, &errjs[0], (int)res.datalen, 30)) {
			return -1001;
		}
		if (!parsejson_errmsg(errjs)) {
			return -2001;
		}
		SetLastError(last_errcode);
		return -4001;
	}

	if (!(res.datatype & drDT_diskdata)) {
		return -2001;
	}
	if (res.datalen) {
		if (res.datatype & drDT_zstd) {
			if (res.datalen > recv_chunk_maxsize) {
				return -2001;
			}
			m_zbuf.reserve((size_t)res.datalen);
			if (!InetReadAll(m_conn, &m_zbuf[0], (int)res.datalen, 120)) {
				return -1001;
			}
			unsigned long long srclen = ZSTD_getDecompressedSize(&m_zbuf[0], (size_t)res.datalen);
			if (!srclen || srclen > size) {
				return -2001;
			}
			size_t zret = ZSTD_decompress(buf, size, &m_zbuf[0], (size_t)res.datalen);
			unsigned iserr = ZSTD_isError(zret);
			if (iserr) {
				return -2001;
			}
			if (size < zret) {
				return -2001;
			}
			*pBytesRead = (DWORD)zret;
		}
		else {
			if (size < res.datalen) {
				assert(size >= res.datalen);
				return -4001;
			}
			if (!InetReadAll(m_conn, buf, (int)res.datalen, 120)) {
				return -1001;
			}
			*pBytesRead = (uint32_t)res.datalen;
		}

	}
	else {
		*pBytesRead = 0;
	}
	return 0;
}

int Cconnection::CloseDisk()
{
	std::string result;
	return RequestJ(drcmd_close_disk, 0, 0, result);
}

int Cconnection::OpenVolume(LPCTSTR lpszName, DWORD dwOptions, uint64_t pos, uint64_t size, uint64_t *pCalcDataSize)
{
	Log(LSTRW(RID_drcmd_open_volume));
	drcmd_open_volume_request req;
	req.size = size;
	req.dwOptions = dwOptions;
	req.pos = pos;
	strcpy_s(req.name, CW2A(lpszName));
	std::string result;
	int ret = RequestJ(drcmd_open_volume, &req, sizeof(req), result);
	if (ret == 0) {
		try {
			json j = json::parse(result);
			if (j.contains("CalcDataSize")) {
				*pCalcDataSize = j["CalcDataSize"].get<uint64_t>();
			}
		}
		catch (...) {

		}
	}
	return ret;
}

int Cconnection::ReadVolume(uint64_t size)
{
	Log(LSTRW(RID_drcmd_read_volume));
	drcmd_read_volume_request req;
	req.size = size;
	std::string result;
	return RequestJ(drcmd_read_volume, &req, sizeof(req), result);
}

int Cconnection::ReadVolumeData(void* buf, uint32_t size, uint64_t* pBytesRead, BOOL* pIsFreeSpace)
{
	*pBytesRead = 0;
	*pIsFreeSpace = 0;
	drcmd_result res;
	if (!InetReadAll(m_conn, &res, sizeof(res), 120)) {
		return -1001;
	}
	if (res.cmd != drcmd_read_volume) {
		return -2001;
	}
	if (res.err != 0) {
		if (res.datatype != drDT_json) {
			return -2001;
		}
		if (res.datalen > read_chunk_maxsize) {
			return -2001;
		}
		if (!res.datalen) {
			SetLastError(ERROR_READ_FAULT);
			return -4001;
		}
		std::string errjs;
		errjs.reserve((size_t)res.datalen);
		errjs.resize((size_t)res.datalen);
		if (!InetReadAll(m_conn, &errjs[0], (int)res.datalen, 30)) {
			return -1001;
		}
		if (!parsejson_errmsg(errjs)) {
			return -2001;
		}
		SetLastError(last_errcode);
		return -4001;
	}
	if (!(res.datatype & drDT_diskdata)) {
		return -2001;
	}
	if (res.datatype & drDT_zeroblock) {
		*pBytesRead = res.datalen;
		*pIsFreeSpace = TRUE;
		memset(buf, 0, size);
	}
	else {
		if (res.datalen) {
			if (res.datatype & drDT_zstd) {
				if (res.datalen > recv_chunk_maxsize) {
					return -2001;
				}
				m_zbuf.reserve((size_t)res.datalen);
				if (!InetReadAll(m_conn, &m_zbuf[0], (int)res.datalen, 120)) {
					return -1001;
				}
				unsigned long long srclen = ZSTD_getDecompressedSize(&m_zbuf[0], (size_t)res.datalen);
				if (!srclen || srclen > size) {
					return -2001;
				}
				size_t zret = ZSTD_decompress(buf, size, &m_zbuf[0], (size_t)res.datalen);
				unsigned iserr = ZSTD_isError(zret);
				if (iserr) {
					return -2001;
				}
				if (size < zret) {
					return -2001;
				}
				*pBytesRead = (DWORD)zret;
			}
			else {
				if (res.datalen > read_chunk_maxsize) {
					return -2001;
				}
				if (size < res.datalen) {
					assert(size >= res.datalen);
					return -4001;
				}
				if (!InetReadAll(m_conn, buf, (int)res.datalen, 120)) {
					return -1001;
				}
				*pBytesRead = res.datalen;
			}
		}
		else {
			*pBytesRead = 0;
		}
		*pIsFreeSpace = FALSE;
	}
	return 0;
}

int Cconnection::CloseVolume()
{
	std::string result;
	return RequestJ(drcmd_close_volume, 0, 0, result);
}

int Cconnection::RequestJ(uint8_t cmd, const void* req, int reqlen, std::string& resultJ, int limitresultsize)
{
	drcmd_hdr hdr;
	hdr.cmd = cmd;
	hdr.datalen = reqlen;

	if (!InetWriteAll(m_conn, &hdr, sizeof(hdr), 15)) {
		return -1001;
	}
	if (req && reqlen) {
		if (!InetWriteAll(m_conn, req, reqlen, 15)) {
			return -1001;
		}
	}

	drcmd_result res;
	if (!InetReadAll(m_conn, &res, sizeof(res), 30)) {
		return -1001;
	}
	if (res.cmd != cmd || res.datatype != drDT_json) {
		return -2001;
	}
	if (limitresultsize != 0) {
		if (res.datalen > limitresultsize) {
			return -2001;
		}
	}
	if (res.datalen) {
		resultJ.reserve((size_t)res.datalen);
		resultJ.resize((size_t)res.datalen);
		if (!InetReadAll(m_conn, &resultJ[0], (int)res.datalen, 30)) {
			return -1001;
		}
		if (cmd != drcmd_query_pc_info && cmd != drcmd_query_disk_info) {
			if (!parsejson_errmsg(resultJ)) {
				return -2001;
			}
			SetLastError(last_errcode);
		}
	}
	if (res.err != 0) {
		return -4001;
	}

	return 0;
}

bool Cconnection::parsejson_errmsg(std::string& result)
{
	last_errmsg.clear();
	last_errcode = -1;
	try {
		json j = json::parse(result);
		if (j.contains("code") && j.contains("msg")) {
			last_errmsg = CA2W(j["msg"].get<std::string>().c_str(), CP_UTF8);
			last_errcode = j["code"].get<DWORD>();
			return true;
		}
	}
	catch (...) {

	}
	return false;
}

void Cconnection::Log(LPCTSTR lpszText)
{
	if (m_pCB) {
		m_pCB->OnConnLog(lpszText);
	}
}

void Cconnection::LogFmt(LPCTSTR fmt, ...)
{
	WCHAR buf[1024];
	va_list ap;
	va_start(ap, fmt);
	_vsnwprintf(buf, _countof(buf) - 3, fmt, ap);
	va_end(ap);
	if (m_pCB) {
		m_pCB->OnConnLog(buf);
	}
}