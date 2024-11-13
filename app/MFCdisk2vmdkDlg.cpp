
// MFCdisk2vmdkDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "MFCdisk2vmdk.h"
#include "MFCdisk2vmdkDlg.h"
#include "afxdialogex.h"
#include <fstream>
#include <algorithm>
#include "CPartDlg.h"
#include "CConnDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
public:
	CStatic m_pic;
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
	//m_pic.SetBitmap(::LoadBitmap(NULL, MAKEINTRESOURCEW(IDB_BITMAP1)));
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PIC, m_pic);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CMFCdisk2vmdkDlg 对话框

#define MYMSG_PROGRESS (WM_APP + 100)
#define MYMSG_END (WM_APP + 101)
#define MYMSG_ItemDone (WM_APP + 102)
#define MYMSG_AllItemsDone (WM_APP + 103)
#define MYMSG_VSS (WM_APP + 104)
#define MYMSG_DiskListNotify (WM_APP + 105)

//#define MYTM_SPEED 100
#define MYTM_WaitMakerExit 101

enum {
	MYMSG_PARAM_BeforeStart,
	MYMSG_PARAM_OnStart,
	MYMSG_PARAM_OnCopied,
	MYMSG_PARAM_OnRemoteConnLog,
	MYMSG_PARAM_OnEnd,
};

int CMFCdisk2vmdkDlg::m_bDeveloperMode = 1;
UINT CMFCdisk2vmdkDlg::g_uTBBC = 0;
typedef 
BOOL WINAPI __ChangeWindowMessageFilter(
	UINT  message,
	DWORD dwFlag
);
static __ChangeWindowMessageFilter *pChangeWindowMessageFilter = NULL;

CMFCdisk2vmdkDlg::CMFCdisk2vmdkDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_MFCdisk2vmdk_DIALOG, pParent)
	, m_pMaker(NULL)
	, m_nJobIndex(0)
{
	m_ver_clk_times = 0;
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CMFCdisk2vmdkDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST2, m_cDiskList);
	DDX_Control(pDX, IDOK, m_BtnStart);
	DDX_Control(pDX, IDC_STATIC_MAIN, m_StaticMain);
	DDX_Control(pDX, IDC_BUTTON_STOP, m_btnStop);
}

BEGIN_MESSAGE_MAP(CMFCdisk2vmdkDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_NOTIFY(NM_DBLCLK, IDC_LIST2, &CMFCdisk2vmdkDlg::OnDblclkDiskList)
	ON_BN_CLICKED(IDOK, &CMFCdisk2vmdkDlg::OnBnClickedOk)
	ON_MESSAGE(MYMSG_PROGRESS, OnMakerProgress)
	ON_MESSAGE(MYMSG_END, OnMakerEnd)
	ON_MESSAGE(MYMSG_ItemDone, OnItemDone)
	ON_MESSAGE(MYMSG_AllItemsDone, OnAllItemsDone)
	ON_MESSAGE(MYMSG_VSS, OnVSSEvent)
	ON_MESSAGE(MYMSG_DiskListNotify, OnDiskListNotify)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDCANCEL, &CMFCdisk2vmdkDlg::OnESC)
	ON_STN_CLICKED(IDC_LOGO, &CMFCdisk2vmdkDlg::OnStnClickedLogo)
	ON_STN_DBLCLK(IDC_STATIC_VER, &CMFCdisk2vmdkDlg::OnStnDblclickStaticVer)
	ON_REGISTERED_MESSAGE(g_uTBBC, &CMFCdisk2vmdkDlg::OnCreateThumbToolBar)
	ON_BN_CLICKED(IDC_BUTTON_STOP, &CMFCdisk2vmdkDlg::OnBnClickedButtonStop)
END_MESSAGE_MAP()


// CMFCdisk2vmdkDlg 消息处理程序

