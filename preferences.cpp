#include "stdafx.h"
#include <windows.h>
#include <commctrl.h> 
#include <string>
#include <vector>
#include <d2d1.h>          
#include <d2d1helper.h>    
#include <SDK/preferences_page.h>
#include <SDK/core_api.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

// 全局变量 extern 声明
extern int g_fontSize;
extern std::wstring g_mongolFont;
extern std::wstring g_chineseFont;
extern D2D1_COLOR_F g_mongolColor;
extern D2D1_COLOR_F g_chineseColor;
extern D2D1_COLOR_F g_mongolOutlineColor;
extern D2D1_COLOR_F g_chineseOutlineColor;
extern float g_mongolOutlineSize;
extern float g_chineseOutlineSize;
extern int g_displayMode;

extern bool g_isVisible;
extern bool g_isLocked;
extern bool g_isTopmost;

void ApplySettingsAndRefresh();

// ================= 定义 GUID 层级关系 =================
extern const GUID guid_preferences_page = { 0x9C3D2A1B, 0x8B4E, 0x4C2D,{ 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x1A, 0x2B } };
const GUID guid_pref_mongol = { 0x9C3D2A1B, 0x8B4E, 0x4C2D,{ 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x1A, 0x2C } };
const GUID guid_pref_chinese = { 0x9C3D2A1B, 0x8B4E, 0x4C2D,{ 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x1A, 0x2D } };

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
    int idx = (int)SendMessage(hCombo, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)defaultFont.c_str());
    if (idx != CB_ERR) SendMessage(hCombo, CB_SETCURSEL, idx, 0);
    else SendMessage(hCombo, CB_SETCURSEL, 0, 0);
}

// =========================================================================
// 1. 通用父页面：字体大小 + 双语显示 + 窗口控制
// =========================================================================
class MongolPrefsRootInstance : public preferences_page_instance {
private:
    HWND m_hwnd;
    preferences_page_callback::ptr m_callback;
    HWND m_hSliderFontSize, m_hLabelFontSize;
    HWND m_hComboDisplayMode;
    HWND m_hChkVisible, m_hChkLock, m_hChkTopmost;
    bool m_dirty = false;
    WNDPROC m_oldWndProc = nullptr;

