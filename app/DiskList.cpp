// DiskList.cpp: 实现文件
//

#include "pch.h"
#include "MFCdisk2vmdk.h"
#include "DiskList.h"
#include "CPartDlg.h"
#include "CConnDlg.h"
#include "GetPCInfo.h"

#define ClassT CDiskList
ClassT::_tagLVHeader ClassT::m_hdrs[ClassT::LVM_COLUMNCOUNT] =
{
	RID_DiskNum, 100,
	RID_Lable, 260,
	RID_BusType, 100,
	RID_Size, 200,
	RID_OtherInfo, 200
};

#define MYMSG_DISKS (WM_APP + 100)

#define NOTIFY_ERROR      1
#define NOTIFY_DONE       2
#define NOTIFY_PROGRESS   3

// CDiskList

IMPLEMENT_DYNAMIC(CDiskList, CListCtrl)

CDiskList::CDiskList()
	: m_bLocked(FALSE)
	, m_pAsyncDisks(NULL)
	, m_hParentWnd(NULL)
	, m_cbMsg(0)
	, m_bRemoteMode(FALSE)
{

}

CDiskList::~CDiskList()
{
	if (m_pAsyncDisks)
		delete m_pAsyncDisks;
}

BEGIN_MESSAGE_MAP(CDiskList, CListCtrl)
	ON_WM_CONTEXTMENU()
	ON_COMMAND(ID_DISK_FRESH, &CDiskList::OnDiskFresh)
	ON_MESSAGE(MYMSG_DISKS, &CDiskList::OnGetDisks)
	ON_NOTIFY_REFLECT(LVN_ITEMCHANGING, &CDiskList::OnLvnItemchanging)
	ON_COMMAND(ID_DISK_REMOTE, &CDiskList::OnDiskRemote)
END_MESSAGE_MAP()



// CDiskList 消息处理程序




BOOL CDiskList::Init(HWND hParentWnd, UINT cbMsg)
{
	int n;

	ModifyStyle(0, LVS_SHOWSELALWAYS);
	SetExtendedStyle(GetExtendedStyle() | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP);

	for (n = 0; n < LVM_COLUMNCOUNT; n++)
	{
		LVCOLUMN lv;
		TCHAR szHeader[64];
		_tcscpy_s(szHeader, LSTRW(m_hdrs[n].HeaderRid));

		lv.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
		lv.iSubItem = 0;
		lv.cx = m_hdrs[n].nWidth;
		lv.pszText = szHeader;

		InsertColumn(n, &lv);
	}
	m_hParentWnd = hParentWnd;
	m_cbMsg = cbMsg;
	FreshLocalDiskList();
	return 0;
}

LRESULT CDiskList::OnGetDisks(WPARAM wParam, LPARAM lParam)
{
	CStringW strStatus;
	if (wParam == NOTIFY_PROGRESS) {
		strStatus = (WCHAR*)lParam;
		free((WCHAR*)lParam);
		if (m_cbMsg) {
			::SendMessage(m_hParentWnd, m_cbMsg, 2, (LPARAM)(LPCWSTR)strStatus);
		}
	}

	if (m_pAsyncDisks) {
		if (m_pAsyncDisks->m_bRemoteMode) {
			if (wParam == NOTIFY_DONE) {
				try {
					m_bRemoteMode = TRUE;
					m_pcinfo = json::parse(m_pAsyncDisks->_pcinfo);
					m_jdisks = json::parse(m_pAsyncDisks->_result);
					for (auto & disk : m_jdisks) {
						disk["__remote_disk"] = true;
						disk["__pcinfo"] = m_pcinfo;
					}

					int nItemCount = InitDiskList();

					if (m_cbMsg) {
						if (nItemCount)
							::SendMessage(m_hParentWnd, m_cbMsg, 1, (LPARAM)LSTRW(RID_Disk_Remotely_Tip1));
						else
							::SendMessage(m_hParentWnd, m_cbMsg, 1, (LPARAM)LSTRW(RID_Disk_Remotely_NoDisks));
					}
				}
				catch (...) {
					::SendMessage(m_hParentWnd, m_cbMsg, 1, (LPARAM)LSTRW(RID_Disk_Remotely_Unexpect));
				}
				delete m_pAsyncDisks;
				m_pAsyncDisks = NULL;
			}
			else if (wParam == NOTIFY_ERROR) {
				delete m_pAsyncDisks;
				m_pAsyncDisks = NULL;
				DeleteAllItems();
				if (m_cbMsg) {
					::SendMessage(m_hParentWnd, m_cbMsg, 1, (LPARAM)LSTRW(RID_Disk_Remotely_Error));
				}
			}
		}
		else {
			try {
				m_pcinfo = json::parse(m_pAsyncDisks->_pcinfo);
				m_jdisks = json::parse(m_pAsyncDisks->_result);
				for (auto& disk : m_jdisks) {
					disk["__pcinfo"] = m_pcinfo;
				}
			}
			catch (...) {

			}
			delete m_pAsyncDisks;
			m_pAsyncDisks = NULL;
			m_bRemoteMode = FALSE;
			int nItemCount = InitDiskList();

			if (m_cbMsg) {
				if (nItemCount)
					::SendMessage(m_hParentWnd, m_cbMsg, 1, (LPARAM)LSTRW(RID_Disk_Locally_Tip1));
				else
					::SendMessage(m_hParentWnd, m_cbMsg, 1, (LPARAM)LSTRW(RID_Disk_Locally_NoDisks));
			}
		}
	}
	return 0;
}