BOOL CMFCdisk2vmdkDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标
	// TODO: 在此添加额外的初始化代码
	SetWindowText(_T("disk2vmdk - 狼蛛安全实验室：www.threatexpert.cn"));
	SetVer();
	CRect rc;
	m_BtnStart.GetWindowRect(&rc);
	ScreenToClient(rc);
	m_btnStop.MoveWindow(rc);
	m_btnStop.ShowWindow(SW_HIDE);
	m_BtnStart.ShowWindow(SW_SHOW);
	*(void**)&pChangeWindowMessageFilter = GetProcAddress(GetModuleHandleA("user32"), "ChangeWindowMessageFilter");
	g_uTBBC = RegisterWindowMessage(L"TaskbarButtonCreated");
	if (pChangeWindowMessageFilter)
		pChangeWindowMessageFilter(g_uTBBC, MSGFLT_ADD);
	EnableBackupPrivilege();
	m_cDiskList.Init(m_hWnd, MYMSG_DiskListNotify);

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CMFCdisk2vmdkDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else if (nID == SC_CLOSE)
	{
		OnCancel();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CMFCdisk2vmdkDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CMFCdisk2vmdkDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


LRESULT CMFCdisk2vmdkDlg::OnCreateThumbToolBar(WPARAM, LPARAM)
{
	if (NULL == m_spTaskbarList3)
	{
		CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_ALL,
			IID_ITaskbarList3, (void**)&m_spTaskbarList3);
	}
	if (m_spTaskbarList3) {
		if (FAILED(m_spTaskbarList3->HrInit())) {
			m_spTaskbarList3 = NULL;
		}
	}
	return 0;
}

void CMFCdisk2vmdkDlg::OnDblclkDiskList(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (!m_BtnStart.IsWindowVisible()) {
		return;
	}
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	if (pNMItemActivate->iItem >= 0) {
		m_cDiskList.EnterPartition(pNMItemActivate->iItem);
	}
	*pResult = 0;
}

BOOL check_disk_config(json &disk, CString &errInfo)
{
	std::string __saveto;

	if (disk.contains("__saveto")) {
		__saveto = disk["__saveto"].get<std::string>();
		if (!__saveto.size()) {
			errInfo = LSTRW(RID_Part_NotConfig);
			return FALSE;
		}
		CString strPath = CA2W(__saveto.c_str(), CP_UTF8);
		if (PathFileExists(strPath)) {
			errInfo = LSTRW(RID_Part_FileExist);
			return FALSE;
		}
		return TRUE;
	}
	else {
		errInfo = LSTRW(RID_Part_NotConfig);
		return FALSE;
	}

}

BOOL check_drive_bitlocker(json& disk, CString& errInfo)
{
	if (disk.contains("__bitlocker_decrypted_prefered")) {
		bool __bitlocker_decrypted_prefered = disk["__bitlocker_decrypted_prefered"];
		if (!__bitlocker_decrypted_prefered)
			return TRUE;

		json &partitions = disk["partitions"];
		int i;
		for (i = 0; i < (int)partitions.size(); i++)
		{
			json &part = partitions[i];
			std::string volume = part["volume"].get<std::string>();
			std::string mount = part["mount"].get<std::string>();
			uint64_t length = part["length"];
			int bitlocker = part["bitlocker"];
			std::string bitlockstat = VolumeProtectionStatus_tostring(bitlocker);

			if (disk.contains("__remote_disk")) {
				if (bitlockstat == "locked") {
					errInfo = LSTRW(RID_Part_expect_unlock);
					return FALSE;
				}
			}
			else {
				if (mount.size() >= 2 && mount[1] == ':' && (bitlockstat == "locked" || bitlockstat == "unlocked")) {
					bitlocker = GetVolumeProtectionStatus(mount[0]);
					part["bitlocker"] = bitlocker;
					bitlockstat = VolumeProtectionStatus_tostring(bitlocker);
				}

				if (bitlockstat == "locked") {
					errInfo = LSTRW(RID_Part_expect_unlock);
					return FALSE;
				}
				else if (bitlockstat == "unlocked") {
					CVolumeReader readtest;
					if (!readtest.setSource(CA2W(volume.c_str(), CP_UTF8), length, FALSE)) {
						errInfo = LSTRW(RID_Part_bitlock_unexpected);
						errInfo += GetLastErrorAsString().c_str();
						return FALSE;
					}
				}
			}

		}
		return TRUE;
	}
	else {
		return TRUE;
	}

}

