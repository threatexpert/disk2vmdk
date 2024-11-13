// CConnDlg.cpp: 实现文件
//

#include "pch.h"
#include "MFCdisk2vmdk.h"
#include "CConnDlg.h"
#include "afxdialogex.h"


CString CConnDlg::strIP = _T("127.0.0.1");
int CConnDlg::port = 8000;
CString CConnDlg::strPasswd = _T("");

// CConnDlg 对话框

IMPLEMENT_DYNAMIC(CConnDlg, CDialogEx)

CConnDlg::CConnDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_CONN, pParent)
{
}

CConnDlg::~CConnDlg()
{
}

void CConnDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CConnDlg, CDialogEx)
	ON_BN_CLICKED(IDOK, &CConnDlg::OnBnClickedOk)
END_MESSAGE_MAP()


// CConnDlg 消息处理程序


void CConnDlg::OnBnClickedOk()
{
	GetDlgItemText(IDC_EDIT_IP, strIP);
	port = GetDlgItemInt(IDC_EDIT_PORT);
	GetDlgItemText(IDC_EDIT_PSW, strPasswd);
	CDialogEx::OnOK();
}


BOOL CConnDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	SetDlgItemText(IDC_EDIT_IP, strIP);
	SetDlgItemInt(IDC_EDIT_PORT, port);
	SetDlgItemText(IDC_EDIT_PSW, strPasswd);
	return TRUE;
}
