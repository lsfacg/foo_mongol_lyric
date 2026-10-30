#include "stdafx.h"
#include <windows.h>
#include <commctrl.h> 
#include <shellapi.h>
#include <string>
#include <vector>
#include <d2d1.h>          
#include <d2d1helper.h>    
#include <SDK/preferences_page.h>
#include <SDK/core_api.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")

// ================= 外部变量 =================
extern int g_fontSize; extern std::wstring g_mongolFont; extern std::wstring g_chineseFont; extern D2D1_COLOR_F g_mongolColor; extern D2D1_COLOR_F g_chineseColor;
extern D2D1_COLOR_F g_mongolOutlineColor; extern D2D1_COLOR_F g_chineseOutlineColor; extern float g_mongolOutlineSize; extern float g_chineseOutlineSize;
extern int g_displayMode; extern bool g_isVisible; extern bool g_isLocked; extern bool g_isTopmost;
extern bool g_desktopShowTip;
extern HWND g_hwndLyrics;

extern int g_panelFontSize; extern std::wstring g_panelMongolFont; extern std::wstring g_panelChineseFont; extern D2D1_COLOR_F g_panelMongolColor; extern D2D1_COLOR_F g_panelChineseColor;
extern D2D1_COLOR_F g_panelHighlightMongolColor; extern D2D1_COLOR_F g_panelHighlightChineseColor; extern D2D1_COLOR_F g_panelBgColor; extern int g_panelDisplayMode;
extern bool g_panelShowTip;

void ApplySettingsAndRefresh(); extern void SavePanelSettings();
extern void SaveSettings(HWND hwnd);
#define WM_APP_SETTINGS_CHANGED (WM_APP + 1)

extern const GUID guid_preferences_page = { 0x9C3D2A1B, 0x8B4E, 0x4C2D,{ 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x1A, 0x2B } };
extern const GUID guid_pref_desktop = { 0x9C3D2A1B, 0x8B4E, 0x4C2D,{ 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x1A, 0x2C } };
extern const GUID guid_pref_panel = { 0x9C3D2A1B, 0x8B4E, 0x4C2D,{ 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x1A, 0x2E } };

int CALLBACK EnumFontProc(const LOGFONTW* lpelfe, const TEXTMETRICW* lpntme, DWORD FontType, LPARAM lParam) {
    if (lpelfe->lfFaceName[0] == L'@') return 1;
    HWND hCombo = (HWND)lParam;
    if (SendMessageW(hCombo, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)lpelfe->lfFaceName) == CB_ERR) {
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)lpelfe->lfFaceName);
    }
    return 1;
}

void PopulateFontsCombo(HWND hCombo, const std::wstring& defaultFont) {
    HDC hdc = GetDC(NULL);
    LOGFONTW lf = { 0 };
    lf.lfCharSet = DEFAULT_CHARSET;
    EnumFontFamiliesExW(hdc, &lf, EnumFontProc, (LPARAM)hCombo, 0);
    ReleaseDC(NULL, hdc);

    if (SendMessageW(hCombo, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)L"Microsoft YaHei") == CB_ERR) {
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Microsoft YaHei");
    }

    int idx = (int)SendMessage(hCombo, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)defaultFont.c_str());
    if (idx != CB_ERR) {
        SendMessage(hCombo, CB_SETCURSEL, idx, 0);
    }
    else {
        idx = (int)SendMessage(hCombo, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)L"微软雅黑");
        if (idx == CB_ERR) idx = (int)SendMessage(hCombo, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)L"Microsoft YaHei UI");
        SendMessage(hCombo, CB_SETCURSEL, (idx != CB_ERR) ? idx : 0, 0);
    }
}

// =========================================================================
// 1. 通用父页面
// =========================================================================
class MongolPrefsRootInstance : public preferences_page_instance {
private:
    HWND m_hwnd, m_hTitle, m_hLink, m_hAuthor;
    preferences_page_callback::ptr m_callback;
    WNDPROC m_oldWndProc = nullptr;
    HFONT m_hFontLarge, m_hFontSmall;

    static LRESULT CALLBACK PrefsWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        MongolPrefsRootInstance* pThis = (MongolPrefsRootInstance*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (pThis) {
            if (msg == WM_SIZE) {
                RECT rc; GetClientRect(hwnd, &rc);
                if (pThis->m_hTitle) {
                    int titleW = 400; int titleH = 50;
                    SetWindowPos(pThis->m_hTitle, NULL, (rc.right - titleW) / 2 - 12, (rc.bottom - titleH) / 2 - 105, titleW, titleH, SWP_NOZORDER);
                }
                if (pThis->m_hLink && pThis->m_hAuthor) {
                    int bottomY = rc.bottom - 30;
                    SetWindowPos(pThis->m_hLink, NULL, 20, bottomY, 100, 30, SWP_NOZORDER);
                    SetWindowPos(pThis->m_hAuthor, NULL, 125, bottomY - 5, 300, 30, SWP_NOZORDER);
                }
            }
            else if (msg == WM_NOTIFY) {
                LPNMHDR pHdr = (LPNMHDR)lp;
                if (pHdr->code == NM_CLICK && pHdr->idFrom == 2001) {
                    PNMLINK pNMLink = (PNMLINK)lp;
                    ShellExecuteW(NULL, L"open", pNMLink->item.szUrl, NULL, NULL, SW_SHOWNORMAL);
                    return 0;
                }
            }
            else if (msg == WM_CTLCOLORSTATIC) {
                HDC hdc = (HDC)wp;
                SetTextColor(hdc, GetSysColor(COLOR_BTNTEXT));
                SetBkMode(hdc, TRANSPARENT);
                return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
            }
        }
        if (pThis && pThis->m_oldWndProc) return CallWindowProc(pThis->m_oldWndProc, hwnd, msg, wp, lp);
        return DefWindowProc(hwnd, msg, wp, lp);
    }

public:
    MongolPrefsRootInstance(HWND p_parent, preferences_page_callback::ptr p_callback) : m_hwnd(nullptr), m_callback(p_callback) {
        HINSTANCE hInst = core_api::get_my_instance();
        m_hwnd = CreateWindowExW(0, WC_DIALOG, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, 500, 300, p_parent, nullptr, hInst, nullptr);
        if (m_hwnd) {
            SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);
            m_oldWndProc = (WNDPROC)SetWindowLongPtr(m_hwnd, GWLP_WNDPROC, (LONG_PTR)PrefsWndProc);
        }

