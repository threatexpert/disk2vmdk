// CPartDlg.cpp: 实现文件
//

#include "pch.h"
#include "MFCdisk2vmdk.h"
#include "CPartDlg.h"
#include "afxdialogex.h"
#include <algorithm>
#include <sstream>
#include "MFCdisk2vmdkDlg.h"

// CPartDlg 对话框

IMPLEMENT_DYNAMIC(CPartDlg, CDialogEx)

CPartDlg::CPartDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_PART_DIALOG, pParent)
	, m_pjdisks(NULL)
	, m_diskno(0)
    , m_ppcinfo(NULL)
    , m_winPartOffset(0)
{

}

CPartDlg::~CPartDlg()
{
}

void CPartDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST_PART, m_cPartList);
    DDX_Control(pDX, IDC_EDIT_SAVETO, m_SaveTo);
}


BEGIN_MESSAGE_MAP(CPartDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BUTTON_SELPATH, &CPartDlg::OnBnClickedButtonSelpath)
    ON_BN_CLICKED(IDOK, &CPartDlg::OnBnClickedOk)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_PART, &CPartDlg::OnItemchangedListPart)
    ON_MESSAGE(WM_QUICKLIST_GETLISTITEMDATA, OnGetListItem)
    ON_MESSAGE(WM_QUICKLIST_CLICK, OnListClick)
END_MESSAGE_MAP()


// CPartDlg 消息处理程序


BOOL CPartDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();
    SetWindowText(LSTRW(RID_Partdlg_Title));
	m_cPartList.Init();
#ifndef QUICKLIST_NOEMPTYMESSAGE	
    m_cPartList.SetEmptyMessage(0);
#endif
    m_bWow64 = IsWow64();
    BOOL bVSSAPI_OK = FALSE;
    char buf[256];
    int i;
    json &disk = m_pjdisks->at(m_diskno);
    json &property = disk["property"];
    json &partitions = disk["partitions"];

    if (!disk.contains("__remote_disk")) {
        bVSSAPI_OK = !m_bWow64 && InitVSSAPI();
    }

    m_strWinPart = _T("C:");
    if ((*m_ppcinfo)["os"].contains("windir")) {
        std::string winpart = (*m_ppcinfo)["os"]["windir"].get<std::string>();
        winpart = winpart.substr(0, 2);
        std::transform(winpart.begin(), winpart.end(), winpart.begin(), ::toupper);
        m_strWinPart = winpart.c_str();
    }

    std::vector<int> __exclude;
    std::vector<int> __only_used_space;
    std::vector<int> __use_vss;

    if (disk.contains("__exclude")) {
        __exclude = disk["__exclude"].get<std::vector<int>>();
    }
    if (disk.contains("__only_used_space")) {
        __only_used_space = disk["__only_used_space"].get<std::vector<int>>();
    }
    if (disk.contains("__use_vss")) {
        __use_vss = disk["__use_vss"].get<std::vector<int>>();
    }
    if (disk.contains("__saveto")) {
        std::string __saveto = disk["__saveto"].get<std::string>();
        m_SaveTo.SetWindowText(CA2W(__saveto.c_str(), CP_UTF8));
    }

    for (i = 0; i < (int)partitions.size(); i++)
    {
        json part = partitions[i];
        int number = part["number"];
        uint64_t length = part["length"];
        if (!length)
            continue;
        uint64_t offset = part["offset"];
        uint64_t length_free = part["free"];
        std::string label = part["label"].get<std::string>();
        std::string mount = part["mount"].get<std::string>();
        std::string format = part["format"].get<std::string>();
        std::string item_name;
        int BytesPerSector = property["BytesPerSector"];
        bool supportvss = bVSSAPI_OK && format.size();

        item_name = label;
        if (mount.size()) {
            item_name += "(";
            item_name += mount;
            item_name += ")";
        }
        if (format.size()) {
            item_name += " - ";
            item_name += format;
        }
        std::string strlength = calculateSize1024(length, buf, sizeof(buf));
        std::string used;
        if (length_free) {
            int percent_used = (int)((length - length_free) * 100 / length);
            snprintf(buf, sizeof(buf), "%d%%", percent_used);
            used = buf;
        }

        CStringW wstrNumber, wstrLabel, wstrSize, wstrUsage, wstrFirstSector;

        wstrNumber = (LPCWSTR)CA2W(std::to_string(number).c_str(), CP_UTF8);
        wstrLabel = (LPCWSTR)CA2W(item_name.c_str(), CP_UTF8);
        wstrSize = (LPCWSTR)CA2W(strlength.c_str());
        wstrUsage = (LPCWSTR)CA2W(used.c_str());
        wstrFirstSector = (LPCWSTR)CA2W(std::to_string(offset / BytesPerSector).c_str(), CP_UTF8);

        int bitlocker = part["bitlocker"];
        std::string bitlockstat = VolumeProtectionStatus_tostring(bitlocker);
        
        if (mount.size() >= 2 && mount[1] == ':' && (bitlockstat == "locked" || bitlockstat == "unlocked")) {
            bitlocker = GetVolumeProtectionStatus(mount[0]);
            part["bitlocker"] = bitlocker;
            bitlockstat = VolumeProtectionStatus_tostring(bitlocker);
        }
        bool excluded = (std::find(__exclude.begin(), __exclude.end(), number) != __exclude.end());
        bool only_used_space = (std::find(__only_used_space.begin(), __only_used_space.end(), number) != __only_used_space.end());
        bool use_vss = (std::find(__use_vss.begin(), __use_vss.end(), number) != __use_vss.end());

        CPartItemData itemdata;

        itemdata.no = number;
        itemdata.Number = wstrNumber;
        itemdata.Label = wstrLabel;
        itemdata.Size = wstrSize;
        itemdata.Usage = wstrUsage;
        itemdata.firstSector = wstrFirstSector;
        itemdata.Bitlocker = VolumeProtectionStatus_tostringzh(bitlocker);
        itemdata.chk_selected = !excluded;
        itemdata.chk_entireSpace = !only_used_space;
        itemdata._supportVSS = supportvss;
        itemdata.chk_useVSS = use_vss;

        itemdata._mountOn = mount.c_str();
        itemdata._offset = offset;
        itemdata._isWinPart = itemdata._mountOn.Right(2).CompareNoCase(m_strWinPart) == 0;
        if (itemdata._isWinPart) {
            m_winPartOffset = offset;
        }

        m_data.push_back(itemdata);
    }

    UpdateList();

    for (int j = 0; j < m_cPartList.GetHeaderCtrl()->GetItemCount(); ++j)
        m_cPartList.SetColumnWidth(j, LVSCW_AUTOSIZE_USEHEADER);

    bool __bitlocker_decrypted_prefered = true; //默认偏好bitlocker解锁数据
    if (disk.contains("__bitlocker_decrypted_prefered")) {
        __bitlocker_decrypted_prefered = disk["__bitlocker_decrypted_prefered"];
    }
    bool __ignore_P_H_V = false;
    if (disk.contains("__ignore_P_H_V")) {
        __ignore_P_H_V = disk["__ignore_P_H_V"];
    }

    CheckDlgButton(IDC_CHECK_BITLOCKER, __bitlocker_decrypted_prefered ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_CHECK_IGNORE_PGH, __ignore_P_H_V ? BST_CHECKED : BST_UNCHECKED);

	return TRUE;  // return TRUE unless you set the focus to a control
				  // 异常: OCX 属性页应返回 FALSE
}

