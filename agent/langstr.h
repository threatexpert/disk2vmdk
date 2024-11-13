#pragma once



LPCWSTR LSTRW(int i);

enum {
	RID_UNK,
	RID_Session_Established,
	RID_Start,
	RID_Started,
	RID_Stop,
	RID_Stopped,
	RID_Notice,
	RID_SureExit,
	RID_StatupFailed,
	RID_ListenOn,
	RID_Notice_listen_localhost,
	RID_Notice_encrypted,
	RID_Notice_not_encrypted,
	RID_handshake_failed,
	RID_timeout,
	RID_over,
	RID_netword_aborted,
	RID_logpref,
	RID_query_pc_info,
	RID_drcmd_query_disk_info,
	RID_open_disk_failed,
	RID_seek_failed,
	RID_seek_at_failed,
	RID_seek_at_ok,
	RID_open_disk_seek_failed,
	RID_open_disk_ok,
	RID_drcmd_read_disk,
	RID_drcmd_read_disk_error,
	RID_drcmd_close_disk,
	RID_open_vol_failed,
	RID_volume_seek_failed,
	RID_volume_open_at,
	RID_volume_read,
	RID_volume_close,
	RID_EmptyPassword_WARNING,
};