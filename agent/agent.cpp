// agent.cpp : 定义应用程序的入口点。
//

#include "pch.h"
#include "resource.h"
#include "agent.h"
#include "../misc/netutils.h"
#include "../core/osutils.h"
#include "Server.h"
#include "langstr.h"

#define MYWM_MSG  WM_USER+1

// 全局变量:
HWND hMainDlg;
HINSTANCE hInst;                                // 当前实例
class CMyServer* gServer;
void UpdateStatusText(LPCTSTR fmt, ...);

class CMyServer : public CServer
{
public:

    virtual bool OnNewClientConnected(const char* ip, int port) {
        CString str;
        str.Format(LSTRW(RID_Session_Established), m_id, ip, port);
        UpdateStatusText(str);
        return true;
    }
    virtual void req_log_output(int cmd, int err, LPCWSTR lpszDetails) {
        UpdateStatusText(lpszDetails);
    }
};

void ShowLocalIP(LPCWSTR lpszTitle)
{
    char hostname[256] = { 0 };
    gethostname(hostname, 256);
    struct hostent* host = gethostbyname(hostname);
    if (host == NULL)
    {
        return;
    }
    if (host->h_addr_list[0] != 0) {
        UpdateStatusText(lpszTitle);
    }
    for (int i = 0; host->h_addr_list[i] != 0; i++)
    {
        char *ip = inet_ntoa(*(struct in_addr*)host->h_addr_list[i]);
        if (ip) {
            UpdateStatusText(L"(%d) %S", i + 1, ip);
        }
    }
}

void CenterWindow(HWND hDlg)
{
    HWND hwndOwner = NULL;
    RECT rcOwner, rcDlg, rc;
    if ((hwndOwner = GetParent(hDlg)) == NULL)
    {
        hwndOwner = GetDesktopWindow();
    }
    GetWindowRect(hwndOwner, &rcOwner);
    GetWindowRect(hDlg, &rcDlg);
    CopyRect(&rc, &rcOwner);
    OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top);
    OffsetRect(&rc, -rc.left, -rc.top);
    OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom);
    SetWindowPos(hDlg,
        HWND_TOP,
        rcOwner.left + (rc.right / 2),
        rcOwner.top + (rc.bottom / 2),
        0, 0,
        SWP_NOSIZE);
}


void UpdateStatusText(LPCTSTR fmt, ...)
{
    WCHAR buf[1024];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf(buf, _countof(buf) - 3, fmt, ap);
    va_end(ap);
    _tcscat_s(buf, _T("\r\n"));
    PostMessage(hMainDlg, MYWM_MSG, 0, (LPARAM)_wcsdup(buf));
}