void CMFCdisk2vmdkDlg::OnBnClickedOk()
{
	class Cstorages
	{
	public:
		std::vector<CString> _dests;
		std::vector<uint64_t> _dests_size;

		int add(LPCWSTR file, uint64_t est_size) {
			size_t i;
			for (i = 0; i < _dests.size(); i++) {
				if (PathIsSameRootW(_dests[i], file)) {
					return check(i, est_size);
				}
			}

			CString strFolder = file;
			*((LPTSTR)(LPCTSTR)PathFindFileName(strFolder)) = '\0';
			strFolder.ReleaseBuffer();
			ULARGE_INTEGER FreeBytesAvailableToCaller;
			if (GetDiskFreeSpaceEx(strFolder, &FreeBytesAvailableToCaller, NULL, NULL)) {
				_dests.push_back((LPCWSTR)strFolder);
				_dests_size.push_back(FreeBytesAvailableToCaller.QuadPart);
				return check(i, est_size);
			}
			return -1;
		}

		int check(size_t i, uint64_t est_size) {
			uint64_t& sz = _dests_size[i];
			if (est_size > sz) {
				return 0;
			}
			sz -= est_size;
			return 1;
		}
	}strg;

	int nItemCount = m_cDiskList.GetItemCount();
	int i;
	std::list<int> selected_disk;
	CString errInfo;
	CString errMsg;
	for (i = 0; i < nItemCount; i++) {
		if (ListView_GetCheckState(m_cDiskList.GetSafeHwnd(), i)) {
			json &disk = m_cDiskList.m_jdisks.at(i);
			int index = disk["index"];
			if (!check_disk_config(disk, errInfo)) {
				errMsg.Format(LSTRW(RID_DiskConfigError), index, (LPCTSTR)errInfo);
				MessageBox(errMsg, LSTRW(RID_TIPS), MB_ICONERROR);
				return;
			}

			if (!check_drive_bitlocker(disk, errInfo)) {
				errMsg.Format(LSTRW(RID_DiskConfigError), index, (LPCTSTR)errInfo);
				MessageBox(errMsg, LSTRW(RID_TIPS), MB_ICONERROR);
				return;
			}
			CString saveto;
			uint64_t est_size = 0;
			if (!CPartDlg::GetOutputInfo(disk, saveto, est_size)) {
				errMsg.Format(LSTRW(RID_DiskConfigError), index, LSTRW(RID_UnableToEstOutputSize));
				MessageBox(errMsg, LSTRW(RID_TIPS), MB_ICONERROR);
				return;
			}

			switch (strg.add(saveto, est_size)) {
			case -1:
				errMsg.Format(LSTRW(RID_ImageOutputWarning1), index, GetLastErrorAsString().c_str());
				if (MessageBox(errMsg, LSTRW(RID_TIPS), MB_ICONERROR | MB_YESNO) != IDYES) {
					return;
				}
				break;
			case 0:
				errMsg.Format(LSTRW(RID_ImageOutputWarning2), index);
				if (MessageBox(errMsg, LSTRW(RID_TIPS), MB_ICONWARNING | MB_YESNO) != IDYES) {
					return;
				}
				break;
			case 1:
				break;
			}

			selected_disk.push_back(i);
		}
	}

	if (!selected_disk.size()) {
		errMsg = LSTRW(RID_NoDisksChecked);
		MessageBox(errMsg, LSTRW(RID_TIPS), MB_ICONERROR);
		return;
	}

	m_pMaker = new CMakerItem();
	m_pMaker->m_pMainDlg = this;
	m_nJobIndex = m_pMaker->m_iItem = m_cDiskList.FindNextSelected(-1);
	if (!m_pMaker->Start()) {
		errMsg = m_pMaker->m_maker.lasterr().c_str();
		delete m_pMaker;
		m_pMaker = NULL;
		MessageBox(errMsg, LSTRW(RID_TIPS), MB_ICONERROR);
		return;
	}

	errMsg.Format(LSTRW(RID_ImageTaskStarted), (int)m_cDiskList.GetItemData(m_nJobIndex));
	m_StaticMain.SetWindowText(errMsg);

	m_cDiskList.Lock(TRUE);
	m_BtnStart.ShowWindow(SW_HIDE);
	m_btnStop.ShowWindow(SW_SHOW);
}


void CMFCdisk2vmdkDlg::OnBnClickedButtonStop()
{
	if (IDYES != MessageBox(LSTRW(RID_SureToStop), LSTRW(RID_TIPS), MB_YESNO)) {
		return;
	}

	if (m_pMaker) {
		m_pMaker->Stop();
		m_pMaker->Wait(INFINITE);
		delete m_pMaker;
		m_pMaker = NULL;
	}
	m_btnStop.ShowWindow(SW_HIDE);
	m_BtnStart.ShowWindow(SW_SHOW);
	m_cDiskList.Lock(FALSE);
	m_StaticMain.SetWindowText(_T(""));
	if (m_spTaskbarList3) {
		m_spTaskbarList3->SetProgressState(m_hWnd, TBPF_NOPROGRESS);
	}
}

