#include "pch.h"
#include <assert.h>
#include "langstr.h"

const int default_lang = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);

#define strxx(dl, en) {\
    if (lid == default_lang) { return dl; }\
    else { return en; }\
}

#define casexx(i, dl, en) \
case i:\
strxx(\
dl, \
en)

LPCWSTR LSTRW_X(int i, int lid)
{
	switch (i) {
		casexx(
			RID_Session_Established,
			L"会话%d已连接，%S:%d",
			L"Session %d connected, %S:%d"
		);
		casexx(
			RID_Start,
			L"启动",
			L"Start"
		);
		casexx(
			RID_Started,
			L"已启动",
			L"Started"
		);
		casexx(
			RID_Stop,
			L"停止",
			L"Stop"
		);
		casexx(
			RID_Stopped,
			L"已停止.",
			L"Stopped."
		);
		casexx(
			RID_Notice,
			L"提示",
			L"Notice"
		);
		casexx(
			RID_SureExit,
			L"确定要退出?",
			L"Are you sure you want to exit?"
		);
		casexx(
			RID_StatupFailed,
			L"启动失败...",
			L"Startup failed"
		);
		casexx(
			RID_ListenOn,
			L"端口监听在以下地址:",
			L"The port listens on the following addresses:"
		);
		casexx(
			RID_Notice_listen_localhost,
			L"端口仅监听在环回地址，如果要支持远程访问，在启动前请将IP地址留空。",
			L"The port only listens on the loopback address. If you want to support remote access, leave the IP address blank before starting."
		);
		casexx(
			RID_Notice_encrypted,
			L"由于设置了密码，网络传输将启用加密.",
			L"Since a password is set, network transmissions will be encrypted."
		);
		casexx(
			RID_Notice_not_encrypted,
			L"由于没有设置密码，网络传输将不会加密.",
			L"Since no password is set, network transmissions will not be encrypted."
		);
		casexx(
			RID_handshake_failed,
			L"协议异常，可能密码不匹配",
			L"Handshake failed, maybe the password does not match"
		);
		casexx(
			RID_timeout,
			L"超时",
			L"time out"
		);
		casexx(
			RID_over,
			L"结束",
			L"over"
		);
		casexx(
			RID_netword_aborted,
			L"网络中断",
			L"network aborted"
		);
		casexx(
			RID_logpref,
			L"%s [会话 %d] %s",
			L"%s [Session %d] %s"
		);
		casexx(
			RID_query_pc_info,
			L"查询主机信息",
			L"Query host information"
		);
		casexx(
			RID_drcmd_query_disk_info,
			L"查询磁盘信息",
			L"Query disk information"
		);
		casexx(
			RID_open_disk_failed,
			L"打开磁盘%S失败, %s",
			L"Failed to open disk %S, %s"
		);
		casexx(
			RID_seek_failed,
			L"调整磁盘指针失败",
			L"Failed to adjust disk pointer"
		);
		casexx(
			RID_seek_at_failed,
			L"调整磁盘指针失败. @ %I64d",
			L"Failed to adjust disk pointer. @ %I64d"
		);
		casexx(
			RID_seek_at_ok,
			L"调整磁盘指针成功. @ %I64d",
			L"Disk pointer adjustment successful. @ %I64d"
		);
		casexx(
			RID_open_disk_seek_failed,
			L"打开磁盘后调整指针失败",
			L"Failed to adjust pointer after opening disk"
		);
		casexx(
			RID_open_disk_ok,
			L"打开磁盘%S成功",
			L"Opening disk %S successfully"
		);
		casexx(
			RID_drcmd_read_disk,
			L"开始读取磁盘数据...",
			L"Starting to read disk data..."
		);
		casexx(
			RID_drcmd_read_disk_error,
			L"读取磁盘数据失败, %s",
			L"Failed to read disk data, %s"
		);
		casexx(
			RID_drcmd_close_disk,
			L"关闭磁盘.",
			L"Close the disk."
		);
		casexx(
			RID_open_vol_failed,
			L"打开分区失败.",
			L"Failed to open volume."
		);
		casexx(
			RID_volume_seek_failed,
			L"调整分区指针失败",
			L"Failed to adjust partition pointer"
		);
		casexx(
			RID_volume_open_at,
			L"打开分区 %S, 指针@ %I64d",
			L"open volume @ %I64d"
		);
		casexx(
			RID_volume_read,
			L"开始读取分区数据...",
			L"Starting to read partition data..."
		);
		casexx(
			RID_volume_close,
			L"关闭分区",
			L"close volume"
		);
		casexx(
			RID_EmptyPassword_WARNING,
			L"你没有设置密码，可能被其他人访问到磁盘的所有数据，确定要继续吗？",
			L"You have not set a password. All data on the disk may be accessed by others. Are you sure you want to continue?"
		);


	default:
		assert(false);
	}
	return L"";
}

LPCWSTR LSTRW(int i)
{
	LANGID lid = GetUserDefaultUILanguage();
	return LSTRW_X(i, lid);
}