int CDiskList::FreshLocalDiskList()
{
	if (!m_pAsyncDisks) {
		DeleteAllItems();
		m_jdisks.clear();
		m_pcinfo.clear();

		if (m_cbMsg) {
			::SendMessage(m_hParentWnd, m_cbMsg, 0, (LPARAM)LSTRW(RID_Disk_GetList));
		}
		m_pAsyncDisks = new CAsyncDisks();
		if (!m_pAsyncDisks->Start(m_hWnd, MYMSG_DISKS)) {
			delete m_pAsyncDisks;
			m_pAsyncDisks = NULL;
			if (m_cbMsg) {
				::SendMessage(m_hParentWnd, m_cbMsg, 1, (LPARAM)LSTRW(RID_Disk_GetListFailed));
			}
			return -1;
		}
		return 1;
	}
	return 0;
}

int CDiskList::QueryRemoteDiskList()
{
	if (!m_pAsyncDisks) {
		DeleteAllItems();
		m_jdisks.clear();
		m_pcinfo.clear();

		if (m_cbMsg) {
			::SendMessage(m_hParentWnd, m_cbMsg, 0, (LPARAM)LSTRW(RID_Disk_RemotelyGetList));
		}
		m_pAsyncDisks = new CAsyncDisks();
		if (!m_pAsyncDisks->StartRemote(m_hWnd, MYMSG_DISKS, CConnDlg::strIP, CConnDlg::port, CConnDlg::strPasswd)) {
			delete m_pAsyncDisks;
			m_pAsyncDisks = NULL;
			if (m_cbMsg) {
				::SendMessage(m_hParentWnd, m_cbMsg, 1, (LPARAM)LSTRW(RID_Disk_RemotelyGetListFailed));
			}
			return -1;
		}
		return 1;
	}
	return 0;
}

int CDiskList::InitDiskList()
{
	DeleteAllItems();
	int i = 0;
	char buf[256];

	for (i = 0; i < (int)m_jdisks.size(); i++)
	{
		json disk = m_jdisks[i];
		json property = disk["property"];
		int index = disk["index"];
		std::string Index = std::to_string(index);

		std::string Caption = property["VendorId"].get<std::string>() + " " + property["ProductId"].get<std::string>();
		while (Caption.size() && *Caption.begin() == ' ') {
			Caption.erase(Caption.begin());
		}
		std::string BusType = property["BusType"].get<std::string>();
		uint64_t DiskSize = property["DiskSize"];
		std::string strDiskSize = std::to_string(DiskSize);
		std::string strDiskSizeH = calculateSize1000(DiskSize, buf, sizeof(buf));

		CStringW wstrIndex, wstrCaption, wstrBusType, wstrSize;

		wstrIndex = CA2W(Index.c_str(), CP_UTF8);
		wstrCaption = (LPCWSTR)CA2W(Caption.c_str(), CP_UTF8);
		wstrBusType = (LPCWSTR)CA2W(BusType.c_str());
		wstrSize = (LPCWSTR)CA2W((strDiskSize + "(" + strDiskSizeH + ")").c_str());

		LVITEM lvi;
		int iItem;

		lvi.iItem = GetItemCount();
		lvi.iSubItem = COL_Index;
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		lvi.pszText = (LPWSTR)(LPCWSTR)wstrIndex;
		lvi.lParam = (LPARAM)index;

		iItem = InsertItem(&lvi);
		SetItemText(iItem, COL_Caption, wstrCaption);
		SetItemText(iItem, COL_BusType, wstrBusType);
		SetItemText(iItem, COL_Size, wstrSize);
	}

	for (int j = 0; j < GetHeaderCtrl()->GetItemCount(); ++j)
		SetColumnWidth(j, LVSCW_AUTOSIZE_USEHEADER);
	return i;
}


void CDiskList::OnContextMenu(CWnd* pWnd, CPoint point)
{
	if (m_bLocked)
		return;

	CMenu menu;
	if (menu.LoadMenu(MAKEINTRESOURCE(IDR_DISK_MENU)))
	{
		CMenu* sub = menu.GetSubMenu(0);

		sub->TrackPopupMenu(TPM_LEFTBUTTON,
			point.x,
			point.y,
			this
		);

	}

}


void CDiskList::OnDiskFresh()
{
	FreshLocalDiskList();
}


