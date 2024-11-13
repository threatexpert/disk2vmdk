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
			L"�Ự%d�����ӣ�%S:%d",
			L"Session %d connected, %S:%d"
		);
		casexx(
			RID_Start,
			L"����",
			L"Start"
		);
		casexx(
			RID_Started,
			L"������",
			L"Started"
		);
		casexx(
			RID_Stop,
			L"ֹͣ",
			L"Stop"
		);
		casexx(
			RID_Stopped,
			L"��ֹͣ.",
			L"Stopped."
		);
		casexx(
			RID_Notice,
			L"��ʾ",
			L"Notice"
		);
		casexx(
			RID_SureExit,
			L"ȷ��Ҫ�˳�?",
			L"Are you sure you want to exit?"
		);
		casexx(
			RID_StatupFailed,
			L"����ʧ��...",
			L"Startup failed"
		);
		casexx(
			RID_ListenOn,
			L"�˿ڼ��������µ�ַ:",
			L"The port listens on the following addresses:"
		);
		casexx(
			RID_Notice_listen_localhost,
			L"�˿ڽ������ڻ��ص�ַ�����Ҫ֧��Զ�̷��ʣ�������ǰ�뽫IP��ַ���ա�",
			L"The port only listens on the loopback address. If you want to support remote access, leave the IP address blank before starting."
		);
		casexx(
			RID_Notice_encrypted,
			L"�������������룬���紫�佫���ü���.",
			L"Since a password is set, network transmissions will be encrypted."
		);
		casexx(
			RID_Notice_not_encrypted,
			L"����û���������룬���紫�佫�������.",
			L"Since no password is set, network transmissions will not be encrypted."
		);
		casexx(
			RID_handshake_failed,
			L"Э���쳣���������벻ƥ��",
			L"Handshake failed, maybe the password does not match"
		);
		casexx(
			RID_timeout,
			L"��ʱ",
			L"time out"
		);
		casexx(
			RID_over,
			L"����",
			L"over"
		);
		casexx(
			RID_netword_aborted,
			L"�����ж�",
			L"network aborted"
		);
		casexx(
			RID_logpref,
			L"%s [�Ự %d] %s",
			L"%s [Session %d] %s"
		);
		casexx(
			RID_query_pc_info,
			L"��ѯ������Ϣ",
			L"Query host information"
		);
		casexx(
			RID_drcmd_query_disk_info,
			L"��ѯ������Ϣ",
			L"Query disk information"
		);
		casexx(
			RID_open_disk_failed,
			L"�򿪴���%Sʧ��, %s",
			L"Failed to open disk %S, %s"
		);
		casexx(
			RID_seek_failed,
			L"��������ָ��ʧ��",
			L"Failed to adjust disk pointer"
		);
		casexx(
			RID_seek_at_failed,
			L"��������ָ��ʧ��. @ %I64d",
			L"Failed to adjust disk pointer. @ %I64d"
		);
		casexx(
			RID_seek_at_ok,
			L"��������ָ��ɹ�. @ %I64d",
			L"Disk pointer adjustment successful. @ %I64d"
		);
		casexx(
			RID_open_disk_seek_failed,
			L"�򿪴��̺����ָ��ʧ��",
			L"Failed to adjust pointer after opening disk"
		);
		casexx(
			RID_open_disk_ok,
			L"�򿪴���%S�ɹ�",
			L"Opening disk %S successfully"
		);
		casexx(
			RID_drcmd_read_disk,
			L"��ʼ��ȡ��������...",
			L"Starting to read disk data..."
		);
		casexx(
			RID_drcmd_read_disk_error,
			L"��ȡ��������ʧ��, %s",
			L"Failed to read disk data, %s"
		);
		casexx(
			RID_drcmd_close_disk,
			L"�رմ���.",
			L"Close the disk."
		);
		casexx(
			RID_open_vol_failed,
			L"�򿪷���ʧ��.",
			L"Failed to open volume."
		);
		casexx(
			RID_volume_seek_failed,
			L"��������ָ��ʧ��",
			L"Failed to adjust partition pointer"
		);
		casexx(
			RID_volume_open_at,
			L"�򿪷��� %S, ָ��@ %I64d",
			L"open volume @ %I64d"
		);
		casexx(
			RID_volume_read,
			L"��ʼ��ȡ��������...",
			L"Starting to read partition data..."
		);
		casexx(
			RID_volume_close,
			L"�رշ���",
			L"close volume"
		);
		casexx(
			RID_EmptyPassword_WARNING,
			L"��û���������룬���ܱ������˷��ʵ����̵��������ݣ�ȷ��Ҫ������",
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
