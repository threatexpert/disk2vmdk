#pragma once

#include "QuickList.h"
// CPartList

class CPartList : public CQuickList
{
	DECLARE_DYNAMIC(CPartList)

public:

	enum COL_ID
	{
		COL_Number,
		COL_Label,
		COL_Size,
		COL_Usage,
		COL_1stSector,
		COL_Bitlocker,
		COL_EntireSpace,
		COL_VSS,

		LVM_COLUMNCOUNT,
	};

	struct _tagLVHeader
	{
		int HeaderRid;
		int nWidth;
	};

	static _tagLVHeader m_hdrs[LVM_COLUMNCOUNT];

	CPartList();
	virtual ~CPartList();

protected:
	DECLARE_MESSAGE_MAP()

public:
	BOOL Init();

};