void CPartDlg::UpdateList()
{
    m_cPartList.LockWindowUpdate();
    m_cPartList.SetItemCount((int)m_data.size());
    m_cPartList.UnlockWindowUpdate();
    m_cPartList.RedrawItems(
        m_cPartList.GetTopIndex(),
        m_cPartList.GetTopIndex() + m_cPartList.GetCountPerPage());
}

bool IsEnglishOnly(LPCTSTR str, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        TCHAR ch = str[i];
        if (ch < 0 || ch > 127 || !isprint(ch)) { // Check for non-ASCII or non-printable characters
            return false;
        }
    }
    return true;
}


void CPartDlg::OnBnClickedButtonSelpath()
{
    CFileDialog dlg(FALSE, _T("*.vmdk"), NULL, OFN_OVERWRITEPROMPT|OFN_EXPLORER);
    //if (CMFCdisk2vmdkDlg::m_bDeveloperMode) {
        dlg.m_ofn.lpstrFilter = L"VMDK (*.vmdk)\0*.vmdk\0RAW (*.dd)\0*.dd\0VHD (*.vhd)\0*.vhd\0VDI (*.vdi)\0*.vdi\0\0";
    //}
    //else {
    //    dlg.m_ofn.lpstrFilter = L"VMDK (*.vmdk)\0*.vmdk\0RAW (*.dd)\0*.dd\0\0";
    //}
    dlg.m_ofn.lpstrTitle = LSTRW(RID_SaveFileDlgTitle);

    if (dlg.DoModal() == IDOK)
    {
        CString strMsg;
        CString strPath = dlg.GetPathName();
        BOOL bExist = PathFileExists(strPath);
        if (bExist) {
            if (DeleteFile(strPath))
                bExist = FALSE;
        }

        if (bExist) {
            m_SaveTo.SetWindowTextW(_T(""));
            MessageBox(LSTRW(RID_FileExistAndDelFailed), LSTRW(RID_TIPS), MB_ICONERROR);
            return;
        }
        else if (strPath.Right(5).CompareNoCase(_T(".vmdk"))
            && strPath.Right(3).CompareNoCase(_T(".dd"))
            && strPath.Right(4).CompareNoCase(_T(".vdi"))
            && strPath.Right(4).CompareNoCase(_T(".vhd"))
            ) {
            m_SaveTo.SetWindowTextW(_T(""));
            MessageBox(LSTRW(RID_InvalidFileExt), LSTRW(RID_TIPS), MB_ICONERROR);
            return;
        }

        LPCTSTR name = PathFindFileName(strPath);
        if (!IsEnglishOnly(name, _tcslen(name))) {
            strMsg.Format(LSTRW(RID_NotAllowedChar), name);
            MessageBox(strMsg, LSTRW(RID_TIPS), MB_ICONERROR);
            return;
        }

        m_SaveTo.SetWindowTextW(strPath);

        json& disk = m_pjdisks->at(m_diskno);
        disk["__saveto"] = (LPCSTR)CW2A((LPCWSTR)strPath, CP_UTF8);
    }
}