    static LRESULT CALLBACK PrefsWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        MongolPrefsRootInstance* pThis = (MongolPrefsRootInstance*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (pThis) {
            if (msg == WM_HSCROLL) {
                HWND hCtrl = (HWND)lp;
                if (hCtrl == pThis->m_hSliderFontSize) {
                    wchar_t buf[32]; swprintf_s(buf, L"%d", (int)SendMessage(hCtrl, TBM_GETPOS, 0, 0)); SetWindowTextW(pThis->m_hLabelFontSize, buf);
                }
                pThis->m_dirty = true;
                if (pThis->m_callback.is_valid()) pThis->m_callback->on_state_changed();
                return 0;
            }
            else if (msg == WM_COMMAND) {
                int id = LOWORD(wp); int code = HIWORD(wp);
                if (id == 1003 && code == CBN_SELCHANGE) {
                    pThis->m_dirty = true;
                    if (pThis->m_callback.is_valid()) pThis->m_callback->on_state_changed();
                }
                else if ((id == 1004 || id == 1005 || id == 1006) && code == BN_CLICKED) {
                    pThis->m_dirty = true;
                    if (pThis->m_callback.is_valid()) pThis->m_callback->on_state_changed();
                }
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

        int labelW = 130, ctrlX = 150, y = 30, gap = 45;

        CreateWindowW(L"STATIC", L"\u5168\u5C40\u5B57\u4F53\u5927\u5C0F:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr);
        m_hSliderFontSize = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS, ctrlX, y - 5, 200, 30, m_hwnd, (HMENU)1001, hInst, nullptr);
        m_hLabelFontSize = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, ctrlX + 210, y, 50, 20, m_hwnd, nullptr, hInst, nullptr);
        SendMessage(m_hSliderFontSize, TBM_SETRANGE, TRUE, MAKELONG(12, 100));
        SendMessage(m_hSliderFontSize, TBM_SETPOS, TRUE, g_fontSize);

        y += gap;
        CreateWindowW(L"STATIC", L"\u53CC\u8BED\u6B4C\u8BCD\u663E\u793A:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr);
        m_hComboDisplayMode = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, ctrlX, y - 3, 200, 200, m_hwnd, (HMENU)1003, hInst, nullptr);

        SendMessage(m_hComboDisplayMode, CB_ADDSTRING, 0, (LPARAM)L"\u663E\u793A\u53CC\u8BED\u6B4C\u8BCD\uFF08\u5982\u679C\u6709\uFF09");
        SendMessage(m_hComboDisplayMode, CB_ADDSTRING, 0, (LPARAM)L"\u4EC5\u663E\u793A\u8499\u53E4\u6587");
        SendMessage(m_hComboDisplayMode, CB_ADDSTRING, 0, (LPARAM)L"\u4EC5\u663E\u793A\u4E2D\u6587\u7FFB\u8BD1");

        SendMessage(m_hComboDisplayMode, CB_SETCURSEL, g_displayMode, 0);

        y += gap;
        // 【修改】窗口控制 -> 歌词窗口控制
        CreateWindowW(L"STATIC", L"\u6B4C\u8BCD\u7A97\u53E3\u63A7\u5236:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr);
        // 【修改】显示桌面歌词 -> 启用桌面歌词
        m_hChkVisible = CreateWindowW(L"BUTTON", L"\u542F\u7528\u684C\u9762\u6B4C\u8BCD", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, ctrlX, y, 200, 20, m_hwnd, (HMENU)1004, hInst, nullptr);
        SendMessage(m_hChkVisible, BM_SETCHECK, g_isVisible ? BST_CHECKED : BST_UNCHECKED, 0);

        y += 30;
        m_hChkLock = CreateWindowW(L"BUTTON", L"\u9501\u5B9A\u684C\u9762\u6B4C\u8BCD", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, ctrlX, y, 200, 20, m_hwnd, (HMENU)1005, hInst, nullptr);
        SendMessage(m_hChkLock, BM_SETCHECK, g_isLocked ? BST_CHECKED : BST_UNCHECKED, 0);

        y += 30;
        m_hChkTopmost = CreateWindowW(L"BUTTON", L"\u7F6E\u9876\u6B4C\u8BCD\u7A97\u53E3", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, ctrlX, y, 200, 20, m_hwnd, (HMENU)1006, hInst, nullptr);
        SendMessage(m_hChkTopmost, BM_SETCHECK, g_isTopmost ? BST_CHECKED : BST_UNCHECKED, 0);

        wchar_t buf[32];
        swprintf_s(buf, L"%d", g_fontSize); SetWindowTextW(m_hLabelFontSize, buf);

        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HWND child = GetWindow(m_hwnd, GW_CHILD);
        while (child) { SendMessage(child, WM_SETFONT, (WPARAM)hFont, TRUE); child = GetWindow(child, GW_HWNDNEXT); }
    }

    ~MongolPrefsRootInstance() { if (m_hwnd) { SetWindowLongPtr(m_hwnd, GWLP_WNDPROC, (LONG_PTR)m_oldWndProc); DestroyWindow(m_hwnd); } }
    virtual HWND get_wnd() override { return m_hwnd; }
    virtual t_uint32 get_state() override { return preferences_state::resettable | (m_dirty ? preferences_state::changed : 0); }

    virtual void apply() override {
        g_fontSize = (int)SendMessage(m_hSliderFontSize, TBM_GETPOS, 0, 0);
        g_displayMode = (int)SendMessage(m_hComboDisplayMode, CB_GETCURSEL, 0, 0);

        g_isVisible = (SendMessage(m_hChkVisible, BM_GETCHECK, 0, 0) == BST_CHECKED);
        g_isLocked = (SendMessage(m_hChkLock, BM_GETCHECK, 0, 0) == BST_CHECKED);
        g_isTopmost = (SendMessage(m_hChkTopmost, BM_GETCHECK, 0, 0) == BST_CHECKED);

        ApplySettingsAndRefresh();
        m_dirty = false;
        if (m_callback.is_valid()) m_callback->on_state_changed();
    }

    virtual void reset() override {
        SendMessage(m_hSliderFontSize, TBM_SETPOS, TRUE, 40);
        SendMessage(m_hComboDisplayMode, CB_SETCURSEL, 0, 0);
        SendMessage(m_hChkVisible, BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessage(m_hChkLock, BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessage(m_hChkTopmost, BM_SETCHECK, BST_CHECKED, 0);
        SetWindowTextW(m_hLabelFontSize, L"40");
        m_dirty = true;
        if (m_callback.is_valid()) m_callback->on_state_changed();
    }
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
// 2. 蒙古文设置子页面
// =========================================================================
class MongolPrefsMongolInstance : public preferences_page_instance {
private:
    HWND m_hwnd;
    preferences_page_callback::ptr m_callback;
    HWND m_hComboFont, m_hBtnColor, m_hBtnOutlineColor, m_hSliderOutline, m_hLabelOutline;
    bool m_dirty = false;
    WNDPROC m_oldWndProc = nullptr;

    static LRESULT CALLBACK PrefsWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        MongolPrefsMongolInstance* pThis = (MongolPrefsMongolInstance*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (pThis) {
            if (msg == WM_DRAWITEM) {
                LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lp;
                if (pDIS->CtlID == 2001 || pDIS->CtlID == 2003) {
                    D2D1_COLOR_F color = (pDIS->CtlID == 2001) ? g_mongolColor : g_mongolOutlineColor;
                    COLORREF col = RGB((BYTE)(color.r * 255), (BYTE)(color.g * 255), (BYTE)(color.b * 255));
                    HBRUSH hBrush = CreateSolidBrush(col);
                    FillRect(pDIS->hDC, &pDIS->rcItem, hBrush);
                    DeleteObject(hBrush);
                    FrameRect(pDIS->hDC, &pDIS->rcItem, (HBRUSH)GetStockObject(BLACK_BRUSH));
                    if (pDIS->itemState & ODS_SELECTED) { RECT rc = pDIS->rcItem; InflateRect(&rc, -1, -1); DrawFocusRect(pDIS->hDC, &rc); }
                    return TRUE;
                }
            }
            else if (msg == WM_HSCROLL) {
                HWND hCtrl = (HWND)lp;
                if (hCtrl == pThis->m_hSliderOutline) {
                    wchar_t buf[32]; swprintf_s(buf, L"%.1f", SendMessage(hCtrl, TBM_GETPOS, 0, 0) / 10.0f); SetWindowTextW(pThis->m_hLabelOutline, buf);
                    pThis->m_dirty = true; if (pThis->m_callback.is_valid()) pThis->m_callback->on_state_changed();
                }
                return 0;
            }
            else if (msg == WM_COMMAND) {
                int id = LOWORD(wp); int code = HIWORD(wp);
                if (id == 1004 && code == CBN_SELCHANGE) {
                    pThis->m_dirty = true; if (pThis->m_callback.is_valid()) pThis->m_callback->on_state_changed();
                }
                else if ((id == 2001 || id == 2003) && code == BN_CLICKED) {
                    CHOOSECOLORW cc = { 0 }; static COLORREF customColors[16] = { 0 };
                    cc.lStructSize = sizeof(cc); cc.hwndOwner = hwnd; cc.lpCustColors = customColors; cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                    D2D1_COLOR_F* targetColor = (id == 2001) ? &g_mongolColor : &g_mongolOutlineColor;
                    cc.rgbResult = RGB((BYTE)(targetColor->r * 255), (BYTE)(targetColor->g * 255), (BYTE)(targetColor->b * 255));
                    if (ChooseColorW(&cc)) {
                        *targetColor = D2D1::ColorF(GetRValue(cc.rgbResult) / 255.0f, GetGValue(cc.rgbResult) / 255.0f, GetBValue(cc.rgbResult) / 255.0f);
                        InvalidateRect((HWND)lp, NULL, TRUE);
                        pThis->m_dirty = true; if (pThis->m_callback.is_valid()) pThis->m_callback->on_state_changed();
                    }
                }
            }
        }
        if (pThis && pThis->m_oldWndProc) return CallWindowProc(pThis->m_oldWndProc, hwnd, msg, wp, lp);
        return DefWindowProc(hwnd, msg, wp, lp);
    }

public:
    MongolPrefsMongolInstance(HWND p_parent, preferences_page_callback::ptr p_callback) : m_hwnd(nullptr), m_callback(p_callback) {
        HINSTANCE hInst = core_api::get_my_instance();
        m_hwnd = CreateWindowExW(0, WC_DIALOG, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, 480, 350, p_parent, nullptr, hInst, nullptr);
        if (m_hwnd) {
            SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);
            m_oldWndProc = (WNDPROC)SetWindowLongPtr(m_hwnd, GWLP_WNDPROC, (LONG_PTR)PrefsWndProc);
        }

        int labelW = 140, ctrlX = 160, gap = 45, y = 20;
        CreateWindowW(L"STATIC", L"\u8499\u53E4\u6587\u5B57\u4F53:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr);
        m_hComboFont = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, ctrlX, y - 3, 250, 200, m_hwnd, (HMENU)1004, hInst, nullptr);
        PopulateFontsCombo(m_hComboFont, g_mongolFont);

        y += gap;
        CreateWindowW(L"STATIC", L"\u8499\u53E4\u6587\u5B57\u4F53\u989C\u8272:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr);
        m_hBtnColor = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, ctrlX, y - 3, 80, 25, m_hwnd, (HMENU)2001, hInst, nullptr);

        y += gap;
        CreateWindowW(L"STATIC", L"\u8499\u53E4\u6587\u8FB9\u6846\u989C\u8272:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr);
        m_hBtnOutlineColor = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, ctrlX, y - 3, 80, 25, m_hwnd, (HMENU)2003, hInst, nullptr);

        y += gap;
        CreateWindowW(L"STATIC", L"\u8499\u53E4\u6587\u8FB9\u6846\u7C97\u7EC6:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr);
        m_hSliderOutline = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS, ctrlX, y - 5, 150, 30, m_hwnd, (HMENU)1006, hInst, nullptr);
        m_hLabelOutline = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, ctrlX + 160, y, 50, 20, m_hwnd, nullptr, hInst, nullptr);
        SendMessage(m_hSliderOutline, TBM_SETRANGE, TRUE, MAKELONG(0, 30));
        SendMessage(m_hSliderOutline, TBM_SETPOS, TRUE, (int)(g_mongolOutlineSize * 10));

        wchar_t buf[32]; swprintf_s(buf, L"%.1f", g_mongolOutlineSize); SetWindowTextW(m_hLabelOutline, buf);

        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HWND child = GetWindow(m_hwnd, GW_CHILD);
        while (child) { SendMessage(child, WM_SETFONT, (WPARAM)hFont, TRUE); child = GetWindow(child, GW_HWNDNEXT); }
    }

    ~MongolPrefsMongolInstance() { if (m_hwnd) { SetWindowLongPtr(m_hwnd, GWLP_WNDPROC, (LONG_PTR)m_oldWndProc); DestroyWindow(m_hwnd); } }
    virtual HWND get_wnd() override { return m_hwnd; }
    virtual t_uint32 get_state() override { return preferences_state::resettable | (m_dirty ? preferences_state::changed : 0); }
    virtual void apply() override {
        int idx = (int)SendMessage(m_hComboFont, CB_GETCURSEL, 0, 0);
        if (idx != CB_ERR) { wchar_t buf[256]; SendMessage(m_hComboFont, CB_GETLBTEXT, idx, (LPARAM)buf); g_mongolFont = buf; }
        g_mongolOutlineSize = (int)SendMessage(m_hSliderOutline, TBM_GETPOS, 0, 0) / 10.0f;
        ApplySettingsAndRefresh();
        m_dirty = false;
        if (m_callback.is_valid()) m_callback->on_state_changed();
    }
    virtual void reset() override {
        SendMessage(m_hSliderOutline, TBM_SETPOS, TRUE, 7);
        SetWindowTextW(m_hLabelOutline, L"0.7");
        g_mongolColor = D2D1::ColorF(0.0f, 1.0f, 0.0f, 1.0f);
        g_mongolOutlineColor = D2D1::ColorF(0, 0, 0, 1);
        InvalidateRect(m_hBtnColor, NULL, TRUE); InvalidateRect(m_hBtnOutlineColor, NULL, TRUE);
        m_dirty = true;
        if (m_callback.is_valid()) m_callback->on_state_changed();
    }
};

class MongolPrefsMongolPage : public preferences_page_v3 {
public:
    virtual preferences_page_instance::ptr instantiate(HWND p_parent, preferences_page_callback::ptr p_callback) override { return new service_impl_t<MongolPrefsMongolInstance>(p_parent, p_callback); }
    const char* get_name() override { return "\xE8\x92\x99\xE5\x8F\xA4\xE6\x96\x87\xE8\xAE\xBE\xE7\xBD\xAE"; }
    GUID get_guid() override { return guid_pref_mongol; }
    GUID get_parent_guid() override { return guid_preferences_page; }
};
static preferences_page_factory_t<MongolPrefsMongolPage> g_mongol_page_factory;

// =========================================================================
// 3. 中文设置子页面
// =========================================================================
class MongolPrefsChineseInstance : public preferences_page_instance {
private:
    HWND m_hwnd;
    preferences_page_callback::ptr m_callback;
    HWND m_hComboFont, m_hBtnColor, m_hBtnOutlineColor, m_hSliderOutline, m_hLabelOutline;
    bool m_dirty = false;
    WNDPROC m_oldWndProc = nullptr;

    static LRESULT CALLBACK PrefsWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        MongolPrefsChineseInstance* pThis = (MongolPrefsChineseInstance*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (pThis) {
            if (msg == WM_DRAWITEM) {
                LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lp;
                if (pDIS->CtlID == 2002 || pDIS->CtlID == 2004) {
                    D2D1_COLOR_F color = (pDIS->CtlID == 2002) ? g_chineseColor : g_chineseOutlineColor;
                    COLORREF col = RGB((BYTE)(color.r * 255), (BYTE)(color.g * 255), (BYTE)(color.b * 255));
                    HBRUSH hBrush = CreateSolidBrush(col);
                    FillRect(pDIS->hDC, &pDIS->rcItem, hBrush);
                    DeleteObject(hBrush);
                    FrameRect(pDIS->hDC, &pDIS->rcItem, (HBRUSH)GetStockObject(BLACK_BRUSH));
                    if (pDIS->itemState & ODS_SELECTED) { RECT rc = pDIS->rcItem; InflateRect(&rc, -1, -1); DrawFocusRect(pDIS->hDC, &rc); }
                    return TRUE;
                }
            }
            else if (msg == WM_HSCROLL) {
                HWND hCtrl = (HWND)lp;
                if (hCtrl == pThis->m_hSliderOutline) {
                    wchar_t buf[32]; swprintf_s(buf, L"%.1f", SendMessage(hCtrl, TBM_GETPOS, 0, 0) / 10.0f); SetWindowTextW(pThis->m_hLabelOutline, buf);
                    pThis->m_dirty = true; if (pThis->m_callback.is_valid()) pThis->m_callback->on_state_changed();
                }
                return 0;
            }
            else if (msg == WM_COMMAND) {
                int id = LOWORD(wp); int code = HIWORD(wp);
                if (id == 1005 && code == CBN_SELCHANGE) {
                    pThis->m_dirty = true; if (pThis->m_callback.is_valid()) pThis->m_callback->on_state_changed();
                }
                else if ((id == 2002 || id == 2004) && code == BN_CLICKED) {
                    CHOOSECOLORW cc = { 0 }; static COLORREF customColors[16] = { 0 };
                    cc.lStructSize = sizeof(cc); cc.hwndOwner = hwnd; cc.lpCustColors = customColors; cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                    D2D1_COLOR_F* targetColor = (id == 2002) ? &g_chineseColor : &g_chineseOutlineColor;
                    cc.rgbResult = RGB((BYTE)(targetColor->r * 255), (BYTE)(targetColor->g * 255), (BYTE)(targetColor->b * 255));
                    if (ChooseColorW(&cc)) {
                        *targetColor = D2D1::ColorF(GetRValue(cc.rgbResult) / 255.0f, GetGValue(cc.rgbResult) / 255.0f, GetBValue(cc.rgbResult) / 255.0f);
                        InvalidateRect((HWND)lp, NULL, TRUE);
                        pThis->m_dirty = true; if (pThis->m_callback.is_valid()) pThis->m_callback->on_state_changed();
                    }
                }
            }
        }
        if (pThis && pThis->m_oldWndProc) return CallWindowProc(pThis->m_oldWndProc, hwnd, msg, wp, lp);
        return DefWindowProc(hwnd, msg, wp, lp);
    }

public:
    MongolPrefsChineseInstance(HWND p_parent, preferences_page_callback::ptr p_callback) : m_hwnd(nullptr), m_callback(p_callback) {
        HINSTANCE hInst = core_api::get_my_instance();
        m_hwnd = CreateWindowExW(0, WC_DIALOG, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, 480, 350, p_parent, nullptr, hInst, nullptr);
        if (m_hwnd) {
            SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);
            m_oldWndProc = (WNDPROC)SetWindowLongPtr(m_hwnd, GWLP_WNDPROC, (LONG_PTR)PrefsWndProc);
        }

        int labelW = 140, ctrlX = 160, gap = 45, y = 20;
        CreateWindowW(L"STATIC", L"\u4E2D\u6587\u5B57\u4F53:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr);
        m_hComboFont = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, ctrlX, y - 3, 250, 200, m_hwnd, (HMENU)1005, hInst, nullptr);
        PopulateFontsCombo(m_hComboFont, g_chineseFont);

        y += gap;
        CreateWindowW(L"STATIC", L"\u4E2D\u6587\u5B57\u4F53\u989C\u8272:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr);
        m_hBtnColor = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, ctrlX, y - 3, 80, 25, m_hwnd, (HMENU)2002, hInst, nullptr);

        y += gap;
        CreateWindowW(L"STATIC", L"\u4E2D\u6587\u8FB9\u6846\u989C\u8272:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr);
        m_hBtnOutlineColor = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, ctrlX, y - 3, 80, 25, m_hwnd, (HMENU)2004, hInst, nullptr);

        y += gap;
        CreateWindowW(L"STATIC", L"\u4E2D\u6587\u8FB9\u6846\u7C97\u7EC6:", WS_CHILD | WS_VISIBLE, 20, y, labelW, 20, m_hwnd, nullptr, hInst, nullptr);
        m_hSliderOutline = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS, ctrlX, y - 5, 150, 30, m_hwnd, (HMENU)1007, hInst, nullptr);
        m_hLabelOutline = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, ctrlX + 160, y, 50, 20, m_hwnd, nullptr, hInst, nullptr);
        SendMessage(m_hSliderOutline, TBM_SETRANGE, TRUE, MAKELONG(0, 30));
        SendMessage(m_hSliderOutline, TBM_SETPOS, TRUE, (int)(g_chineseOutlineSize * 10));

        wchar_t buf[32]; swprintf_s(buf, L"%.1f", g_chineseOutlineSize); SetWindowTextW(m_hLabelOutline, buf);

        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HWND child = GetWindow(m_hwnd, GW_CHILD);
        while (child) { SendMessage(child, WM_SETFONT, (WPARAM)hFont, TRUE); child = GetWindow(child, GW_HWNDNEXT); }
    }