LRESULT CMFCdisk2vmdkDlg::OnMakerProgress(WPARAM wParam, LPARAM lParam)
{
	if (!m_pMaker)
		return 0;
	CSingleLock lock(&m_pMaker->m_lc, TRUE);

	int iItem = (int)wParam;
	switch (lParam)
	{
	case MYMSG_PARAM_BeforeStart:
	case MYMSG_PARAM_OnStart:
	{
		m_cDiskList.SetItemText(iItem, CDiskList::COL_Info, m_pMaker->m_strProgress);
		if (m_spTaskbarList3) {
			m_spTaskbarList3->SetProgressState(m_hWnd, TBPF_NORMAL);
		}
	}
	break;
	case MYMSG_PARAM_OnCopied:
	{
		m_cDiskList.SetItemText(iItem, CDiskList::COL_Info, m_pMaker->m_strProgress);
		m_StaticMain.SetWindowText(m_pMaker->m_strStatic);
		if (m_spTaskbarList3) {
			m_spTaskbarList3->SetProgressValue(m_hWnd, m_pMaker->_percent, 100);
		}
	}
	break;
	case MYMSG_PARAM_OnRemoteConnLog:
	{
		m_cDiskList.SetItemText(iItem, CDiskList::COL_Info, m_pMaker->m_strProgress);
	}
	break;
	}
	return 0;
}

LRESULT CMFCdisk2vmdkDlg::OnMakerEnd(WPARAM wParam, LPARAM lParam)
{
	if (!m_pMaker)
		return 0;
	CSingleLock lock(&m_pMaker->m_lc, TRUE);
	int iItem = (int)wParam;
	CString errMsg;
	int iNextItem;
	m_cDiskList.SetItemText(iItem, CDiskList::COL_Info, m_pMaker->m_strProgress);
	m_StaticMain.SetWindowText(_T(""));

	SetTimer(MYTM_WaitMakerExit, 1000, NULL);
	iNextItem = m_cDiskList.FindNextSelected(m_nJobIndex);
	if (iNextItem > 0) {
		errMsg.Format(LSTRW(RID_PreparingCloneNext), (int)m_cDiskList.GetItemData(iNextItem));
		m_StaticMain.SetWindowText(errMsg);
	}
	else {
		errMsg = LSTRW(RID_WaitForCompletion);
		m_StaticMain.SetWindowText(errMsg);
	}
	return 1;
}

LRESULT CMFCdisk2vmdkDlg::OnItemDone(WPARAM wParam, LPARAM lParam)
{
	if (m_spTaskbarList3) {
		m_spTaskbarList3->SetProgressState(m_hWnd, TBPF_NOPROGRESS);
	}
	
	int nextSelected = m_cDiskList.FindNextSelected(m_nJobIndex);
	if (nextSelected < 0) {
		return SendMessage(MYMSG_AllItemsDone, 0, 0);
	}
	CString errMsg;
	m_pMaker = new CMakerItem();
	m_pMaker->m_pMainDlg = this;
	m_nJobIndex = m_pMaker->m_iItem = nextSelected;
	if (!m_pMaker->Start()) {
		errMsg.Format(LSTRW(RID_ErrorString), (LPCTSTR)m_pMaker->m_strLastErr);
		m_cDiskList.SetItemText(m_nJobIndex, CDiskList::COL_Info, errMsg);
		delete m_pMaker;
		m_pMaker = NULL;
		PostMessage(MYMSG_ItemDone, 0, 0);
		return 0;
	}

	return 0;
}

LRESULT CMFCdisk2vmdkDlg::OnAllItemsDone(WPARAM wParam, LPARAM lParam)
{
	m_cDiskList.Lock(FALSE);
	m_BtnStart.ShowWindow(SW_SHOW);
	m_btnStop.ShowWindow(SW_HIDE);
	m_StaticMain.SetWindowText(_T(""));
	FlashWindow(TRUE);
	return 0;
}

LRESULT CMFCdisk2vmdkDlg::OnVSSEvent(WPARAM wParam, LPARAM lParam)
{
	if (!m_pMaker)
		return 0;
	CSingleLock lock(&m_pMaker->m_lc, TRUE);
	int iItem = (int)wParam;
	HRESULT result = (HRESULT)lParam;
	m_cDiskList.SetItemText(iItem, CDiskList::COL_Info, m_pMaker->m_strProgress);
	if (result != 0) {
		PostMessage(MYMSG_END, iItem, result);
	}
	return 0;
}