void CPartDlg::OnBnClickedOk()
{
    json& disk = m_pjdisks->at(m_diskno);

    CString strPath, strMsg;
    m_SaveTo.GetWindowText(strPath);
    if (strPath.Right(5).CompareNoCase(_T(".vmdk"))
        && strPath.Right(3).CompareNoCase(_T(".dd"))
        && strPath.Right(4).CompareNoCase(_T(".vdi"))
        && strPath.Right(4).CompareNoCase(_T(".vhd"))
        ) {
        MessageBox(LSTRW(RID_InvalidFileExt), LSTRW(RID_TIPS), MB_ICONERROR);
        m_SaveTo.SetFocus();
        return;
    }
    LPCTSTR name = PathFindFileName(strPath);
    if (!IsEnglishOnly(name, _tcslen(name))) {
        strMsg.Format(LSTRW(RID_NotAllowedChar), name);
        MessageBox(strMsg, LSTRW(RID_TIPS), MB_ICONERROR);
        m_SaveTo.SetFocus();
        return;
    }

    disk["__saveto"] = (LPCSTR)CW2A((LPCWSTR)strPath, CP_UTF8);

    std::vector<int> __exclude;
    std::vector<int> __only_used_space;
    std::vector<int> __use_vss;
    int count = m_cPartList.GetItemCount();
    int i;
    for (i = 0; i < count; i++)
    {
        int number = m_data[i].no;
        if (!m_data[i].chk_selected) {
            __exclude.push_back(number);
        }
        if (!m_data[i].chk_entireSpace) {
            __only_used_space.push_back(number);
        }
        if (m_data[i].chk_useVSS) {
            __use_vss.push_back(number);
        }
    }

    disk["__exclude"] = __exclude;
    disk["__only_used_space"] = __only_used_space;
    disk["__use_vss"] = __use_vss;
    disk["__bitlocker_decrypted_prefered"] = IsDlgButtonChecked(IDC_CHECK_BITLOCKER) == BST_CHECKED;
    disk["__ignore_P_H_V"] = IsDlgButtonChecked(IDC_CHECK_IGNORE_PGH) == BST_CHECKED;

    //准备检查目标存储空间是否满足
    json& property = disk["property"];
    json& partitions = disk["partitions"];

    uint64_t nIncludedAndInusedBytes = property["DiskSize"];

    for (i = 0; i < (int)partitions.size(); i++)
    {
        json part = partitions[i];
        int number = part["number"];
        uint64_t length = part["length"];
        if (!length)
            continue;
        uint64_t length_free = part["free"];

        bool excluded = (std::find(__exclude.begin(), __exclude.end(), number) != __exclude.end());
        bool only_used_space = (std::find(__only_used_space.begin(), __only_used_space.end(), number) != __only_used_space.end());
        bool use_vss = (std::find(__use_vss.begin(), __use_vss.end(), number) != __use_vss.end());

        if (excluded) {
            nIncludedAndInusedBytes -= length;
        }
        else if (only_used_space) {
            nIncludedAndInusedBytes -= length_free;
        }
    }
    ULARGE_INTEGER FreeBytesAvailableToCaller;
    CString strFolder = (LPCTSTR)strPath;
    *((LPTSTR)(LPCTSTR)PathFindFileName(strFolder)) = '\0';
    strFolder.ReleaseBuffer();

    if (GetDiskFreeSpaceEx(strFolder, &FreeBytesAvailableToCaller, NULL, NULL)) {
        uint64_t diskSize = property["DiskSize"];
        if (strPath.Right(3).CompareNoCase(_T(".dd")) == 0) {
            if (diskSize > FreeBytesAvailableToCaller.QuadPart) {
                MessageBox(LSTRW(RID_insufficient_diskspace_for_rawimg), LSTRW(RID_TIPS), MB_ICONERROR);
                m_SaveTo.SetFocus();
                return;
            }
        }
        else {
            char szMaxReq[256];
            char szFree[256];
            CStringW strMsg;
            const int imgbasesize = 50 * 1024 * 1024;
            uint64_t max_len_required = imgbasesize + nIncludedAndInusedBytes;
            if (max_len_required > FreeBytesAvailableToCaller.QuadPart) {
                calculateSize1024(max_len_required, szMaxReq, sizeof(szMaxReq));
                calculateSize1024(FreeBytesAvailableToCaller.QuadPart, szFree, sizeof(szFree));
                strMsg.Format(LSTRW(RID_ImageOutputWarning3), szMaxReq, szFree);
                if (MessageBox(strMsg, LSTRW(RID_TIPS), MB_ICONWARNING | MB_YESNO) != IDYES) {
                    m_SaveTo.SetFocus();
                    return;
                }
            }
        }
    }

    CDialogEx::OnOK();
}