        m_hFontLarge = CreateFontW(27, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_ROMAN, L"SimSun");
        m_hFontSmall = CreateFontW(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");

        m_hTitle = CreateWindowW(L"STATIC", L"请选择一个子页面。", WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE, 0, 0, 0, 0, m_hwnd, nullptr, hInst, nullptr);
        SendMessage(m_hTitle, WM_SETFONT, (WPARAM)m_hFontLarge, TRUE);

        m_hLink = CreateWindowW(L"SysLink", L"<a href=\"https://github.com/lsfacg/foo_mongol_lyric\">GitHub 发布页</a>", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0, m_hwnd, (HMENU)2001, hInst, nullptr);
        SendMessage(m_hLink, WM_SETFONT, (WPARAM)m_hFontSmall, TRUE);

        m_hAuthor = CreateWindowW(L"STATIC", L"插件作者：五彩传说-LSF爱唱歌", WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, 0, 0, 0, 0, m_hwnd, nullptr, hInst, nullptr);
        SendMessage(m_hAuthor, WM_SETFONT, (WPARAM)m_hFontSmall, TRUE);

        RECT rc; GetClientRect(m_hwnd, &rc);
        SendMessage(m_hwnd, WM_SIZE, 0, MAKELPARAM(rc.right, rc.bottom));
    }

    ~MongolPrefsRootInstance() {
        if (m_hwnd) { SetWindowLongPtr(m_hwnd, GWLP_WNDPROC, (LONG_PTR)m_oldWndProc); DestroyWindow(m_hwnd); }
        if (m_hFontLarge) DeleteObject(m_hFontLarge);
        if (m_hFontSmall) DeleteObject(m_hFontSmall);
    }
    virtual HWND get_wnd() override { return m_hwnd; }
    virtual t_uint32 get_state() override { return preferences_state::resettable; }
    virtual void apply() override {}
    virtual void reset() override {}
};

class MongolPrefsRootPage : public preferences_page_v3 {
public:
    virtual preferences_page_instance::ptr instantiate(HWND p_parent, preferences_page_callback::ptr p_callback) override { return new service_impl_t<MongolPrefsRootInstance>(p_parent, p_callback); }
    const char* get_name() override { return "\xE8\x92\x99\xE5\x8F\xA4\xE6\x96\x87\xE6\xAD\x8C\xE8\xAF\x8D"; }
    GUID get_guid() override { return guid_preferences_page; }
    GUID get_parent_guid() override { return preferences_page::guid_tools; }
};
static preferences_page_factory_t<MongolPrefsRootPage> g_root_page_factory;

// =========================================================================
// 2. 桌面歌词设置
// =========================================================================
class MongolPrefsDesktopInstance : public preferences_page_instance {
private:
    HWND m_hwnd; preferences_page_callback::ptr m_callback;
    HWND m_hSliderFontSize, m_hLabelFontSize, m_hComboDisplayMode;
    HWND m_hChkVisible, m_hChkLock, m_hChkTopmost;
    HWND m_hChkShowTip;
    HWND m_hComboFontM, m_hBtnColorM, m_hBtnOutlineColorM, m_hSliderOutlineM, m_hLabelOutlineM;
    HWND m_hComboFontC, m_hBtnColorC, m_hBtnOutlineColorC, m_hSliderOutlineC, m_hLabelOutlineC;
    bool m_dirty = false; WNDPROC m_oldWndProc = nullptr; HFONT m_hFontBold;

    // 【新增】暂存颜色变量
    D2D1_COLOR_F m_tempMongolColor, m_tempChineseColor, m_tempMongolOutlineColor, m_tempChineseOutlineColor;