LRESULT CMFCdisk2vmdkDlg::OnDiskListNotify(WPARAM wParam, LPARAM lParam)
{
	if (lParam) {
		m_StaticMain.SetWindowText((LPCWSTR)lParam);
	}
	return 0;
}

void CMFCdisk2vmdkDlg::SetVer()
{
	CString strVer;
	WCHAR szAppVer[64];
	GetAppVersionString(szAppVer, _countof(szAppVer));

	//每次发行新版时，重新编译工程，或这个cpp文件得专门编译一下，否则__DATE__是旧的
	if (m_bDeveloperMode) {
		strVer.Format(_T("Version %s %s (%s)"), GetAppArch(), szAppVer, (LPCTSTR)CA2W(__DATE__));
	}
	else {
		strVer.Format(_T("Version %s %s"), GetAppArch(), szAppVer);
	}
	SetDlgItemText(IDC_STATIC_VER, strVer);
}

void CMFCdisk2vmdkDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == MYTM_WaitMakerExit) {
		if (m_pMaker) {
			if (!m_pMaker->Wait(0))
				return;
			KillTimer(nIDEvent);
			delete m_pMaker;
			m_pMaker = NULL;
			PostMessage(MYMSG_ItemDone, 0, 0);
		}
		return;
	}
	CDialogEx::OnTimer(nIDEvent);
}


void CMFCdisk2vmdkDlg::PostNcDestroy()
{

	CDialogEx::PostNcDestroy();
}


void CMFCdisk2vmdkDlg::OnESC()
{
	//CDialogEx::OnCancel();
}


void CMFCdisk2vmdkDlg::OnCancel()
{
	if (m_pMaker) {
		m_pMaker->Stop();
		m_pMaker->Wait(INFINITE);
		delete m_pMaker;
		m_pMaker = NULL;
	}
	CDialogEx::OnCancel();
}


void CMFCdisk2vmdkDlg::OnStnClickedLogo()
{
	// TODO: 在此添加控件通知处理程序代码
	CAboutDlg* aDlg = new  CAboutDlg;
	aDlg->Create(IDD_ABOUTBOX, this);
	aDlg->ShowWindow(SW_SHOW);
}

/// <summary>
/// ////////////////////////////////////
/// </summary>
/// 
CMakerItem::CMakerItem()
	: m_iItem(0)
	, m_pMainDlg(NULL)
{
	_position = 0;
	_disksize = 0;
	_data_copied = 0;
	_data_space_size = 0;
	_percent = 0;
}

CMakerItem::~CMakerItem()
{
	
}

