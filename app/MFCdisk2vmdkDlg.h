
// MFCdisk2vmdkDlg.h: 头文件
//

#pragma once
#include "DiskList.h"
#include <vector>
#include "GetPCInfo.h"

class CMakerItem
	: public IImageMakerCallback
	, public vssclient_Callback
{
public:
	int m_iItem;
	CCriticalSection m_lc;
	class CMFCdisk2vmdkDlg* m_pMainDlg;
	Cvssclient m_vssc;
	std::vector<CVolumeRangeHandler*> m_resolvingDevPath;
	CImageMaker m_maker;
	json m_jdisk;

	DWORD mTickBegin;
	DWORD mTick;
	uint64_t position_prev, data_copied_prev;
	uint64_t _position, _disksize, _data_copied, _data_space_size;

	CString m_strProgress;
	CString m_strStatic;
	CString m_strLastErr;
	int _percent;

	CMakerItem();
	~CMakerItem();

	bool Start();
	void Stop();
	bool Wait(DWORD ms);

protected:
	bool ResolveVolume2VSSDev();
	bool Start_Internal();

	virtual void ImageMaker_BeforeStart(LPCWSTR lpszStatus);
	virtual void ImageMaker_OnStart();
	virtual void ImageMaker_OnCopied(uint64_t position, uint64_t disksize, uint64_t data_copied, uint64_t data_space_size);
	virtual void ImageMaker_OnRemoteConnLog(LPCWSTR lpszStatus);
	virtual void ImageMaker_OnEnd(int err_code);

	virtual void vssclient_OnProgress(int evt, HRESULT result, LPCWSTR lpszDesc);;

};

// CMFCdisk2vmdkDlg 对话框
class CMFCdisk2vmdkDlg : public CDialogEx
{
// 构造
public:
	CMFCdisk2vmdkDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MFCdisk2vmdk_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()

public:
	CDiskList m_cDiskList;
	int m_nJobIndex;
	CMakerItem *m_pMaker;
	afx_msg void OnDblclkDiskList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedButtonStop();
	LRESULT OnMakerProgress(WPARAM wParam, LPARAM lParam);
	LRESULT OnMakerEnd(WPARAM wParam, LPARAM lParam);
	LRESULT OnItemDone(WPARAM wParam, LPARAM lParam);
	LRESULT OnAllItemsDone(WPARAM wParam, LPARAM lParam);
	LRESULT OnVSSEvent(WPARAM wParam, LPARAM lParam);
	LRESULT OnCreateThumbToolBar(WPARAM, LPARAM);
	LRESULT OnDiskListNotify(WPARAM wParam, LPARAM lParam);


protected:
	void SetVer();
public:
	CButton m_BtnStart;
	CButton m_btnStop;
	CStatic m_StaticMain;
	static int m_bDeveloperMode;
	int m_ver_clk_times;
	static UINT g_uTBBC;
	CComPtr<ITaskbarList3> m_spTaskbarList3;
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	virtual void PostNcDestroy();
	afx_msg void OnESC();
	virtual void OnCancel();
	afx_msg void OnStnClickedLogo();
	afx_msg void OnStnDblclickStaticVer();
};
