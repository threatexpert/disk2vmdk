#pragma once

#include "PartList.h"
#include <vector>

class CPartItemData
{
public:
	int no;
	CString Number;
	CString Label;
	CString Size;
	CString Usage;
	CString firstSector;
	CString Bitlocker;

	bool chk_selected;
	bool chk_entireSpace;
	bool _supportVSS;
	bool chk_useVSS;

	CString _mountOn;
	bool _isWinPart;
	uint64_t _offset;
};
// CPartDlg 对话框

class CPartDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CPartDlg)

public:


	CPartDlg(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CPartDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_PART_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	json* m_pjdisks;
	int m_diskno;
	json* m_ppcinfo;
	CPartList m_cPartList;
	std::vector<CPartItemData> m_data;
	BOOL m_bWow64;
	uint64_t m_winPartOffset;
	CString m_strWinPart;

	virtual BOOL OnInitDialog();
	CEdit m_SaveTo;
	afx_msg void OnBnClickedButtonSelpath();
	afx_msg void OnBnClickedOk();
	afx_msg void OnItemchangedListPart(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg LRESULT OnGetListItem(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnListClick(WPARAM wParam, LPARAM lParam);
	void UpdateList();
	BOOL GetItemConfig(int index, CString& saveto, CString& excludes);
	static BOOL GetOutputInfo(json& disk, CString& saveto, uint64_t& est_size);
};