    ~MongolPrefsChineseInstance() { if (m_hwnd) { SetWindowLongPtr(m_hwnd, GWLP_WNDPROC, (LONG_PTR)m_oldWndProc); DestroyWindow(m_hwnd); } }
    virtual HWND get_wnd() override { return m_hwnd; }
    virtual t_uint32 get_state() override { return preferences_state::resettable | (m_dirty ? preferences_state::changed : 0); }
    virtual void apply() override {
        int idx = (int)SendMessage(m_hComboFont, CB_GETCURSEL, 0, 0);
        if (idx != CB_ERR) { wchar_t buf[256]; SendMessage(m_hComboFont, CB_GETLBTEXT, idx, (LPARAM)buf); g_chineseFont = buf; }
        g_chineseOutlineSize = (int)SendMessage(m_hSliderOutline, TBM_GETPOS, 0, 0) / 10.0f;
        ApplySettingsAndRefresh();
        m_dirty = false;
        if (m_callback.is_valid()) m_callback->on_state_changed();
    }
    virtual void reset() override {
        SendMessage(m_hSliderOutline, TBM_SETPOS, TRUE, 7);
        SetWindowTextW(m_hLabelOutline, L"0.7");
        g_chineseColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        g_chineseOutlineColor = D2D1::ColorF(0, 0, 0, 1);
        InvalidateRect(m_hBtnColor, NULL, TRUE); InvalidateRect(m_hBtnOutlineColor, NULL, TRUE);
        m_dirty = true;
        if (m_callback.is_valid()) m_callback->on_state_changed();
    }
};

class MongolPrefsChinesePage : public preferences_page_v3 {
public:
    virtual preferences_page_instance::ptr instantiate(HWND p_parent, preferences_page_callback::ptr p_callback) override { return new service_impl_t<MongolPrefsChineseInstance>(p_parent, p_callback); }
    const char* get_name() override { return "\xE4\xB8\xAD\xE6\x96\x87\xE8\xAE\xBE\xE7\xBD\xAE"; }
    GUID get_guid() override { return guid_pref_chinese; }
    GUID get_parent_guid() override { return guid_preferences_page; }
};
static preferences_page_factory_t<MongolPrefsChinesePage> g_chinese_page_factory;