bool CMakerItem::Start()
{
	if (m_pMainDlg->m_bDeveloperMode) {
		m_maker.EnableVBoxLibMode();
	}
	m_jdisk = m_pMainDlg->m_cDiskList.m_jdisks.at(m_iItem);
	json property = m_jdisk["property"];
	json &partitions = m_jdisk["partitions"];
	int index = m_jdisk["index"];
	uint64_t DiskSize = property["DiskSize"];
	int i;

	if (m_jdisk.contains("__remote_disk")) {
		m_maker.setBufferSize(recv_chunk_maxsize);
		if (!m_maker.setRemoteSource(CConnDlg::strIP, CConnDlg::port, CConnDlg::strPasswd,index, DiskSize)) {
			m_strLastErr = m_maker.lasterr().c_str();
			return false;
		}
	}
	else {
		if (!m_maker.setSource(index, DiskSize)) {
			m_strLastErr = m_maker.lasterr().c_str();
			return false;
		}
	}

	std::vector<int> __exclude;
	std::vector<int> __only_used_space;
	std::vector<int> __use_vss;

	if (m_jdisk.contains("__exclude")) {
		//没有勾选的分区
		__exclude = m_jdisk["__exclude"].get<std::vector<int>>();
	}
	if (m_jdisk.contains("__only_used_space")) {
		//没有勾选[全部空间]的分区
		__only_used_space = m_jdisk["__only_used_space"].get<std::vector<int>>();
	}
	if (m_jdisk.contains("__use_vss")) {
		//勾选[VSS]的分区
		__use_vss = m_jdisk["__use_vss"].get<std::vector<int>>();
	}

	for (i = 0; i < (int)partitions.size(); i++)
	{
		json &part = partitions[i];
		int no = part["number"];
		uint64_t offset = part["offset"];
		uint64_t length = part["length"];
		std::string volume = part["volume"].get<std::string>();
		std::string mount = part["mount"].get<std::string>();
		int bitlocker = part["bitlocker"];
		std::string bitlocker_stat = VolumeProtectionStatus_tostring(bitlocker);

		if (std::find(__exclude.begin(), __exclude.end(), no) != __exclude.end()) {
			//excluded
			if (!m_maker.addExcludeRange(offset, offset + length, 1)) {
				m_strLastErr = m_maker.lasterr().c_str();
				return false;
			}
		}
		else if (volume.size()){
			CRangeMgr::RangeHandler* pRH = NULL;
			bool only_used_space = (std::find(__only_used_space.begin(), __only_used_space.end(), no) != __only_used_space.end());
			bool __bitlocker_decrypted_prefered = m_jdisk.contains("__bitlocker_decrypted_prefered") ? m_jdisk["__bitlocker_decrypted_prefered"] : false;
			bool __ignore_P_H_V = m_jdisk.contains("__ignore_P_H_V") ? m_jdisk["__ignore_P_H_V"] : false;
			bool use_vss = (std::find(__use_vss.begin(), __use_vss.end(), no) != __use_vss.end());

			uint64_t totalClustersInBytes = (uint64_t)part["TotalNumberOfCluster"] * (uint64_t)part["SectorsPerCluster"] * (uint64_t)part["BytesPerSector"];
			if (totalClustersInBytes > length) {
				m_strLastErr = LSTRW(RID_OpenVolErrorComma);
				return false;
			}
			if (m_jdisk.contains("__remote_disk")) {
				if (only_used_space || (bitlocker_stat == "unlocked" && __bitlocker_decrypted_prefered)) {
					CRemoteVolumeRangeHandler* pRVR = new CRemoteVolumeRangeHandler();
					pRVR->SetConnectionCallback(&m_maker);
					if (only_used_space) {
						pRVR->setOptions(pRVR->getOptions() | CVolumeReader::OptOnlyUsedSpace);
						if (__ignore_P_H_V)
							pRVR->setOptions(pRVR->getOptions() | CVolumeReader::OptIgnorePagefile | CVolumeReader::OptIgnoreHiberfil | CVolumeReader::OptIgnoreSVI);
					}
					if (!pRVR->OpenRemote(CConnDlg::strIP, CConnDlg::port, CConnDlg::strPasswd, CA2W(volume.c_str(), CP_UTF8), length)) {
						m_strLastErr = LSTRW(RID_OpenVolErrorComma);
						m_strLastErr += pRVR->lasterr().c_str();
						delete pRVR;
						return false;
					}
					pRH = pRVR;
				}
			}
			else {
				if (only_used_space || use_vss || (bitlocker_stat == "unlocked" && __bitlocker_decrypted_prefered)) {
					//只拷贝正在使用的空间，以及bitlocker解锁的分区，需要专门打开该分区读取数据
					CVolumeRangeHandler *pVR = new CVolumeRangeHandler();
					if (use_vss) {
						pVR->setOptions(pVR->getOptions() | CVolumeReader::OptOnlyUsedSpace); //vss模式开启对快照也使用非[全部空间]
						if (__ignore_P_H_V)
							pVR->setOptions(pVR->getOptions() | CVolumeReader::OptOnlyUsedSpace | CVolumeReader::OptIgnorePagefile | CVolumeReader::OptIgnoreHiberfil | CVolumeReader::OptIgnoreSVI);
						std::wstring volName = CA2W(mount.size() ? mount.c_str() : volume.c_str(), CP_UTF8);
						m_vssc.AddVolume(volName.c_str(), TRUE);
						pVR->setSource(volName.c_str(),
							length,
							TRUE);//TRUE表示延迟打开，因为VSS创建分区还原点需要等，不能卡住界面，先保存pVR指针，后面还原点完成后，重新setSource
						pVR->SetSnapshotTime();
						m_resolvingDevPath.push_back(pVR);
					}
					else {
						if (only_used_space) {
							pVR->setOptions(pVR->getOptions() | CVolumeReader::OptOnlyUsedSpace);
							if (__ignore_P_H_V)
								pVR->setOptions(pVR->getOptions() | CVolumeReader::OptIgnorePagefile | CVolumeReader::OptIgnoreHiberfil | CVolumeReader::OptIgnoreSVI);
						}
						if (!pVR->setSource(CA2W(volume.c_str(), CP_UTF8), length, FALSE)) {
							m_strLastErr = LSTRW(RID_OpenVolErrorComma);
							m_strLastErr += pVR->lasterr().c_str();
							delete pVR;
							return false;
						}
					}
					pRH = pVR;
				}
			}
			if (pRH) {
				if (!m_maker.addRangeHandler(offset, offset + length, 1, pRH)) {
					m_strLastErr = m_maker.lasterr().c_str();
					delete pRH;
					return false;
				}
				pRH->Release();

			}
		}
	}

	if (m_resolvingDevPath.size() > 0) {
		if (IsWow64()) {
			m_strLastErr = LSTRW(RID_ShouldRunX64Version);
			return false;
		}
		return !!m_vssc.AsyncStart(this);
	}
	else {
		return Start_Internal();
	}
}

