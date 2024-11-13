// PartList.cpp: 实现文件
//

#include "pch.h"
#include "MFCdisk2vmdk.h"
#include "PartList.h"


#define ClassT CPartList
ClassT::_tagLVHeader ClassT::m_hdrs[ClassT::LVM_COLUMNCOUNT] =
{
	RID_PartIndex, 120,
	RID_Lable,340,
	RID_Size,140,
	RID_PartUsed,80,
	RID_SectorOffset, 200,
	RID_Bitlocker, 100,
	RID_Option_All_Space, 100,
	RID_Option_VSS, 100
};

// CPartList

IMPLEMENT_DYNAMIC(CPartList, CListCtrl)

CPartList::CPartList()
{
}

CPartList::~CPartList()
{
}


BEGIN_MESSAGE_MAP(CPartList, CQuickList)
END_MESSAGE_MAP()



// CPartList 消息处理程序




BOOL CPartList::Init()
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

	return 0;
}