void CDiskList::EnterPartition(int iItem)
{
	CPartDlg dlg(this);
	dlg.m_pjdisks = &m_jdisks;
	dlg.m_diskno = iItem;
	dlg.m_ppcinfo = &m_pcinfo;
	INT_PTR nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		CString strSaveto, strConfig;
		dlg.GetItemConfig(iItem, strSaveto, strConfig);
		SetItemText(iItem, COL_Info, strConfig + _T(", ") + strSaveto);
	}
	else if (nResponse == IDCANCEL)
	{
	}
	else if (nResponse == -1)
	{
	}
}


void CDiskList::OnLvnItemchanging(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	if (m_bLocked)
	{
		if (pNMLV->uNewState == 8192 || pNMLV->uNewState == 4096) //Item checked or unchecked
		{
			*pResult = 1;
			return;
		}
	}
	*pResult = 0;
}

int CDiskList::FindNextSelected(int iStart)
{
	int nItemCount = GetItemCount();
	for (int i = iStart+1; i < nItemCount; i++) {
		if (ListView_GetCheckState(GetSafeHwnd(), i)) {
			return i;
		}
	}
	return -1;
}

void CDiskList::Lock(BOOL b)
{
	m_bLocked = b;
}


void CDiskList::OnDiskRemote()
{
	CConnDlg dlg;
	if (IDOK == dlg.DoModal()) {
		QueryRemoteDiskList();
	}

}

///
CAsyncDisks::CAsyncDisks()
{
	m_hWndNotify = NULL;
	m_msgNotify = 0;
	m_port = 0;
	m_bRemoteMode = FALSE;
}

CAsyncDisks::~CAsyncDisks()
{
	Stop();
}

BOOL CAsyncDisks::Start(HWND hWndNotify, UINT msg)
{
	CSingleLock lock(&m_lc, TRUE);
	m_hWndNotify = hWndNotify;
	m_msgNotify = msg;
	return CThread::Start();
}

BOOL CAsyncDisks::StartRemote(HWND hWndNotify, UINT msg, LPCTSTR lspzIP, int port, LPCTSTR lpszPasswd)
{
	CSingleLock lock(&m_lc, TRUE);
	m_hWndNotify = hWndNotify;
	m_msgNotify = msg;
	m_bRemoteMode = TRUE;
	m_strIP = lspzIP;
	m_port = port;
	m_strPasswd = lpszPasswd;
	m_conn.SetCallback(this);
	return CThread::Start();

}

void CAsyncDisks::Stop()
{
	m_conn.Abort();
	{
		CSingleLock lock(&m_lc, TRUE);
		m_hWndNotify = NULL;
		m_msgNotify = 0;
	}
	if (CThread::GetHandle()) {
		if (!Wait(1000)) {
			CThread::Terminate();
		}
		CThread::CloseThreadHandle();
	}
}

DWORD CAsyncDisks::run()
{
	int err;
	std::string pcinfo, diskinfo;

	if (m_bRemoteMode) {
		m_conn.setTarget(m_strIP, m_port, m_strPasswd);
		if (!m_conn.Connect()) {
			_lastErr = LSTRW(RID_Disk_ConnectFailed);
			::PostMessage(m_hWndNotify, m_msgNotify, NOTIFY_ERROR, 0);
			return 0;
		}
		err = m_conn.GetPCInfo(pcinfo);
		if (err != 0) {
			_lastErr.Format(LSTRW(RID_Disk_GetRemoteHostInfoFailed), m_conn.last_errmsg.c_str());
			::PostMessage(m_hWndNotify, m_msgNotify, NOTIFY_ERROR, 0);
			return 0;
		}
		err = m_conn.GetDiskInfo(diskinfo);
		if (err != 0) {
			_lastErr.Format(LSTRW(RID_Disk_GetRemoteDiskInfoFailed), m_conn.last_errmsg.c_str());
			::PostMessage(m_hWndNotify, m_msgNotify, NOTIFY_ERROR, 0);
			return 0;
		}
		m_conn.Quit();
		{
			CSingleLock lock(&m_lc, TRUE);
			_pcinfo = pcinfo;
			_result = diskinfo;
			if (m_hWndNotify != NULL) {
				::PostMessage(m_hWndNotify, m_msgNotify, NOTIFY_DONE, 0);
			}
		}
	}
	else {
		pcinfo = get_pcinfo();
		diskinfo = enumPartitions();
		{
			CSingleLock lock(&m_lc, TRUE);
			_pcinfo = pcinfo;
			_result = diskinfo;
			if (m_hWndNotify != NULL) {
				::PostMessage(m_hWndNotify, m_msgNotify, NOTIFY_DONE, 0);
			}
		}
	}
	return 0;
}

void CAsyncDisks::OnConnLog(LPCWSTR lpszText)
{
	if (m_hWndNotify != NULL) {
		::PostMessage(m_hWndNotify, m_msgNotify, NOTIFY_PROGRESS, (LPARAM)_wcsdup(lpszText));
	}
}