//将要镜像的分区路径换为VSS创建好的还原点的设备路径
bool CMakerItem::ResolveVolume2VSSDev()
{
	for (auto pVR : m_resolvingDevPath) {
		BOOL bOpen = FALSE;
		uint64_t volSize;
		std::wstring devpath;
		std::wstring volName = pVR->getSource(&bOpen);
		if (m_vssc.GetVolumeSnapshotDevice(volName.c_str(), devpath)) {
			pVR->GetSize(&volSize);
			if (!pVR->setSource(devpath.c_str(), volSize, FALSE)) {
				m_strLastErr = LSTRW(RID_OpenVSSVolFailed);
				m_strLastErr += pVR->lasterr().c_str();
				return false;
			}
		}
		else {
			m_strLastErr = LSTRW(RID_GetVSSVolPathFailed);
			m_strLastErr += fromHResult(m_vssc.LastErr()).c_str();
			return false;
		}
	}
	return true;
}

bool CMakerItem::Start_Internal()
{
	std::string __saveto;
	if (!m_jdisk.contains("__saveto")) {
		m_strLastErr = LSTRW(RID_ImagePathNotSet);
		return false;
	}
	__saveto = m_jdisk["__saveto"].get<std::string>();

	if (!ResolveVolume2VSSDev()) {
		return false;
	}
	m_resolvingDevPath.clear();

	CStringW strDest = CA2W(__saveto.c_str(), CP_UTF8);
	if (!m_maker.setDest(strDest, true)) {
		m_strLastErr = m_maker.lasterr().c_str();
		return false;
	}

	m_maker.setCallback(this);

	if (!m_maker.start()) {
		m_strLastErr = m_maker.lasterr().c_str();
		return false;
	}
	return true;
}

void CMakerItem::Stop()
{
	m_vssc.Cancel();
	m_maker.setCallback(NULL);
	m_maker.stop();
}

bool CMakerItem::Wait(DWORD ms)
{
	if (m_vssc.CThread::GetHandle()) {
		if (!m_vssc.CThread::Wait(0)) {
			m_vssc.Cancel();
		}
		if (ms) {
			if (!m_vssc.CThread::Wait(ms)) {
				return false;
			}
		}
	}
	return !!m_maker.wait(ms);
}

void CMakerItem::ImageMaker_BeforeStart(LPCWSTR lpszStatus)
{
	CSingleLock lock(&m_lc, TRUE);
	m_strProgress = lpszStatus;
	m_pMainDlg->PostMessage(MYMSG_PROGRESS, m_iItem, MYMSG_PARAM_BeforeStart);
}

void CMakerItem::ImageMaker_OnStart()
{
	{
		CSingleLock lock(&m_lc, TRUE);

		mTickBegin = mTick = GetTickCount();
		position_prev = data_copied_prev = 0;
		_position = _disksize = 0;
		_percent = 0;
	
		m_strProgress.Format(LSTRW(RID_GettingStarted));
		m_pMainDlg->PostMessage(MYMSG_PROGRESS, m_iItem, MYMSG_PARAM_OnStart);
	}

	std::string __saveto = m_jdisk["__saveto"].get<std::string>();
	CStringW strDest = CA2W(__saveto.c_str(), CP_UTF8);
	CStringA strDestA = (LPCSTR)CW2A(strDest);
	std::ofstream savejson;
	savejson.open(strDestA + ".json");
	savejson << m_jdisk.dump(2);
	savejson.close();
}

