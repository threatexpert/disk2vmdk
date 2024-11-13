#pragma once

#include "../core/Cconnection.h"
#include "../core/Thread.h"

class CAsyncDisks
	: public CThread
	, public IConnectionCallback
{
	CCriticalSection m_lc;
	HWND m_hWndNotify;
	UINT m_msgNotify;
	CString m_strIP;
	CString m_strPasswd;
	int m_port;
	Cconnection m_conn;
public:
	BOOL m_bRemoteMode;
	std::string _pcinfo;
	std::string _result;
	CString _lastErr;

	CAsyncDisks();
	~CAsyncDisks();

	BOOL Start(HWND hWndNotify, UINT msg);
	BOOL StartRemote(HWND hWndNotify, UINT msg, LPCTSTR lspzIP , int port, LPCTSTR lpszPasswd);
	void Stop();

	virtual DWORD run();
	virtual void OnConnLog(LPCWSTR lpszText);
};

// CDiskList

class CDiskList : public CListCtrl
{
	DECLARE_DYNAMIC(CDiskList)

public:

	enum COL_ID
	{
		COL_Index,
		COL_Caption,
		COL_BusType,
		COL_Size,
		COL_Info,

		LVM_COLUMNCOUNT,
	};

	struct _tagLVHeader
	{
		int HeaderRid;
		int nWidth;
	};

	static _tagLVHeader m_hdrs[LVM_COLUMNCOUNT];


	json m_jdisks;
	BOOL m_bLocked;
	json m_pcinfo;
	BOOL m_bRemoteMode;

	CDiskList();
	virtual ~CDiskList();

	void EnterPartition(int iItem);

protected:
	CAsyncDisks* m_pAsyncDisks;
	UINT m_cbMsg;
	HWND m_hParentWnd;
	DECLARE_MESSAGE_MAP()

	int InitDiskList();
public:
	BOOL Init(HWND hParentWnd, UINT cbMsg);
	afx_msg void OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/);
	afx_msg void OnDiskFresh();
	afx_msg void OnLvnItemchanging(NMHDR* pNMHDR, LRESULT* pResult);
	LRESULT OnGetDisks(WPARAM wParam, LPARAM lParam);
	int FreshLocalDiskList();
	int QueryRemoteDiskList();
	int FindNextSelected(int iStart);

	void Lock(BOOL b);
	afx_msg void OnDiskRemote();
};