void CPartDlg::OnItemchangedListPart(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    *pResult = 0;
}

LRESULT CPartDlg::OnGetListItem(WPARAM wParam, LPARAM lParam)
{
    ASSERT((HWND)wParam == m_cPartList.GetSafeHwnd());

    //lParam is a pointer to the data that 
    //is needed for the element
    CQuickList::CListItemData* data =
        (CQuickList::CListItemData*) lParam;

    //Get which item and subitem that is asked for.
    int item = data->GetItem();
    int subItem = data->GetSubItem();

    //...insert information that is needed in "data"...
    CPartItemData& itemdata = m_data[item];
    UpdateData();

    switch (subItem) {
    case CPartList::COL_Number:
        data->m_text = itemdata.Number;

        data->m_button.m_draw = true;
        data->m_button.m_style = DFCS_BUTTONCHECK;
        if (itemdata.chk_selected)
            data->m_button.m_style |= DFCS_CHECKED;

        break;
    case CPartList::COL_Label:
        data->m_text = itemdata.Label;
        break;
    case CPartList::COL_Size:
        data->m_text = itemdata.Size;
        break;
    case CPartList::COL_Usage:
        data->m_text = itemdata.Usage;
        break;
    case CPartList::COL_1stSector:
        data->m_text = itemdata.firstSector;
        break;
    case CPartList::COL_Bitlocker:
        data->m_text = itemdata.Bitlocker;
        break;
    case CPartList::COL_EntireSpace:
        data->m_text = L"";
        data->m_button.m_draw = true;
        data->m_button.m_style = DFCS_BUTTONCHECK;
        if (itemdata.chk_entireSpace)
            data->m_button.m_style |= DFCS_CHECKED;
        break;
    case CPartList::COL_VSS:
        if (itemdata._supportVSS) {
            data->m_text = L"";
            data->m_button.m_draw = true;
            data->m_button.m_style = DFCS_BUTTONCHECK;
            if (itemdata.chk_useVSS)
                data->m_button.m_style |= DFCS_CHECKED;
        }
        else {
            data->m_text = m_bWow64 ? L"X" : L"";
            data->m_button.m_draw = false;
        }
        break;
    }
    return 0;
}