    static LRESULT CALLBACK PrefsWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        MongolPrefsDesktopInstance* pThis = (MongolPrefsDesktopInstance*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (pThis) {
            if (msg == WM_MOUSEWHEEL) { short zDelta = GET_WHEEL_DELTA_WPARAM(wp); int scrollCode = (zDelta > 0) ? SB_LINEUP : SB_LINEDOWN; for (int i = 0; i < 3; ++i) SendMessage(hwnd, WM_VSCROLL, MAKEWPARAM(scrollCode, 0), 0); return 0; }
            else if (msg == WM_DRAWITEM) {
                LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lp;
                if (pDIS->CtlID == 2001 || pDIS->CtlID == 2003 || pDIS->CtlID == 2002 || pDIS->CtlID == 2004) {
                    // 【修改】读取临时颜色
                    D2D1_COLOR_F color = (pDIS->CtlID == 2001) ? pThis->m_tempMongolColor : (pDIS->CtlID == 2003) ? pThis->m_tempMongolOutlineColor : (pDIS->CtlID == 2002) ? pThis->m_tempChineseColor : pThis->m_tempChineseOutlineColor;
                    HBRUSH hBrush = CreateSolidBrush(RGB((BYTE)(color.r * 255), (BYTE)(color.g * 255), (BYTE)(color.b * 255))); FillRect(pDIS->hDC, &pDIS->rcItem, hBrush); DeleteObject(hBrush);
                    FrameRect(pDIS->hDC, &pDIS->rcItem, (HBRUSH)GetStockObject(BLACK_BRUSH)); if (pDIS->itemState & ODS_SELECTED) { RECT rc = pDIS->rcItem; InflateRect(&rc, -1, -1); DrawFocusRect(pDIS->hDC, &rc); } return TRUE;
                }
            }
            else if (msg == WM_VSCROLL) {
                int nScrollCode = LOWORD(wp); SCROLLINFO si = { sizeof(si) }; si.fMask = SIF_ALL; GetScrollInfo(hwnd, SB_VERT, &si); int oldPos = si.nPos;
                switch (nScrollCode) { case SB_TOP: si.nPos = si.nMin; break; case SB_BOTTOM: si.nPos = si.nMax; break; case SB_LINEUP: si.nPos -= 30; break; case SB_LINEDOWN: si.nPos += 30; break; case SB_PAGEUP: si.nPos -= si.nPage; break; case SB_PAGEDOWN: si.nPos += si.nPage; break; case SB_THUMBTRACK: si.nPos = si.nTrackPos; break; default: return 0; }
                                                  if (si.nPos < si.nMin) si.nPos = si.nMin; if (si.nPos > si.nMax - (int)si.nPage + 1) si.nPos = si.nMax - (int)si.nPage + 1;
                                                  si.fMask = SIF_POS; SetScrollInfo(hwnd, SB_VERT, &si, TRUE); GetScrollInfo(hwnd, SB_VERT, &si);
                                                  if (si.nPos != oldPos) { ScrollWindowEx(hwnd, 0, oldPos - si.nPos, NULL, NULL, NULL, NULL, SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE); UpdateWindow(hwnd); } return 0;
            }
            else if (msg == WM_HSCROLL) {
                HWND hCtrl = (HWND)lp;
                if (hCtrl == pThis->m_hSliderFontSize) { wchar_t buf[32]; swprintf_s(buf, L"%d", (int)SendMessage(hCtrl, TBM_GETPOS, 0, 0)); SetWindowTextW(pThis->m_hLabelFontSize, buf); }
                else if (hCtrl == pThis->m_hSliderOutlineM) { wchar_t buf[32]; swprintf_s(buf, L"%.1f", SendMessage(hCtrl, TBM_GETPOS, 0, 0) / 10.0f); SetWindowTextW(pThis->m_hLabelOutlineM, buf); }
                else if (hCtrl == pThis->m_hSliderOutlineC) { wchar_t buf[32]; swprintf_s(buf, L"%.1f", SendMessage(hCtrl, TBM_GETPOS, 0, 0) / 10.0f); SetWindowTextW(pThis->m_hLabelOutlineC, buf); }
                pThis->m_dirty = true; if (pThis->m_callback.is_valid()) pThis->m_callback->on_state_changed(); return 0;
            }
            else if (msg == WM_COMMAND) {
                int id = LOWORD(wp); int code = HIWORD(wp);
                if ((id == 1003 && code == CBN_SELCHANGE) || (id == 1004 && code == CBN_SELCHANGE) || (id == 1005 && code == CBN_SELCHANGE) || ((id == 1007 || id == 1008 || id == 1009 || id == 1010) && code == BN_CLICKED)) { pThis->m_dirty = true; if (pThis->m_callback.is_valid()) pThis->m_callback->on_state_changed(); }
                else if ((id == 2001 || id == 2003 || id == 2002 || id == 2004) && code == BN_CLICKED) {
                    CHOOSECOLORW cc = { 0 }; static COLORREF customColors[16] = { 0 }; cc.lStructSize = sizeof(cc); cc.hwndOwner = hwnd; cc.lpCustColors = customColors; cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                    // 【修改】指向临时颜色
                    D2D1_COLOR_F* targetColor = (id == 2001) ? &pThis->m_tempMongolColor : (id == 2003) ? &pThis->m_tempMongolOutlineColor : (id == 2002) ? &pThis->m_tempChineseColor : &pThis->m_tempChineseOutlineColor;
                    cc.rgbResult = RGB((BYTE)(targetColor->r * 255), (BYTE)(targetColor->g * 255), (BYTE)(targetColor->b * 255));
                    if (ChooseColorW(&cc)) { *targetColor = D2D1::ColorF(GetRValue(cc.rgbResult) / 255.0f, GetGValue(cc.rgbResult) / 255.0f, GetBValue(cc.rgbResult) / 255.0f); InvalidateRect((HWND)lp, NULL, TRUE); pThis->m_dirty = true; if (pThis->m_callback.is_valid()) pThis->m_callback->on_state_changed(); }
                }
            }
        }
        if (pThis && pThis->m_oldWndProc) return CallWindowProc(pThis->m_oldWndProc, hwnd, msg, wp, lp);
        return DefWindowProc(hwnd, msg, wp, lp);
    }
public:
    MongolPrefsDesktopInstance(HWND p_parent, preferences_page_callback::ptr p_callback) : m_hwnd(nullptr), m_callback(p_callback), m_hFontBold(nullptr) {
        // 【新增】初始化临时颜色
        m_tempMongolColor = g_mongolColor;
        m_tempChineseColor = g_chineseColor;
        m_tempMongolOutlineColor = g_mongolOutlineColor;
        m_tempChineseOutlineColor = g_chineseOutlineColor;

        HINSTANCE hInst = core_api::get_my_instance(); m_hwnd = CreateWindowExW(0, WC_DIALOG, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_VSCROLL, 0, 0, 500, 500, p_parent, nullptr, hInst, nullptr);
        if (m_hwnd) { SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this); m_oldWndProc = (WNDPROC)SetWindowLongPtr(m_hwnd, GWLP_WNDPROC, (LONG_PTR)PrefsWndProc); }
        m_hFontBold = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");
        int labelW = 130, ctrlX = 160, gap = 45, y = 20; HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HWND hTitleBase = CreateWindowW(L"STATIC", L"歌词窗口控制", WS_CHILD | WS_VISIBLE, 20, y, 120, 25, m_hwnd, nullptr, hInst, nullptr); SendMessage(hTitleBase, WM_SETFONT, (WPARAM)m_hFontBold, TRUE); CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ, 140, y + 12, 320, 2, m_hwnd, nullptr, hInst, nullptr);
        y += 45; CreateWindowW(L"STATIC", L"\u5168\u5C40\u5B57\u4F53\u5927\u5C0F:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr); m_hSliderFontSize = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS, ctrlX, y - 5, 150, 30, m_hwnd, (HMENU)1001, hInst, nullptr); m_hLabelFontSize = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, ctrlX + 160, y, 50, 20, m_hwnd, nullptr, hInst, nullptr); SendMessage(m_hSliderFontSize, TBM_SETRANGE, TRUE, MAKELONG(12, 100)); SendMessage(m_hSliderFontSize, TBM_SETPOS, TRUE, g_fontSize);
        y += gap; CreateWindowW(L"STATIC", L"\u53CC\u8BED\u6B4C\u8BCD\u663E\u793A:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr); m_hComboDisplayMode = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, ctrlX, y - 3, 200, 200, m_hwnd, (HMENU)1003, hInst, nullptr); SendMessage(m_hComboDisplayMode, CB_ADDSTRING, 0, (LPARAM)L"\u663E\u793A\u53CC\u8BED\u6B4C\u8BCD\uFF08\u5982\u679C\u6709\uFF09"); SendMessage(m_hComboDisplayMode, CB_ADDSTRING, 0, (LPARAM)L"\u4EC5\u663E\u793A\u8499\u53E4\u6587"); SendMessage(m_hComboDisplayMode, CB_ADDSTRING, 0, (LPARAM)L"\u4EC5\u663E\u793A\u4E2D\u6587\u7FFB\u8BD1"); SendMessage(m_hComboDisplayMode, CB_SETCURSEL, g_displayMode, 0);
        y += gap; CreateWindowW(L"STATIC", L"\u7A97\u53E3\u63A7\u5236:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr); m_hChkVisible = CreateWindowW(L"BUTTON", L"\u542F\u7528\u684C\u9762\u6B4C\u8BCD", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, ctrlX, y, 200, 20, m_hwnd, (HMENU)1007, hInst, nullptr); SendMessage(m_hChkVisible, BM_SETCHECK, g_isVisible ? BST_CHECKED : BST_UNCHECKED, 0);
        y += 30; m_hChkLock = CreateWindowW(L"BUTTON", L"\u9501\u5B9A\u684C\u9762\u6B4C\u8BCD", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, ctrlX, y, 200, 20, m_hwnd, (HMENU)1008, hInst, nullptr); SendMessage(m_hChkLock, BM_SETCHECK, g_isLocked ? BST_CHECKED : BST_UNCHECKED, 0);
        y += 30; m_hChkTopmost = CreateWindowW(L"BUTTON", L"\u7F6E\u9876\u6B4C\u8BCD\u7A97\u53E3", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, ctrlX, y, 200, 20, m_hwnd, (HMENU)1009, hInst, nullptr); SendMessage(m_hChkTopmost, BM_SETCHECK, g_isTopmost ? BST_CHECKED : BST_UNCHECKED, 0);
        y += 30; m_hChkShowTip = CreateWindowW(L"BUTTON", L"\u663E\u793A\u72B6\u6001\u63D0\u793A", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, ctrlX, y, 200, 20, m_hwnd, (HMENU)1010, hInst, nullptr); SendMessage(m_hChkShowTip, BM_SETCHECK, g_desktopShowTip ? BST_CHECKED : BST_UNCHECKED, 0);

        y += 50; HWND hTitleM = CreateWindowW(L"STATIC", L"蒙古文设置", WS_CHILD | WS_VISIBLE, 20, y, 100, 25, m_hwnd, nullptr, hInst, nullptr); SendMessage(hTitleM, WM_SETFONT, (WPARAM)m_hFontBold, TRUE); CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ, 120, y + 12, 340, 2, m_hwnd, nullptr, hInst, nullptr);
        y += 45; CreateWindowW(L"STATIC", L"\u8499\u53E4\u6587\u5B57\u4F53:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr); m_hComboFontM = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, ctrlX, y - 3, 250, 200, m_hwnd, (HMENU)1004, hInst, nullptr); PopulateFontsCombo(m_hComboFontM, g_mongolFont);
        y += gap; CreateWindowW(L"STATIC", L"\u8499\u53E4\u6587\u5B57\u4F53\u989C\u8272:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr); m_hBtnColorM = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, ctrlX, y - 3, 80, 25, m_hwnd, (HMENU)2001, hInst, nullptr);
        y += gap; CreateWindowW(L"STATIC", L"\u8499\u53E4\u6587\u8FB9\u6846\u989C\u8272:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr); m_hBtnOutlineColorM = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, ctrlX, y - 3, 80, 25, m_hwnd, (HMENU)2003, hInst, nullptr);
        y += gap; CreateWindowW(L"STATIC", L"\u8499\u53E4\u6587\u8FB9\u6846\u7C97\u7EC6:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr); m_hSliderOutlineM = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS, ctrlX, y - 5, 150, 30, m_hwnd, (HMENU)1006, hInst, nullptr); m_hLabelOutlineM = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, ctrlX + 160, y, 50, 20, m_hwnd, nullptr, hInst, nullptr); SendMessage(m_hSliderOutlineM, TBM_SETRANGE, TRUE, MAKELONG(0, 30)); SendMessage(m_hSliderOutlineM, TBM_SETPOS, TRUE, (int)(g_mongolOutlineSize * 10)); wchar_t bufM[32]; swprintf_s(bufM, L"%.1f", g_mongolOutlineSize); SetWindowTextW(m_hLabelOutlineM, bufM);

        y += 60; HWND hTitleC = CreateWindowW(L"STATIC", L"中文设置", WS_CHILD | WS_VISIBLE, 20, y, 100, 25, m_hwnd, nullptr, hInst, nullptr); SendMessage(hTitleC, WM_SETFONT, (WPARAM)m_hFontBold, TRUE); CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ, 120, y + 12, 340, 2, m_hwnd, nullptr, hInst, nullptr);
        y += 45; CreateWindowW(L"STATIC", L"\u4E2D\u6587\u5B57\u4F53:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr); m_hComboFontC = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, ctrlX, y - 3, 250, 200, m_hwnd, (HMENU)1005, hInst, nullptr); PopulateFontsCombo(m_hComboFontC, g_chineseFont);
        y += gap; CreateWindowW(L"STATIC", L"\u4E2D\u6587\u5B57\u4F53\u989C\u8272:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr); m_hBtnColorC = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, ctrlX, y - 3, 80, 25, m_hwnd, (HMENU)2002, hInst, nullptr);
        y += gap; CreateWindowW(L"STATIC", L"\u4E2D\u6587\u8FB9\u6846\u989C\u8272:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr); m_hBtnOutlineColorC = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, ctrlX, y - 3, 80, 25, m_hwnd, (HMENU)2004, hInst, nullptr);
        y += gap; CreateWindowW(L"STATIC", L"\u4E2D\u6587\u8FB9\u6846\u7C97\u7EC6:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr); m_hSliderOutlineC = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS, ctrlX, y - 5, 150, 30, m_hwnd, (HMENU)1007, hInst, nullptr); m_hLabelOutlineC = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, ctrlX + 160, y, 50, 20, m_hwnd, nullptr, hInst, nullptr); SendMessage(m_hSliderOutlineC, TBM_SETRANGE, TRUE, MAKELONG(0, 30)); SendMessage(m_hSliderOutlineC, TBM_SETPOS, TRUE, (int)(g_chineseOutlineSize * 10)); wchar_t bufC[32]; swprintf_s(bufC, L"%.1f", g_chineseOutlineSize); SetWindowTextW(m_hLabelOutlineC, bufC);

        wchar_t bufFS[32]; swprintf_s(bufFS, L"%d", g_fontSize); SetWindowTextW(m_hLabelFontSize, bufFS);
        y += 50; RECT rc; GetClientRect(m_hwnd, &rc); SCROLLINFO si = { sizeof(SCROLLINFO) }; si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS; si.nMin = 0; si.nMax = y; si.nPage = rc.bottom; si.nPos = 0; SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);

        HWND child = GetWindow(m_hwnd, GW_CHILD);
        while (child) { SendMessage(child, WM_SETFONT, (WPARAM)hFont, TRUE); child = GetWindow(child, GW_HWNDNEXT); }
        SendMessage(hTitleBase, WM_SETFONT, (WPARAM)m_hFontBold, TRUE); SendMessage(hTitleM, WM_SETFONT, (WPARAM)m_hFontBold, TRUE); SendMessage(hTitleC, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);
    }

    ~MongolPrefsDesktopInstance() { if (m_hwnd) { SetWindowLongPtr(m_hwnd, GWLP_WNDPROC, (LONG_PTR)m_oldWndProc); DestroyWindow(m_hwnd); } if (m_hFontBold) DeleteObject(m_hFontBold); }
    virtual HWND get_wnd() override { return m_hwnd; }
    virtual t_uint32 get_state() override { return preferences_state::resettable | (m_dirty ? preferences_state::changed : 0); }

    virtual void apply() override {
        g_fontSize = (int)SendMessage(m_hSliderFontSize, TBM_GETPOS, 0, 0); g_displayMode = (int)SendMessage(m_hComboDisplayMode, CB_GETCURSEL, 0, 0); g_isVisible = (SendMessage(m_hChkVisible, BM_GETCHECK, 0, 0) == BST_CHECKED);
        g_isLocked = (SendMessage(m_hChkLock, BM_GETCHECK, 0, 0) == BST_CHECKED); g_isTopmost = (SendMessage(m_hChkTopmost, BM_GETCHECK, 0, 0) == BST_CHECKED);
        g_desktopShowTip = (SendMessage(m_hChkShowTip, BM_GETCHECK, 0, 0) == BST_CHECKED);
        int idxM = (int)SendMessage(m_hComboFontM, CB_GETCURSEL, 0, 0); if (idxM != CB_ERR) { wchar_t buf[256]; SendMessage(m_hComboFontM, CB_GETLBTEXT, idxM, (LPARAM)buf); g_mongolFont = buf; }
        g_mongolOutlineSize = (int)SendMessage(m_hSliderOutlineM, TBM_GETPOS, 0, 0) / 10.0f;
        int idxC = (int)SendMessage(m_hComboFontC, CB_GETCURSEL, 0, 0); if (idxC != CB_ERR) { wchar_t buf[256]; SendMessage(m_hComboFontC, CB_GETLBTEXT, idxC, (LPARAM)buf); g_chineseFont = buf; }
        g_chineseOutlineSize = (int)SendMessage(m_hSliderOutlineC, TBM_GETPOS, 0, 0) / 10.0f;

        // 【修改】点击应用时，生效颜色
        g_mongolColor = m_tempMongolColor;
        g_chineseColor = m_tempChineseColor;
        g_mongolOutlineColor = m_tempMongolOutlineColor;
        g_chineseOutlineColor = m_tempChineseOutlineColor;

        ApplySettingsAndRefresh(); m_dirty = false; if (m_callback.is_valid()) m_callback->on_state_changed();
    }

    virtual void reset() override {
        SendMessage(m_hSliderFontSize, TBM_SETPOS, TRUE, 40); SendMessage(m_hComboDisplayMode, CB_SETCURSEL, 0, 0); g_isVisible = true; SendMessage(m_hChkVisible, BM_SETCHECK, BST_CHECKED, 0); SendMessage(m_hChkLock, BM_SETCHECK, BST_UNCHECKED, 0); SendMessage(m_hChkTopmost, BM_SETCHECK, BST_CHECKED, 0); SetWindowTextW(m_hLabelFontSize, L"40");
        g_desktopShowTip = true; SendMessage(m_hChkShowTip, BM_SETCHECK, BST_CHECKED, 0);

        g_mongolFont = L"Mongolian Baiti"; g_chineseFont = L"Microsoft YaHei";
        int idxFontM = (int)SendMessage(m_hComboFontM, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)L"Mongolian Baiti");
        SendMessage(m_hComboFontM, CB_SETCURSEL, (idxFontM != CB_ERR) ? idxFontM : 0, 0);

        int idxFontC = (int)SendMessage(m_hComboFontC, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)L"Microsoft YaHei");
        if (idxFontC == CB_ERR) idxFontC = (int)SendMessage(m_hComboFontC, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)L"微软雅黑");
        if (idxFontC == CB_ERR) idxFontC = (int)SendMessage(m_hComboFontC, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)L"Microsoft YaHei UI");
        SendMessage(m_hComboFontC, CB_SETCURSEL, (idxFontC != CB_ERR) ? idxFontC : 0, 0);

        SendMessage(m_hSliderOutlineM, TBM_SETPOS, TRUE, 7); SetWindowTextW(m_hLabelOutlineM, L"0.7");
        SendMessage(m_hSliderOutlineC, TBM_SETPOS, TRUE, 7); SetWindowTextW(m_hLabelOutlineC, L"0.7");

        // 【修改】只重置临时颜色和UI
        m_tempMongolColor = D2D1::ColorF(0.0f, 1.0f, 0.0f, 1.0f);
        m_tempMongolOutlineColor = D2D1::ColorF(0, 0, 0, 1);
        m_tempChineseColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        m_tempChineseOutlineColor = D2D1::ColorF(0, 0, 0, 1);

        InvalidateRect(m_hBtnColorM, NULL, TRUE); InvalidateRect(m_hBtnOutlineColorM, NULL, TRUE);
        InvalidateRect(m_hBtnColorC, NULL, TRUE); InvalidateRect(m_hBtnOutlineColorC, NULL, TRUE);

        // 【新增：重置时，让桌面歌词窗口也归位到屏幕右侧】
        if (g_hwndLyrics) {
            int screenWidth = GetSystemMetrics(SM_CXSCREEN);
            int defaultX = screenWidth - 250 - 50;
            SetWindowPos(g_hwndLyrics, NULL, defaultX, 150, 250, 800, SWP_NOZORDER);
        }

        m_dirty = true; if (m_callback.is_valid()) m_callback->on_state_changed();
    }
};

class MongolPrefsDesktopPage : public preferences_page_v3 {
public:
    virtual preferences_page_instance::ptr instantiate(HWND p_parent, preferences_page_callback::ptr p_callback) override { return new service_impl_t<MongolPrefsDesktopInstance>(p_parent, p_callback); }
    const char* get_name() override { return "\xE6\xA1\x8C\xE9\x9D\xA2\xE6\xAD\x8C\xE8\xAF\x8D\xE8\xAE\xBE\xE7\xBD\xAE"; }
    GUID get_guid() override { return guid_pref_desktop; }
    GUID get_parent_guid() override { return guid_preferences_page; }
};
static preferences_page_factory_t<MongolPrefsDesktopPage> g_desktop_page_factory;

// =========================================================================
// 3. 面板歌词设置子页面
// =========================================================================
class MongolPrefsPanelInstance : public preferences_page_instance {
private:
    HWND m_hwnd, m_hSliderFontSize, m_hLabelFontSize, m_hComboDisplayMode, m_hBtnColorBg, m_hComboFontM, m_hBtnColorM, m_hBtnColorHM, m_hComboFontC, m_hBtnColorC, m_hBtnColorHC;
    HWND m_hChkShowTip;
    preferences_page_callback::ptr m_callback; bool m_dirty = false; WNDPROC m_oldWndProc = nullptr; HFONT m_hFontBold;

    // 【新增】暂存颜色变量
    D2D1_COLOR_F m_tempPanelMongolColor, m_tempPanelChineseColor, m_tempPanelHighlightMongolColor, m_tempPanelHighlightChineseColor, m_tempPanelBgColor;

    static LRESULT CALLBACK PrefsWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        MongolPrefsPanelInstance* pThis = (MongolPrefsPanelInstance*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (pThis) {
            if (msg == WM_MOUSEWHEEL) { short zDelta = GET_WHEEL_DELTA_WPARAM(wp); int scrollCode = (zDelta > 0) ? SB_LINEUP : SB_LINEDOWN; for (int i = 0; i < 3; ++i) SendMessage(hwnd, WM_VSCROLL, MAKEWPARAM(scrollCode, 0), 0); return 0; }
            else if (msg == WM_DRAWITEM) {
                LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lp;
                if (pDIS->CtlID == 3001 || pDIS->CtlID == 3002 || pDIS->CtlID == 3004 || pDIS->CtlID == 3008 || pDIS->CtlID == 3009) {
                    // 【修改】读取临时颜色
                    D2D1_COLOR_F color = (pDIS->CtlID == 3001) ? pThis->m_tempPanelMongolColor : (pDIS->CtlID == 3002) ? pThis->m_tempPanelChineseColor : (pDIS->CtlID == 3004) ? pThis->m_tempPanelBgColor : (pDIS->CtlID == 3008) ? pThis->m_tempPanelHighlightMongolColor : pThis->m_tempPanelHighlightChineseColor;
                    HBRUSH hBrush = CreateSolidBrush(RGB((BYTE)(color.r * 255), (BYTE)(color.g * 255), (BYTE)(color.b * 255))); FillRect(pDIS->hDC, &pDIS->rcItem, hBrush); DeleteObject(hBrush);
                    FrameRect(pDIS->hDC, &pDIS->rcItem, (HBRUSH)GetStockObject(BLACK_BRUSH)); return TRUE;
                }
            }
            else if (msg == WM_VSCROLL) {
                int nScrollCode = LOWORD(wp); SCROLLINFO si = { sizeof(si) }; si.fMask = SIF_ALL; GetScrollInfo(hwnd, SB_VERT, &si); int oldPos = si.nPos;
                switch (nScrollCode) { case SB_TOP: si.nPos = si.nMin; break; case SB_BOTTOM: si.nPos = si.nMax; break; case SB_LINEUP: si.nPos -= 30; break; case SB_LINEDOWN: si.nPos += 30; break; case SB_PAGEUP: si.nPos -= si.nPage; break; case SB_PAGEDOWN: si.nPos += si.nPage; break; case SB_THUMBTRACK: si.nPos = si.nTrackPos; break; default: return 0; }
                                                  if (si.nPos < si.nMin) si.nPos = si.nMin; if (si.nPos > si.nMax - (int)si.nPage + 1) si.nPos = si.nMax - (int)si.nPage + 1;
                                                  si.fMask = SIF_POS; SetScrollInfo(hwnd, SB_VERT, &si, TRUE); GetScrollInfo(hwnd, SB_VERT, &si);
                                                  if (si.nPos != oldPos) { ScrollWindowEx(hwnd, 0, oldPos - si.nPos, NULL, NULL, NULL, NULL, SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE); UpdateWindow(hwnd); } return 0;
            }
            else if (msg == WM_HSCROLL) {
                HWND hCtrl = (HWND)lp;
                if (hCtrl == pThis->m_hSliderFontSize) { wchar_t buf[32]; swprintf_s(buf, L"%d", (int)SendMessage(hCtrl, TBM_GETPOS, 0, 0)); SetWindowTextW(pThis->m_hLabelFontSize, buf); pThis->m_dirty = true; if (pThis->m_callback.is_valid()) pThis->m_callback->on_state_changed(); }
                return 0;
            }
            else if (msg == WM_COMMAND) {
                int id = LOWORD(wp); int code = HIWORD(wp);
                if ((id == 3010 || id == 3006 || id == 3007) && code == CBN_SELCHANGE) { pThis->m_dirty = true; if (pThis->m_callback.is_valid()) pThis->m_callback->on_state_changed(); }
                else if (id == 3011 && code == BN_CLICKED) { pThis->m_dirty = true; if (pThis->m_callback.is_valid()) pThis->m_callback->on_state_changed(); }
                else if ((id == 3001 || id == 3002 || id == 3004 || id == 3008 || id == 3009) && code == BN_CLICKED) {
                    CHOOSECOLORW cc = { 0 }; static COLORREF customColors[16] = { 0 }; cc.lStructSize = sizeof(cc); cc.hwndOwner = hwnd; cc.lpCustColors = customColors; cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                    // 【修改】指向临时颜色
                    D2D1_COLOR_F* targetColor = (id == 3001) ? &pThis->m_tempPanelMongolColor : (id == 3002) ? &pThis->m_tempPanelChineseColor : (id == 3004) ? &pThis->m_tempPanelBgColor : (id == 3008) ? &pThis->m_tempPanelHighlightMongolColor : &pThis->m_tempPanelHighlightChineseColor;
                    cc.rgbResult = RGB((BYTE)(targetColor->r * 255), (BYTE)(targetColor->g * 255), (BYTE)(targetColor->b * 255));
                    if (ChooseColorW(&cc)) { *targetColor = D2D1::ColorF(GetRValue(cc.rgbResult) / 255.0f, GetGValue(cc.rgbResult) / 255.0f, GetBValue(cc.rgbResult) / 255.0f); InvalidateRect((HWND)lp, NULL, TRUE); pThis->m_dirty = true; if (pThis->m_callback.is_valid()) pThis->m_callback->on_state_changed(); }
                }
            }
        }
        if (pThis && pThis->m_oldWndProc) return CallWindowProc(pThis->m_oldWndProc, hwnd, msg, wp, lp);
        return DefWindowProc(hwnd, msg, wp, lp);
    }
public:
    MongolPrefsPanelInstance(HWND p_parent, preferences_page_callback::ptr p_callback) : m_hwnd(nullptr), m_callback(p_callback), m_hFontBold(nullptr) {
        // 【新增】初始化临时颜色
        m_tempPanelMongolColor = g_panelMongolColor;
        m_tempPanelChineseColor = g_panelChineseColor;
        m_tempPanelHighlightMongolColor = g_panelHighlightMongolColor;
        m_tempPanelHighlightChineseColor = g_panelHighlightChineseColor;
        m_tempPanelBgColor = g_panelBgColor;

        HINSTANCE hInst = core_api::get_my_instance(); m_hwnd = CreateWindowExW(0, WC_DIALOG, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_VSCROLL, 0, 0, 480, 350, p_parent, nullptr, hInst, nullptr);
        if (m_hwnd) { SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this); m_oldWndProc = (WNDPROC)SetWindowLongPtr(m_hwnd, GWLP_WNDPROC, (LONG_PTR)PrefsWndProc); }
        m_hFontBold = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");
        int labelW = 140, ctrlX = 160, gap = 45, y = 20; HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HWND hTitleBase = CreateWindowW(L"STATIC", L"基本设置", WS_CHILD | WS_VISIBLE, 20, y, 100, 25, m_hwnd, nullptr, hInst, nullptr); SendMessage(hTitleBase, WM_SETFONT, (WPARAM)m_hFontBold, TRUE); CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ, 120, y + 12, 320, 2, m_hwnd, nullptr, hInst, nullptr);
        y += 45; CreateWindowW(L"STATIC", L"\u9762\u677F\u6B4C\u8BCD\u5B57\u4F53\u5927\u5C0F:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr); m_hSliderFontSize = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS, ctrlX, y - 5, 150, 30, m_hwnd, (HMENU)3005, hInst, nullptr); m_hLabelFontSize = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, ctrlX + 160, y, 50, 20, m_hwnd, nullptr, hInst, nullptr); SendMessage(m_hSliderFontSize, TBM_SETRANGE, TRUE, MAKELONG(10, 40)); SendMessage(m_hSliderFontSize, TBM_SETPOS, TRUE, g_panelFontSize); wchar_t buf[32]; swprintf_s(buf, L"%d", g_panelFontSize); SetWindowTextW(m_hLabelFontSize, buf);
        y += gap; CreateWindowW(L"STATIC", L"\u53CC\u8BED\u6B4C\u8BCD\u663E\u793A:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr); m_hComboDisplayMode = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, ctrlX, y - 3, 200, 200, m_hwnd, (HMENU)3010, hInst, nullptr); SendMessage(m_hComboDisplayMode, CB_ADDSTRING, 0, (LPARAM)L"显示双语歌词（如果有）"); SendMessage(m_hComboDisplayMode, CB_ADDSTRING, 0, (LPARAM)L"仅显示蒙古文"); SendMessage(m_hComboDisplayMode, CB_ADDSTRING, 0, (LPARAM)L"仅显示中文翻译"); SendMessage(m_hComboDisplayMode, CB_SETCURSEL, g_panelDisplayMode, 0);
        y += gap; CreateWindowW(L"STATIC", L"\u80CC\u666F\u989C\u8272:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr); m_hBtnColorBg = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, ctrlX, y - 3, 80, 25, m_hwnd, (HMENU)3004, hInst, nullptr);
        y += 30; m_hChkShowTip = CreateWindowW(L"BUTTON", L"\u663E\u793A\u72B6\u6001\u63D0\u793A", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, ctrlX, y, 200, 20, m_hwnd, (HMENU)3011, hInst, nullptr); SendMessage(m_hChkShowTip, BM_SETCHECK, g_panelShowTip ? BST_CHECKED : BST_UNCHECKED, 0);

        y += 60; HWND hTitleM = CreateWindowW(L"STATIC", L"蒙古文设置", WS_CHILD | WS_VISIBLE, 20, y, 100, 25, m_hwnd, nullptr, hInst, nullptr); SendMessage(hTitleM, WM_SETFONT, (WPARAM)m_hFontBold, TRUE); CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ, 120, y + 12, 320, 2, m_hwnd, nullptr, hInst, nullptr);
        y += 45; CreateWindowW(L"STATIC", L"\u8499\u53E4\u6587\u5B57\u4F53:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr); m_hComboFontM = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, ctrlX, y - 3, 250, 200, m_hwnd, (HMENU)3006, hInst, nullptr); PopulateFontsCombo(m_hComboFontM, g_panelMongolFont);
        y += gap; CreateWindowW(L"STATIC", L"\u8499\u53E4\u6587\u5B57\u4F53\u989C\u8272:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr); m_hBtnColorM = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, ctrlX, y - 3, 80, 25, m_hwnd, (HMENU)3001, hInst, nullptr);
        y += gap; CreateWindowW(L"STATIC", L"\u8499\u53E4\u6587\u9AD8\u4EAE\u989C\u8272:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr); m_hBtnColorHM = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, ctrlX, y - 3, 80, 25, m_hwnd, (HMENU)3008, hInst, nullptr);

        y += 60; HWND hTitleC = CreateWindowW(L"STATIC", L"中文设置", WS_CHILD | WS_VISIBLE, 20, y, 100, 25, m_hwnd, nullptr, hInst, nullptr); SendMessage(hTitleC, WM_SETFONT, (WPARAM)m_hFontBold, TRUE); CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ, 120, y + 12, 320, 2, m_hwnd, nullptr, hInst, nullptr);
        y += 45; CreateWindowW(L"STATIC", L"\u4E2D\u6587\u5B57\u4F53:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr); m_hComboFontC = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, ctrlX, y - 3, 250, 200, m_hwnd, (HMENU)3007, hInst, nullptr); PopulateFontsCombo(m_hComboFontC, g_panelChineseFont);
        y += gap; CreateWindowW(L"STATIC", L"\u4E2D\u6587\u5B57\u4F53\u989C\u8272:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr); m_hBtnColorC = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, ctrlX, y - 3, 80, 25, m_hwnd, (HMENU)3002, hInst, nullptr);
        y += gap; CreateWindowW(L"STATIC", L"\u4E2D\u6587\u9AD8\u4EAE\u989C\u8272:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr); m_hBtnColorHC = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, ctrlX, y - 3, 80, 25, m_hwnd, (HMENU)3009, hInst, nullptr);

        y += 50; RECT rc; GetClientRect(m_hwnd, &rc); SCROLLINFO si = { sizeof(SCROLLINFO) }; si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS; si.nMin = 0; si.nMax = y; si.nPage = rc.bottom; si.nPos = 0; SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);

        HWND child = GetWindow(m_hwnd, GW_CHILD);
        while (child) { SendMessage(child, WM_SETFONT, (WPARAM)hFont, TRUE); child = GetWindow(child, GW_HWNDNEXT); }
        SendMessage(hTitleBase, WM_SETFONT, (WPARAM)m_hFontBold, TRUE); SendMessage(hTitleM, WM_SETFONT, (WPARAM)m_hFontBold, TRUE); SendMessage(hTitleC, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);
    }

    ~MongolPrefsPanelInstance() { if (m_hwnd) { SetWindowLongPtr(m_hwnd, GWLP_WNDPROC, (LONG_PTR)m_oldWndProc); DestroyWindow(m_hwnd); } if (m_hFontBold) DeleteObject(m_hFontBold); }
    virtual HWND get_wnd() override { return m_hwnd; }
    virtual t_uint32 get_state() override { return preferences_state::resettable | (m_dirty ? preferences_state::changed : 0); }

    virtual void apply() override {
        g_panelFontSize = (int)SendMessage(m_hSliderFontSize, TBM_GETPOS, 0, 0); g_panelDisplayMode = (int)SendMessage(m_hComboDisplayMode, CB_GETCURSEL, 0, 0);
        g_panelShowTip = (SendMessage(m_hChkShowTip, BM_GETCHECK, 0, 0) == BST_CHECKED);
        int idxM = (int)SendMessage(m_hComboFontM, CB_GETCURSEL, 0, 0); if (idxM != CB_ERR) { wchar_t buf[256]; SendMessage(m_hComboFontM, CB_GETLBTEXT, idxM, (LPARAM)buf); g_panelMongolFont = buf; }
        int idxC = (int)SendMessage(m_hComboFontC, CB_GETCURSEL, 0, 0); if (idxC != CB_ERR) { wchar_t buf[256]; SendMessage(m_hComboFontC, CB_GETLBTEXT, idxC, (LPARAM)buf); g_panelChineseFont = buf; }

        // 【修改】点击应用时，生效颜色
        g_panelMongolColor = m_tempPanelMongolColor;
        g_panelChineseColor = m_tempPanelChineseColor;
        g_panelHighlightMongolColor = m_tempPanelHighlightMongolColor;
        g_panelHighlightChineseColor = m_tempPanelHighlightChineseColor;
        g_panelBgColor = m_tempPanelBgColor;

        HWND mainWnd = core_api::get_main_window(); if (mainWnd) PostMessage(mainWnd, WM_APP_SETTINGS_CHANGED, 0, 0); SavePanelSettings();
        m_dirty = false; if (m_callback.is_valid()) m_callback->on_state_changed();
    }

    virtual void reset() override {
        SendMessage(m_hSliderFontSize, TBM_SETPOS, TRUE, 15); SetWindowTextW(m_hLabelFontSize, L"15"); SendMessage(m_hComboDisplayMode, CB_SETCURSEL, 0, 0); g_panelDisplayMode = 0;
        g_panelShowTip = true; SendMessage(m_hChkShowTip, BM_SETCHECK, BST_CHECKED, 0);

        g_panelMongolFont = L"Mongolian Baiti"; g_panelChineseFont = L"Microsoft YaHei";
        int idxM = (int)SendMessage(m_hComboFontM, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)L"Mongolian Baiti");
        SendMessage(m_hComboFontM, CB_SETCURSEL, (idxM != CB_ERR) ? idxM : 0, 0);

        int idxC = (int)SendMessage(m_hComboFontC, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)L"Microsoft YaHei");
        if (idxC == CB_ERR) idxC = (int)SendMessage(m_hComboFontC, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)L"微软雅黑");
        if (idxC == CB_ERR) idxC = (int)SendMessage(m_hComboFontC, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)L"Microsoft YaHei UI");
        SendMessage(m_hComboFontC, CB_SETCURSEL, (idxC != CB_ERR) ? idxC : 0, 0);

        // 【修改】只重置临时颜色和UI
        m_tempPanelMongolColor = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
        m_tempPanelChineseColor = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
        m_tempPanelHighlightMongolColor = D2D1::ColorF(0.0f, 0.8f, 0.0f, 1.0f);
        m_tempPanelHighlightChineseColor = D2D1::ColorF(0.0f, 0.8f, 0.0f, 1.0f);
        m_tempPanelBgColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);

        InvalidateRect(m_hBtnColorM, NULL, TRUE); InvalidateRect(m_hBtnColorC, NULL, TRUE); InvalidateRect(m_hBtnColorHM, NULL, TRUE); InvalidateRect(m_hBtnColorHC, NULL, TRUE); InvalidateRect(m_hBtnColorBg, NULL, TRUE);
        SavePanelSettings(); m_dirty = true; if (m_callback.is_valid()) m_callback->on_state_changed();
    }
};

class MongolPrefsPanelPage : public preferences_page_v3 {
public:
    virtual preferences_page_instance::ptr instantiate(HWND p_parent, preferences_page_callback::ptr p_callback) override { return new service_impl_t<MongolPrefsPanelInstance>(p_parent, p_callback); }
    const char* get_name() override { return "\xE9\x9D\xA2\xE6\x9D\xBF\xE6\xAD\x8C\xE8\xAF\x8D\xE8\xAE\xBE\xE7\xBD\xAE"; }
    GUID get_guid() override { return guid_pref_panel; }
    GUID get_parent_guid() override { return guid_preferences_page; }
};
static preferences_page_factory_t<MongolPrefsPanelPage> g_panel_page_factory;