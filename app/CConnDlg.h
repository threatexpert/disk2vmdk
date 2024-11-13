#pragma once


// CConnDlg 对话框

class CConnDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CConnDlg)

public:
	CConnDlg(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CConnDlg();

	static CString strIP;
	static int port;
	static CString strPasswd;

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_CONN };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:

	afx_msg void OnBnClickedOk();
	virtual BOOL OnInitDialog();
};