void CMakerItem::ImageMaker_OnCopied(uint64_t position, uint64_t disksize, uint64_t data_copied, uint64_t data_space_size)
{
	CSingleLock lock(&m_lc, TRUE);
	//char txt_cp[128];
	//char txt_inc[128];
	char txt_speed[128];
	char used[128];
	char lefttime[128];
	DWORD t = GetTickCount() - mTick;
	_position = position;
	_disksize = disksize;
	_data_copied = data_copied;
	_data_space_size = data_space_size;
	if (t >= 1000) {
		mTick = GetTickCount();

		uint64_t inc;
		double speed;
		uint64_t est_sec;

		switch (m_maker.GetWorkingMode()) {
		case CImageMaker::Mode_VD_Piped:
			_percent = max((int)(data_copied * 100 / data_space_size), (int)(position * 100 / disksize));
			if (_percent == 100 && position != disksize)
				_percent -= 1;
			inc = _position - position_prev;
			speed = (double)inc * 1000 / t;
			est_sec = (uint64_t)((disksize - position) / speed);
			break;
		case CImageMaker::Mode_VD_Lib:
			_percent = (int)(data_copied * 100 / data_space_size);
			if (_percent == 100 && position != disksize)
				_percent -= 1;
			inc = data_copied - data_copied_prev;
			speed = (double)inc * 1000 / t;
			est_sec = (uint64_t)((data_space_size - data_copied) / speed);
			break;
		case CImageMaker::Mode_DD:
		default:
			_percent = (int)(position * 100 / disksize);
			inc = _position - position_prev;
			speed = (double)inc * 1000 / t;
			est_sec = (uint64_t)((disksize - position) / speed);
			break;
		}

		calcseconds((mTick - mTickBegin) / 1000, used, sizeof(used));
		calculateSize1024((uint64_t)speed, txt_speed, sizeof(txt_speed));
		calcseconds(est_sec, lefttime, sizeof(lefttime));

		m_strProgress.Format(L"%d%%, %S", _percent, used);
		m_strStatic.Format(LSTRW(RID_Speed_Remaining), txt_speed, lefttime);
		position_prev = position;
		data_copied_prev = data_copied;

		m_pMainDlg->PostMessage(MYMSG_PROGRESS, m_iItem, MYMSG_PARAM_OnCopied);
	}

}

void CMakerItem::ImageMaker_OnRemoteConnLog(LPCWSTR lpszStatus)
{
	CSingleLock lock(&m_lc, TRUE);
	m_strProgress = lpszStatus;
	m_pMainDlg->PostMessage(MYMSG_PROGRESS, m_iItem, MYMSG_PARAM_OnRemoteConnLog);
}

void CMakerItem::ImageMaker_OnEnd(int err_code)
{
	CSingleLock lock(&m_lc, TRUE);
	char used[128];
	mTick = GetTickCount();
	calcseconds((mTick - mTickBegin) / 1000, used, sizeof(used));

	if (_disksize != 0 &&_position == _disksize) {
		m_strProgress.Format(LSTRW(RID_ImageMaker_End_OK), used);
		m_strStatic = _T("");
	}
	else {
		m_strProgress.Format(LSTRW(RID_ImageMaker_End_Err), err_code, m_maker.lasterr().c_str());
		m_strStatic = _T("");
	}

	m_pMainDlg->PostMessage(MYMSG_END, m_iItem, err_code);
}

void CMakerItem::vssclient_OnProgress(int evt, HRESULT result, LPCWSTR lpszDesc)
{
	CSingleLock lock(&m_lc, TRUE);
	m_strProgress = lpszDesc ? lpszDesc : L"";
	m_pMainDlg->PostMessage(MYMSG_VSS, m_iItem, result);
	switch (evt) {
	case Starting:
		break;
	case AddingVolume:
		break;
	case MakingSnapshot:
		break;
	case MadeSnapshot:
		if (result == 0) {
			//还原点创建成功，调用真正开始镜像的流程
			if (!Start_Internal()) {
				result = E_FAIL;
				m_strProgress = m_strLastErr;
				m_pMainDlg->PostMessage(MYMSG_VSS, m_iItem, result);
			}
		}
		break;
	}
}


void CMFCdisk2vmdkDlg::OnStnDblclickStaticVer()
{
	//m_ver_clk_times++;
	//if (m_ver_clk_times >= 2) {
	//	m_bDeveloperMode = !m_bDeveloperMode;
	//	m_ver_clk_times = 0;
	//	SetVer();
	//}
}