INT_PTR CALLBACK DialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        CenterWindow(hDlg);
        SendMessage(GetDlgItem(hDlg, IDC_EDIT_LOG), EM_SETLIMITTEXT, 0, 0);
        SetWindowText(hDlg, TEXT("d2vagent v1.1"));
        
        SetDlgItemInt(hDlg, IDC_EDIT_PORT, 8000, TRUE);
        SetDlgItemText(hDlg, IDOK, LSTRW(RID_Start));
        CheckDlgButton(hDlg, IDC_CHECK_COMPRESS, ENABLE_ZSTD ? BST_CHECKED : 0);
        EnableBackupPrivilege();

        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
        {
            char szIP[128];
            char szPasswd[128];
            int nPort = GetDlgItemInt(hDlg, IDC_EDIT_PORT, NULL, TRUE);
            GetDlgItemTextA(hDlg, IDC_EDIT_IP, szIP, _countof(szIP));
            GetDlgItemTextA(hDlg, IDC_EDIT_PSW, szPasswd, _countof(szPasswd));
            ENABLE_ZSTD = IsDlgButtonChecked(hDlg, IDC_CHECK_COMPRESS) == BST_CHECKED;

            if (gServer) {
                delete gServer;
                gServer = NULL;
                UpdateStatusText(LSTRW(RID_Stopped));
                SetDlgItemText(hDlg, IDOK, LSTRW(RID_Start));
                return 0;
            }
            else
            {
                if (strlen(szPasswd) == 0 && (_stricmp(szIP, "127.0.0.1")!=0) ) {
                    if (MessageBox(hDlg, LSTRW(RID_EmptyPassword_WARNING), LSTRW(RID_Notice),
                        MB_ICONWARNING | MB_YESNO) != IDYES) {
                        return 0;
                    }
                }
                gServer = new CMyServer();
                if (!gServer->Init(szIP, nPort, szPasswd)) {
                    delete gServer;
                    gServer = NULL;
                    UpdateStatusText(LSTRW(RID_StatupFailed));
                    return 0;
                }
                UpdateStatusText(LSTRW(RID_Started));
                SetDlgItemText(hDlg, IDOK, LSTRW(RID_Stop));
                if (!szIP[0] || strcmp(szIP, "0.0.0.0") == 0) {
                    ShowLocalIP(LSTRW(RID_ListenOn));
                }
                else if (strcmp(szIP, "127.0.0.1") == 0) {
                    UpdateStatusText(LSTRW(RID_Notice_listen_localhost));
                }
                if (strlen(szPasswd) > 0) {
                    UpdateStatusText(LSTRW(RID_Notice_encrypted));
                }
                else {
                    UpdateStatusText(LSTRW(RID_Notice_not_encrypted));
                }

            }
        }
        break;
        case IDCANCEL:
        case IDM_EXIT:
            SendMessage(hDlg, WM_CLOSE, 0, 0);
            return TRUE;
            break;
        case IDC_CHECK_COMPRESS:
            ENABLE_ZSTD = IsDlgButtonChecked(hDlg, IDC_CHECK_COMPRESS) == BST_CHECKED;
            break;
        }
        break;
    case MYWM_MSG:
    {
        HWND h = GetDlgItem(hDlg, IDC_EDIT_LOG);
        int nLength = GetWindowTextLength(h);
        enum {
            maxlogsize = 1024 * 1024 * 2
        };
        if (nLength > maxlogsize) {
            ::SendMessageW(h, EM_SETSEL, 0, nLength / 2);
            ::SendMessageW(h, EM_REPLACESEL, (WPARAM)0, (LPARAM)L"");
        }
        nLength = GetWindowTextLength(h);
        ::SendMessageW(h, EM_SETSEL, nLength, nLength);
        ::SendMessageW(h, EM_SCROLLCARET, 0, 0);
        ::SendMessageW(h, EM_REPLACESEL, (WPARAM)0, (LPARAM)lParam);
        free((WCHAR*)lParam);
    }
    break;
    case WM_TIMER:
    {
    }
    break;

    case WM_CLOSE:
        if (gServer) {
            if (MessageBox(hDlg, LSTRW(RID_SureExit), LSTRW(RID_Notice),
                MB_ICONQUESTION | MB_YESNO) == IDYES)
            {
                DestroyWindow(hDlg);
            }
        }
        else {
            DestroyWindow(hDlg);
        }
        return TRUE;

    case WM_DESTROY:
        PostQuitMessage(0);
        return TRUE;
    }

    return FALSE;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    HWND hDlg;
    MSG msg;
    BOOL ret;
    ::hInst = hInst;
    if (GetUserDefaultUILanguage() != MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED) &&
        GetUserDefaultUILanguage() != MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)) {
        SetThreadUILanguage(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
    }
    socket_init_ws32();
    InitCommonControls();
    hMainDlg = hDlg = CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_DIALOG1), 0, DialogProc, 0);
    ShowWindow(hDlg, nCmdShow);

    while ((ret = GetMessage(&msg, 0, 0, 0)) != 0) {
        if (ret == -1)
            break;

        if (!IsDialogMessage(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int) msg.wParam;
}