LRESULT CPartDlg::OnListClick(WPARAM wParam, LPARAM lParam)
{
    ASSERT((HWND)wParam == m_cPartList.GetSafeHwnd());

    CQuickList::CListHitInfo* hit = (CQuickList::CListHitInfo*) lParam;
    CPartItemData& itemdata = m_data[hit->m_item];

    if (hit->m_onButton)
    {
        switch (hit->m_subitem) {
        case CPartList::COL_Number:
            itemdata.chk_selected = !itemdata.chk_selected;
            if (!itemdata.chk_selected) {
                CString strWarning;
                if (!itemdata._isWinPart && !itemdata._mountOn.GetLength() && itemdata._offset < m_winPartOffset) {
                    strWarning.Format(LSTRW(RID_uncheck_part_warning1), (LPCTSTR)m_strWinPart);
                    if (MessageBox(strWarning, LSTRW(RID_TIPS), MB_ICONWARNING | MB_OKCANCEL) != IDOK) {
                        itemdata.chk_selected = !itemdata.chk_selected;
                        return 0;
                    }
                }
                else if (itemdata._isWinPart)
                {
                    strWarning.Format(LSTRW(RID_uncheck_part_warning2),(LPCTSTR)m_strWinPart);
                    if (MessageBox(strWarning, LSTRW(RID_TIPS), MB_ICONWARNING | MB_OKCANCEL) != IDOK) {
                        itemdata.chk_selected = !itemdata.chk_selected;
                        return 0;
                    }
                }
            }
            break;
        case CPartList::COL_EntireSpace:
            itemdata.chk_entireSpace = !itemdata.chk_entireSpace;
            if (itemdata.chk_entireSpace) {
                itemdata.chk_useVSS = false; //“全部空间”和VSS是冲突的，VSS即只备份正在使用的空间
                m_cPartList.RedrawSubitems(hit->m_item, hit->m_item, CPartList::COL_VSS);
            }
            break;
        case CPartList::COL_VSS:
            if (itemdata._supportVSS) {
                itemdata.chk_useVSS = !itemdata.chk_useVSS;
                if (itemdata.chk_useVSS) {
                    itemdata.chk_entireSpace = false;
                }
                m_cPartList.RedrawSubitems(hit->m_item, hit->m_item, CPartList::COL_EntireSpace);
            }
            break;
        }
        m_cPartList.RedrawCheckBoxs(hit->m_item, hit->m_item, hit->m_subitem);
    }

    return 0;
}

BOOL CPartDlg::GetItemConfig(int index, CString& saveto, CString &excludes)
{
    json& disk = m_pjdisks->at(index);

    std::vector<int> __exclude;
    std::vector<int> __only_used_space;
    std::string __saveto;

    if (disk.contains("__only_used_space")) {
        __only_used_space = disk["__only_used_space"].get<std::vector<int>>();
    }
    if (disk.contains("__exclude")) {
        __exclude = disk["__exclude"].get<std::vector<int>>();
    }

    std::stringstream result;
    if (__exclude.size()) {
        std::copy(__exclude.begin(), __exclude.end(), std::ostream_iterator<int>(result, ","));
        std::string result_str = result.str();
        if (*result_str.rbegin() == ',')
            result_str.pop_back();
        excludes.Format(LSTRW(RID_disk_config_exmode_excluded_parts), (LPCTSTR)CA2T(result_str.c_str()));
    } else {
        if (__only_used_space.size()) {
            excludes = LSTRW(RID_disk_config_exmode_onlyused);
        }
        else {
            excludes = LSTRW(RID_disk_config_exmode_allspace);
        }
    }

    if (disk.contains("__saveto")) {
        __saveto = disk["__saveto"].get<std::string>();
        saveto = CA2W(__saveto.c_str(), CP_UTF8);
    }
    
    return 1;
}

BOOL CPartDlg::GetOutputInfo(json& disk, CString& saveto, uint64_t& est_size)
{
    std::vector<int> __exclude;
    std::vector<int> __only_used_space;

    if (disk.contains("__exclude")) {
        __exclude = disk["__exclude"].get<std::vector<int>>();
    }
    if (disk.contains("__only_used_space")) {
        __only_used_space = disk["__only_used_space"].get<std::vector<int>>();
    }
    if (disk.contains("__saveto")) {
        std::string __saveto = disk["__saveto"].get<std::string>();
        saveto = CA2W(__saveto.c_str(), CP_UTF8);
    }
    else {
        return FALSE;
    }

    json& property = disk["property"];
    json& partitions = disk["partitions"];

    uint64_t nIncludedAndInusedBytes = property["DiskSize"];
    int i;
    for (i = 0; i < (int)partitions.size(); i++)
    {
        json part = partitions[i];
        int number = part["number"];
        uint64_t length = part["length"];
        if (!length)
            continue;
        uint64_t length_free = part["free"];

        bool excluded = (std::find(__exclude.begin(), __exclude.end(), number) != __exclude.end());
        bool only_used_space = (std::find(__only_used_space.begin(), __only_used_space.end(), number) != __only_used_space.end());

        if (excluded) {
            nIncludedAndInusedBytes -= length;
        }
        else if (only_used_space) {
            nIncludedAndInusedBytes -= length_free;
        }
    }

    uint64_t diskSize = property["DiskSize"];
    if (saveto.Right(3).CompareNoCase(_T(".dd")) == 0) {
        est_size = diskSize;
    }
    else {
        const int imgbasesize = 50 * 1024 * 1024;
        est_size = imgbasesize + nIncludedAndInusedBytes;
    }
    return 1;
}
