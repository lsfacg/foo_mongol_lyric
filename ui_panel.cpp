#include "stdafx.h"
#include <windows.h>
#include <SDK/ui_element.h>
#include <helpers/atl-misc.h> 
#include <helpers/foobar2000+atl.h>
#include <SDK/core_api.h>
#include <SDK/playback_control.h>
#include <SDK/metadb.h>
#include <SDK/ui.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <vector>
#include <string>
#include <cmath>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

static const GUID guid_mongol_lyric_panel = { 0x12345678, 0x1234, 0x1234,{ 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0 } };

extern const wchar_t* REG_PATH;
extern const GUID guid_pref_panel;

extern bool g_isVisible;
extern bool g_isLocked;
extern bool g_isTopmost;
extern HWND g_hwndLyrics;
extern int g_reload_flag;

int g_panelFontSize = 15;
std::wstring g_panelMongolFont = L"Mongolian Baiti";
std::wstring g_panelChineseFont = L"Microsoft YaHei";
D2D1_COLOR_F g_panelMongolColor = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
D2D1_COLOR_F g_panelChineseColor = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
D2D1_COLOR_F g_panelHighlightMongolColor = D2D1::ColorF(0.0f, 0.8f, 0.0f, 1.0f);
D2D1_COLOR_F g_panelHighlightChineseColor = D2D1::ColorF(0.0f, 0.8f, 0.0f, 1.0f);
D2D1_COLOR_F g_panelBgColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
int g_panelDisplayMode = 0;

void LoadPanelSettings() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_PATH, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type, size;
        int fs = 15, dm = 0;
        size = sizeof(int);
        if (RegQueryValueExW(hKey, L"PanelFontSize", NULL, &type, (LPBYTE)&fs, &size) == ERROR_SUCCESS && type == REG_DWORD) g_panelFontSize = fs;
        size = sizeof(int);
        if (RegQueryValueExW(hKey, L"PanelDisplayMode", NULL, &type, (LPBYTE)&dm, &size) == ERROR_SUCCESS && type == REG_DWORD) g_panelDisplayMode = dm;

        wchar_t fontBuf[256] = { 0 };
        size = sizeof(fontBuf);
        if (RegQueryValueExW(hKey, L"PanelMongolFont", NULL, &type, (LPBYTE)fontBuf, &size) == ERROR_SUCCESS && type == REG_SZ) g_panelMongolFont = fontBuf;
        size = sizeof(fontBuf);
        if (RegQueryValueExW(hKey, L"PanelChineseFont", NULL, &type, (LPBYTE)fontBuf, &size) == ERROR_SUCCESS && type == REG_SZ) g_panelChineseFont = fontBuf;

        DWORD col = 0;
        size = sizeof(DWORD);
        if (RegQueryValueExW(hKey, L"PanelMongolColor", NULL, &type, (LPBYTE)&col, &size) == ERROR_SUCCESS && type == REG_DWORD) g_panelMongolColor = D2D1::ColorF(GetRValue(col) / 255.0f, GetGValue(col) / 255.0f, GetBValue(col) / 255.0f);
        if (RegQueryValueExW(hKey, L"PanelChineseColor", NULL, &type, (LPBYTE)&col, &size) == ERROR_SUCCESS && type == REG_DWORD) g_panelChineseColor = D2D1::ColorF(GetRValue(col) / 255.0f, GetGValue(col) / 255.0f, GetBValue(col) / 255.0f);
        if (RegQueryValueExW(hKey, L"PanelHighlightMongolColor", NULL, &type, (LPBYTE)&col, &size) == ERROR_SUCCESS && type == REG_DWORD) g_panelHighlightMongolColor = D2D1::ColorF(GetRValue(col) / 255.0f, GetGValue(col) / 255.0f, GetBValue(col) / 255.0f);
        if (RegQueryValueExW(hKey, L"PanelHighlightChineseColor", NULL, &type, (LPBYTE)&col, &size) == ERROR_SUCCESS && type == REG_DWORD) g_panelHighlightChineseColor = D2D1::ColorF(GetRValue(col) / 255.0f, GetGValue(col) / 255.0f, GetBValue(col) / 255.0f);
        if (RegQueryValueExW(hKey, L"PanelBgColor", NULL, &type, (LPBYTE)&col, &size) == ERROR_SUCCESS && type == REG_DWORD) g_panelBgColor = D2D1::ColorF(GetRValue(col) / 255.0f, GetGValue(col) / 255.0f, GetBValue(col) / 255.0f);

        RegCloseKey(hKey);
    }
}

void SavePanelSettings() {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_PATH, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        int fs = g_panelFontSize;
        int dm = g_panelDisplayMode;
        RegSetValueExW(hKey, L"PanelFontSize", 0, REG_DWORD, (const BYTE*)&fs, sizeof(int));
        RegSetValueExW(hKey, L"PanelDisplayMode", 0, REG_DWORD, (const BYTE*)&dm, sizeof(int));

        RegSetValueExW(hKey, L"PanelMongolFont", 0, REG_SZ, (const BYTE*)g_panelMongolFont.c_str(), (g_panelMongolFont.length() + 1) * sizeof(wchar_t));
        RegSetValueExW(hKey, L"PanelChineseFont", 0, REG_SZ, (const BYTE*)g_panelChineseFont.c_str(), (g_panelChineseFont.length() + 1) * sizeof(wchar_t));

        DWORD colM = RGB((BYTE)(g_panelMongolColor.r * 255), (BYTE)(g_panelMongolColor.g * 255), (BYTE)(g_panelMongolColor.b * 255));
        DWORD colC = RGB((BYTE)(g_panelChineseColor.r * 255), (BYTE)(g_panelChineseColor.g * 255), (BYTE)(g_panelChineseColor.b * 255));
        DWORD colHM = RGB((BYTE)(g_panelHighlightMongolColor.r * 255), (BYTE)(g_panelHighlightMongolColor.g * 255), (BYTE)(g_panelHighlightMongolColor.b * 255));
        DWORD colHC = RGB((BYTE)(g_panelHighlightChineseColor.r * 255), (BYTE)(g_panelHighlightChineseColor.g * 255), (BYTE)(g_panelHighlightChineseColor.b * 255));
        DWORD colBg = RGB((BYTE)(g_panelBgColor.r * 255), (BYTE)(g_panelBgColor.g * 255), (BYTE)(g_panelBgColor.b * 255));

        RegSetValueExW(hKey, L"PanelMongolColor", 0, REG_DWORD, (const BYTE*)&colM, sizeof(DWORD));
        RegSetValueExW(hKey, L"PanelChineseColor", 0, REG_DWORD, (const BYTE*)&colC, sizeof(DWORD));
        RegSetValueExW(hKey, L"PanelHighlightMongolColor", 0, REG_DWORD, (const BYTE*)&colHM, sizeof(DWORD));
        RegSetValueExW(hKey, L"PanelHighlightChineseColor", 0, REG_DWORD, (const BYTE*)&colHC, sizeof(DWORD));
        RegSetValueExW(hKey, L"PanelBgColor", 0, REG_DWORD, (const BYTE*)&colBg, sizeof(DWORD));

        RegCloseKey(hKey);
    }
}

struct PanelLyricLine {
    double time;
    std::wstring mongolText;
    std::wstring chineseText;
};

class MongolPanelInstance;

class FullScreenLyric : public CWindowImpl<FullScreenLyric> {
private:
    ID2D1Factory* m_pD2DFactory;
    ID2D1HwndRenderTarget* m_pRenderTarget;
    IDWriteFactory* m_pDWriteFactory;
    IDWriteTextFormat* m_pMongolFormat;
    ID2D1SolidColorBrush* m_pMongolBrush;
    ID2D1SolidColorBrush* m_pChineseBrush;
    ID2D1SolidColorBrush* m_pHighlightMongolBrush;
    ID2D1SolidColorBrush* m_pHighlightChineseBrush;
    ID2D1StrokeStyle* m_pDashedStrokeStyle; // 全屏虚线样式
    MongolPanelInstance* m_pParent;
    int m_lastFontSize;
    std::wstring m_lastMFont, m_lastCFont;
    bool m_isMouseDown;
    bool m_isActuallyDragging;
    int m_dragStartX;
    float m_dragStartVirtualIdx;
    float m_currentDragVirtualIdx;
    bool m_isSnapping;
    float m_snapStartIdx;
    float m_snapTargetIdx;
    int m_lastRenderIdx;
    DWORD m_animStartTick;
    int m_lastReloadFlag;

public:
    FullScreenLyric(MongolPanelInstance* parent);
    ~FullScreenLyric();

    DECLARE_WND_CLASS_EX(TEXT("{F1A2B3C4-D5E6-4434-A28C-F16CFCE13C4B}"), CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS, (-1));

    BEGIN_MSG_MAP(FullScreenLyric)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WM_TIMER, OnTimer)
        MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
        MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
        MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)
        MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnLButtonDblClk)
        MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown)
        MESSAGE_HANDLER(WM_MOUSEWHEEL, OnMouseWheel)
        MESSAGE_HANDLER(WM_CONTEXTMENU, OnContextMenu)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(WM_NCDESTROY, OnNcDestroy)
    END_MSG_MAP()

    void InitFullScreen();
    std::vector<PanelLyricLine>& GetLyrics();
    double GetPlaybackTime();

    LRESULT OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnTimer(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnMouseWheel(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnKeyDown(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnNcDestroy(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnLButtonDblClk(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnLButtonDown(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnMouseMove(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnLButtonUp(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnContextMenu(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
};

class MongolPanelInstance : public ui_element_instance, public CWindowImpl<MongolPanelInstance> {
public:
    ID2D1Factory* m_pD2DFactory = nullptr;
    ID2D1HwndRenderTarget* m_pRenderTarget = nullptr;
    IDWriteFactory* m_pDWriteFactory = nullptr;
    IDWriteTextFormat* m_pMongolFormat = nullptr;
    ID2D1SolidColorBrush* m_pMongolBrush = nullptr;
    ID2D1SolidColorBrush* m_pChineseBrush = nullptr;
    ID2D1SolidColorBrush* m_pHighlightMongolBrush = nullptr;
    ID2D1SolidColorBrush* m_pHighlightChineseBrush = nullptr;
    ID2D1StrokeStyle* m_pDashedStrokeStyle = nullptr; // 面板虚线样式

    std::vector<PanelLyricLine> m_lyrics;
    FullScreenLyric* m_pFullScreen = nullptr;

private:
    ui_element_instance_callback::ptr m_callback;
    std::wstring m_currentTrackPath;
    int m_lastFontSize = -1;
    std::wstring m_lastMFont, m_lastCFont;
    int m_lastReloadFlag = 0;
    bool m_isMouseDown = false;
    bool m_isActuallyDragging = false;
    int m_dragStartX = 0;
    float m_dragStartVirtualIdx = 0.0f;
    float m_currentDragVirtualIdx = 0.0f;
    bool m_isSnapping = false;
    float m_snapStartIdx = 0.0f;
    float m_snapTargetIdx = 0.0f;
    int m_lastRenderIdx = -1;
    DWORD m_animStartTick = 0;

public:
    void LoadLyricsForCurrentTrack();
    void ReloadLyrics();
    void OnFullScreenClosed();
    void OpenFullScreen();

    MongolPanelInstance(ui_element_config::ptr cfg, ui_element_instance_callback::ptr cb);
    ~MongolPanelInstance();

    std::vector<PanelLyricLine>& GetLyrics() { return m_lyrics; }

    DECLARE_WND_CLASS_EX(TEXT("{DC2917D5-1288-4434-A28C-F16CFCE13C4B}"), CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS, (-1));

    BEGIN_MSG_MAP(MongolPanelInstance)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_TIMER, OnTimer)
        MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
        MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
        MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)
        MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnLButtonDblClk)
        MESSAGE_HANDLER(WM_MOUSEWHEEL, OnMouseWheel)
        MESSAGE_HANDLER(WM_CONTEXTMENU, OnContextMenu)
    END_MSG_MAP()

    static GUID g_get_guid() { return guid_mongol_lyric_panel; }
    static GUID g_get_subclass() { return ui_element_subclass_utility; }
    static void g_get_name(pfc::string_base& out) { out = "\xE8\x92\x99\xE5\x8F\xA4\xE6\x96\x87\xE6\xAD\x8C\xE8\xAF\x8D"; }
    static const char* g_get_description() { return "\xE6\xB7\xBB\xE5\x8A\xA0\xE8\x92\x99\xE5\x8F\xA4\xE6\x96\x87\xE6\xAD\x8C\xE8\xAF\x8D\xE9\x9D\xA2\xE6\x9D\xBF\xE3\x80\x82"; }
    static ui_element_config::ptr g_get_default_configuration() { return ui_element_config::g_create_empty(guid_mongol_lyric_panel); }

    void initialize_window(HWND parent);
    HWND get_wnd() override { return m_hWnd; }
    void set_configuration(ui_element_config::ptr config) override {}
    ui_element_config::ptr get_configuration() override { return ui_element_config::g_create_empty(guid_mongol_lyric_panel); }
    void notify(const GUID& p_what, t_size p_param1, const void* p_param2, t_size p_param2size) override;

    LRESULT OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnMouseWheel(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnContextMenu(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnTimer(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnLButtonDblClk(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnLButtonDown(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnMouseMove(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnLButtonUp(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
};

FullScreenLyric::FullScreenLyric(MongolPanelInstance* parent) :
    m_pD2DFactory(nullptr), m_pRenderTarget(nullptr), m_pDWriteFactory(nullptr), m_pMongolFormat(nullptr),
    m_pMongolBrush(nullptr), m_pChineseBrush(nullptr), m_pHighlightMongolBrush(nullptr), m_pHighlightChineseBrush(nullptr),
    m_pDashedStrokeStyle(nullptr), m_pParent(parent), m_lastFontSize(-1), m_isMouseDown(false), m_isActuallyDragging(false), m_dragStartX(0),
    m_dragStartVirtualIdx(0.0f), m_currentDragVirtualIdx(0.0f), m_isSnapping(false), m_snapStartIdx(0.0f),
    m_snapTargetIdx(0.0f), m_lastRenderIdx(-1), m_animStartTick(0), m_lastReloadFlag(0) {
}

FullScreenLyric::~FullScreenLyric() {
    if (m_pMongolBrush) m_pMongolBrush->Release();
    if (m_pChineseBrush) m_pChineseBrush->Release();
    if (m_pHighlightMongolBrush) m_pHighlightMongolBrush->Release();
    if (m_pHighlightChineseBrush) m_pHighlightChineseBrush->Release();
    if (m_pMongolFormat) m_pMongolFormat->Release();
    if (m_pDWriteFactory) m_pDWriteFactory->Release();
    if (m_pRenderTarget) m_pRenderTarget->Release();
    if (m_pD2DFactory) m_pD2DFactory->Release();
    if (m_pDashedStrokeStyle) m_pDashedStrokeStyle->Release();
}

void FullScreenLyric::InitFullScreen() {
    m_lastReloadFlag = g_reload_flag;
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&m_pDWriteFactory));
    m_lastFontSize = g_panelFontSize;
    m_lastMFont = g_panelMongolFont;
    m_lastCFont = g_panelChineseFont;
    float fs = (float)g_panelFontSize * 2.5f;
    m_pDWriteFactory->CreateTextFormat(g_panelMongolFont.c_str(), nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fs, L"mn-MN", &m_pMongolFormat);
    m_pMongolFormat->SetReadingDirection(DWRITE_READING_DIRECTION_TOP_TO_BOTTOM);
    m_pMongolFormat->SetFlowDirection(DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT);
    m_pMongolFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    m_pMongolFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    m_pMongolFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP); // 取消自动换行

    D2D1_STROKE_STYLE_PROPERTIES strokeProps = D2D1::StrokeStyleProperties(
        D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT,
        D2D1_LINE_JOIN_MITER, 10.0f, D2D1_DASH_STYLE_DASH, 0.0f);
    m_pD2DFactory->CreateStrokeStyle(strokeProps, nullptr, 0, &m_pDashedStrokeStyle);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    Create(NULL, CWindow::rcDefault, TEXT("FullScreenLyric"), WS_POPUP | WS_VISIBLE, WS_EX_TOPMOST);
    ::SetWindowPos(m_hWnd, HWND_TOPMOST, 0, 0, screenW, screenH, SWP_SHOWWINDOW);
    ::SetForegroundWindow(m_hWnd);
    ::SetFocus(m_hWnd);
    SetTimer(1, 16, NULL);
    double currentTime = GetPlaybackTime();
    int currentIdx = -1;
    for (int i = 0; i < (int)GetLyrics().size(); i++) {
        if (currentTime >= GetLyrics()[i].time) currentIdx = i;
        else break;
    }
    m_lastRenderIdx = currentIdx;
    m_animStartTick = GetTickCount() - 1000;
}

std::vector<PanelLyricLine>& FullScreenLyric::GetLyrics() { return m_pParent->GetLyrics(); }
double FullScreenLyric::GetPlaybackTime() { double t = playback_control::get()->playback_get_position(); return t < 0 ? 0 : t; }
LRESULT FullScreenLyric::OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&) { return 1; }

LRESULT FullScreenLyric::OnTimer(UINT, WPARAM, LPARAM, BOOL&) {
    if (m_lastFontSize != g_panelFontSize || m_lastMFont != g_panelMongolFont || m_lastCFont != g_panelChineseFont) {
        m_lastFontSize = g_panelFontSize; m_lastMFont = g_panelMongolFont; m_lastCFont = g_panelChineseFont;
        if (m_pMongolFormat) m_pMongolFormat->Release();
        float fs = (float)g_panelFontSize * 2.5f;
        m_pDWriteFactory->CreateTextFormat(g_panelMongolFont.c_str(), nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fs, L"mn-MN", &m_pMongolFormat);
        m_pMongolFormat->SetReadingDirection(DWRITE_READING_DIRECTION_TOP_TO_BOTTOM);
        m_pMongolFormat->SetFlowDirection(DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT);
        m_pMongolFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        m_pMongolFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        m_pMongolFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }
    if (m_lastReloadFlag != g_reload_flag) { m_lastReloadFlag = g_reload_flag; m_lastRenderIdx = -1; }
    Invalidate(); return 0;
}

LRESULT FullScreenLyric::OnMouseWheel(UINT, WPARAM wParam, LPARAM, BOOL&) {
    if (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) {
        static DWORD lastWheelTime = 0;
        DWORD currentTime = GetTickCount();
        if (currentTime - lastWheelTime < 50) return 0;
        lastWheelTime = currentTime;

        short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (zDelta > 0) { g_panelFontSize += 1; if (g_panelFontSize > 60) g_panelFontSize = 60; }
        else { g_panelFontSize -= 1; if (g_panelFontSize < 10) g_panelFontSize = 10; }
        SavePanelSettings(); Invalidate(); return 0;
    }
    return 0;
}

LRESULT FullScreenLyric::OnKeyDown(UINT, WPARAM wParam, LPARAM, BOOL&) {
    if (wParam == VK_ESCAPE) { PostMessage(WM_CLOSE); return 0; }
    return 0;
}

LRESULT FullScreenLyric::OnDestroy(UINT, WPARAM, LPARAM, BOOL&) { KillTimer(1); return 0; }

LRESULT FullScreenLyric::OnNcDestroy(UINT, WPARAM, LPARAM, BOOL&) {
    if (m_pParent) { m_pParent->OnFullScreenClosed(); }
    delete this; return 0;
}

LRESULT FullScreenLyric::OnLButtonDblClk(UINT, WPARAM, LPARAM, BOOL&) { PostMessage(WM_CLOSE); return 0; }

LRESULT FullScreenLyric::OnLButtonDown(UINT, WPARAM, LPARAM lParam, BOOL&) {
    if (GetLyrics().empty()) return 0;
    m_isMouseDown = true; m_isActuallyDragging = false; m_isSnapping = false; m_dragStartX = GET_X_LPARAM(lParam);
    double currentTime = GetPlaybackTime();
    int currentIdx = -1;
    for (int i = 0; i < (int)GetLyrics().size(); i++) {
        if (currentTime >= GetLyrics()[i].time) currentIdx = i;
        else break;
    }
    m_dragStartVirtualIdx = (float)currentIdx; m_currentDragVirtualIdx = m_dragStartVirtualIdx; SetCapture(); return 0;
}

LRESULT FullScreenLyric::OnMouseMove(UINT, WPARAM, LPARAM lParam, BOOL&) {
    if (m_isMouseDown) {
        int mouseX = GET_X_LPARAM(lParam); int deltaX = mouseX - m_dragStartX;
        if (std::abs(deltaX) > 5) m_isActuallyDragging = true;
        if (m_isActuallyDragging) {
            float step = (float)g_panelFontSize * 8.0f;
            m_currentDragVirtualIdx = m_dragStartVirtualIdx - (deltaX / step);
            if (m_currentDragVirtualIdx < 0.0f) m_currentDragVirtualIdx = 0.0f;
            if (m_currentDragVirtualIdx > (float)(GetLyrics().size() - 1)) m_currentDragVirtualIdx = (float)(GetLyrics().size() - 1);
            Invalidate();
        }
    }
    return 0;
}

LRESULT FullScreenLyric::OnLButtonUp(UINT, WPARAM, LPARAM, BOOL&) {
    if (m_isMouseDown) {
        m_isMouseDown = false; ReleaseCapture();
        if (m_isActuallyDragging) {
            m_isActuallyDragging = false;
            int targetIdx = (int)round(m_currentDragVirtualIdx);
            if (targetIdx >= 0 && targetIdx < (int)GetLyrics().size()) {
                playback_control::get()->playback_seek(GetLyrics()[targetIdx].time);
                if (std::abs(m_currentDragVirtualIdx - targetIdx) > 0.01f) {
                    m_isSnapping = true; m_snapStartIdx = m_currentDragVirtualIdx; m_snapTargetIdx = (float)targetIdx; m_animStartTick = GetTickCount();
                }
                m_lastRenderIdx = targetIdx;
            }
            Invalidate();
        }
    }
    return 0;
}

LRESULT FullScreenLyric::OnContextMenu(UINT, WPARAM, LPARAM lParam, BOOL&) {
    HMENU hMenu = CreatePopupMenu(); HMENU hSubMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING | (g_isVisible ? MF_CHECKED : MF_UNCHECKED), 1, L"\u542F\u7528\u684C\u9762\u6B4C\u8BCD");
    AppendMenuW(hMenu, MF_STRING | (g_isLocked ? MF_CHECKED : MF_UNCHECKED), 2, L"\u9501\u5B9A\u684C\u9762\u6B4C\u8BCD");
    AppendMenuW(hMenu, MF_STRING | (g_isTopmost ? MF_CHECKED : MF_UNCHECKED), 3, L"\u7F6E\u9876\u684C\u9762\u6B4C\u8BCD");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, 4, L"\u9000\u51FA\u5168\u5C4F\u6B4C\u8BCD");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hSubMenu, MF_STRING | (g_panelDisplayMode == 0 ? MF_CHECKED : MF_UNCHECKED), 10, L"\u663E\u793A\u53CC\u8BED\u6B4C\u8BCD");
    AppendMenuW(hSubMenu, MF_STRING | (g_panelDisplayMode == 1 ? MF_CHECKED : MF_UNCHECKED), 11, L"\u4EC5\u663E\u793A\u8499\u53E4\u6587");
    AppendMenuW(hSubMenu, MF_STRING | (g_panelDisplayMode == 2 ? MF_CHECKED : MF_UNCHECKED), 12, L"\u4EC5\u663E\u793A\u4E2D\u6587\u7FFB\u8BD1");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubMenu, L"\u53CC\u8BED\u6B4C\u8BCD\u663E\u793A");
    AppendMenuW(hMenu, MF_STRING, 5, L"\u91CD\u65B0\u8F7D\u5165\u6B4C\u8BCD");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, 6, L"\u9762\u677F\u6B4C\u8BCD\u8BBE\u7F6E");

    POINT pt; GetCursorPos(&pt);
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, m_hWnd, NULL);
    DestroyMenu(hSubMenu); DestroyMenu(hMenu);

    if (cmd == 1) { g_isVisible = !g_isVisible; if (g_hwndLyrics) ::ShowWindow(g_hwndLyrics, g_isVisible ? SW_SHOW : SW_HIDE); }
    else if (cmd == 2) {
        g_isLocked = !g_isLocked;
        if (g_hwndLyrics) {
            LONG_PTR exStyle = ::GetWindowLongPtr(g_hwndLyrics, GWL_EXSTYLE);
            if (g_isLocked) ::SetWindowLongPtr(g_hwndLyrics, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT);
            else ::SetWindowLongPtr(g_hwndLyrics, GWL_EXSTYLE, exStyle & ~WS_EX_TRANSPARENT);
        }
    }
    else if (cmd == 3) { g_isTopmost = !g_isTopmost; if (g_hwndLyrics) ::SetWindowPos(g_hwndLyrics, g_isTopmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); }
    else if (cmd == 4) { PostMessage(WM_CLOSE); return 0; }
    else if (cmd == 5) { if (m_pParent) m_pParent->ReloadLyrics(); m_lastRenderIdx = -1; }
    else if (cmd == 6) { fb2k::inMainThread([] { static_api_ptr_t<ui_control>()->show_preferences(guid_pref_panel); }); }
    else if (cmd >= 10 && cmd <= 12) { g_panelDisplayMode = cmd - 10; SavePanelSettings(); }
    Invalidate(); return 0;
}

LRESULT FullScreenLyric::OnPaint(UINT, WPARAM, LPARAM, BOOL&) {
    PAINTSTRUCT ps; HDC hdc = BeginPaint(&ps);
    if (!m_pRenderTarget) {
        RECT rc; GetClientRect(&rc); m_pD2DFactory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(), D2D1::HwndRenderTargetProperties(m_hWnd, D2D1::SizeU(rc.right, rc.bottom)), &m_pRenderTarget);
        if (m_pRenderTarget) {
            m_pRenderTarget->CreateSolidColorBrush(g_panelMongolColor, &m_pMongolBrush); m_pRenderTarget->CreateSolidColorBrush(g_panelChineseColor, &m_pChineseBrush); m_pRenderTarget->CreateSolidColorBrush(g_panelHighlightMongolColor, &m_pHighlightMongolBrush);
            m_pRenderTarget->CreateSolidColorBrush(g_panelHighlightChineseColor, &m_pHighlightChineseBrush); m_pRenderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        }
    }
    if (m_pRenderTarget) {
        m_pMongolBrush->SetColor(g_panelMongolColor); m_pChineseBrush->SetColor(g_panelChineseColor); m_pHighlightMongolBrush->SetColor(g_panelHighlightMongolColor); m_pHighlightChineseBrush->SetColor(g_panelHighlightChineseColor);
        m_pRenderTarget->BeginDraw(); m_pRenderTarget->Clear(g_panelBgColor);
        RECT rc; GetClientRect(&rc); float width = (float)rc.right; float height = (float)rc.bottom;

        std::vector<PanelLyricLine> renderLyrics; int currentIdx = -1; double currentTime = GetPlaybackTime();
        if (GetLyrics().empty()) {
            PanelLyricLine nl; nl.time = 0; nl.mongolText = playback_control::get()->is_playing() ? L"\u672A\u627E\u5230\u6B4C\u8BCD" : L"\u64AD\u653E\u5DF2\u505C\u6B62"; renderLyrics.push_back(nl); currentIdx = 0;
        }
        else {
            int tempIdx = -1; for (int i = 0; i < (int)GetLyrics().size(); i++) { if (currentTime >= GetLyrics()[i].time) tempIdx = i; else break; }
            if (tempIdx < 0) { PanelLyricLine nl; nl.time = 0; nl.mongolText = L"\u51C6\u5907\u5C31\u7EEA"; renderLyrics.push_back(nl); currentIdx = 0; }
            else { renderLyrics = GetLyrics(); currentIdx = tempIdx; }
        }

        if (m_pMongolFormat && !renderLyrics.empty()) {
            float visualOffsetX = width * -0.1f; float centerX = width / 2.0f + visualOffsetX; float startY = height / 2.0f - 450.0f; float step = (float)g_panelFontSize * 8.0f; float virtualIdx = 0.0f;

            if (m_isActuallyDragging || m_isSnapping) {
                m_pRenderTarget->DrawLine(
                    D2D1::Point2F(centerX, 0.0f),
                    D2D1::Point2F(centerX, height),
                    m_pHighlightMongolBrush,
                    2.0f,
                    m_pDashedStrokeStyle
                );
            }

            if (renderLyrics.size() == 1) { virtualIdx = 0.0f; }
            else {
                if (m_isActuallyDragging) { virtualIdx = m_currentDragVirtualIdx; }
                else if (m_isSnapping) { float snapProgress = (GetTickCount() - m_animStartTick) / 150.0f; if (snapProgress >= 1.0f) { m_isSnapping = false; virtualIdx = m_snapTargetIdx; } else { float smooth = snapProgress * snapProgress * (3.0f - 2.0f * snapProgress); virtualIdx = m_snapStartIdx + (m_snapTargetIdx - m_snapStartIdx) * smooth; } }
                else {
                    if (currentIdx >= 0 && currentIdx < (int)renderLyrics.size()) {
                        if (currentIdx != m_lastRenderIdx) { m_lastRenderIdx = currentIdx; m_animStartTick = GetTickCount(); }
                        float animProgress = (GetTickCount() - m_animStartTick) / 200.0f; if (animProgress > 1.0f) animProgress = 1.0f;
                        float smoothProgress = animProgress * animProgress * (3.0f - 2.0f * animProgress); virtualIdx = (float)currentIdx;
                        if (currentIdx > 0 && animProgress < 1.0f) virtualIdx = (float)currentIdx - 1.0f + smoothProgress;
                    }
                }
            }

            int highlightIdx = currentIdx;
            if (m_isActuallyDragging) highlightIdx = (int)round(m_currentDragVirtualIdx);
            else if (m_isSnapping) highlightIdx = (int)round(m_snapTargetIdx);

            if (highlightIdx >= 0 && highlightIdx < (int)renderLyrics.size()) {
                int startIdx = highlightIdx > 8 ? highlightIdx - 8 : 0;
                int endIdx = highlightIdx + 15 < (int)renderLyrics.size() ? highlightIdx + 15 : (int)renderLyrics.size() - 1;

                for (int i = startIdx; i <= endIdx; i++) {
                    std::wstring mText = renderLyrics[i].mongolText; std::wstring cText = renderLyrics[i].chineseText; if (g_panelDisplayMode == 1) cText.clear(); if (g_panelDisplayMode == 2) mText.clear(); if (mText.empty() && cText.empty()) continue;
                    std::wstring fullText; std::vector<DWRITE_TEXT_RANGE> spaceRanges; UINT32 mongEnd = 0; UINT32 chStart = 0; UINT32 chLen = 0;
                    if (!mText.empty()) { fullText += mText; mongEnd = (UINT32)fullText.length(); }
                    if (!mText.empty() && !cText.empty()) { fullText += L"\n"; chStart = (UINT32)fullText.length(); for (size_t j = 0; j < cText.length(); ++j) { fullText += cText[j]; if (j < cText.length() - 1) { spaceRanges.push_back({ (UINT32)fullText.length(), 1 }); fullText += L" "; } } chLen = (UINT32)fullText.length() - chStart; }
                    else if (mText.empty() && !cText.empty()) { chStart = 0; for (size_t j = 0; j < cText.length(); ++j) { fullText += cText[j]; if (j < cText.length() - 1) { spaceRanges.push_back({ (UINT32)fullText.length(), 1 }); fullText += L" "; } } chLen = (UINT32)fullText.length(); }
                    if (fullText.empty()) continue;
                    IDWriteTextLayout* pLayout = nullptr;

                    m_pDWriteFactory->CreateTextLayout(fullText.c_str(), (UINT32)fullText.length(), m_pMongolFormat, 5000.0f, 5000.0f, &pLayout);

                    if (pLayout) {
                        if (!mText.empty()) { DWRITE_TEXT_RANGE rangeM = { 0, mongEnd }; pLayout->SetFontFamilyName(g_panelMongolFont.c_str(), rangeM); pLayout->SetDrawingEffect((i == highlightIdx) ? m_pHighlightMongolBrush : m_pMongolBrush, rangeM); }
                        if (!mText.empty() && !cText.empty()) { DWRITE_TEXT_RANGE rangeNewline = { mongEnd, 1 }; pLayout->SetFontSize(1.0f, rangeNewline); pLayout->SetDrawingEffect((i == highlightIdx) ? m_pHighlightMongolBrush : m_pMongolBrush, rangeNewline); DWRITE_TEXT_RANGE rangeC = { chStart, chLen }; pLayout->SetFontFamilyName(g_panelChineseFont.c_str(), rangeC); pLayout->SetFontWeight(DWRITE_FONT_WEIGHT_NORMAL, rangeC); pLayout->SetDrawingEffect((i == highlightIdx) ? m_pHighlightChineseBrush : m_pChineseBrush, rangeC); }
                        else if (mText.empty() && !cText.empty()) { DWRITE_TEXT_RANGE rangeC = { 0, chLen }; pLayout->SetFontFamilyName(g_panelChineseFont.c_str(), rangeC); pLayout->SetFontWeight(DWRITE_FONT_WEIGHT_NORMAL, rangeC); pLayout->SetDrawingEffect((i == highlightIdx) ? m_pHighlightChineseBrush : m_pChineseBrush, rangeC); }
                        for (const auto& r : spaceRanges) { pLayout->SetFontSize(7.0f, r); }

                        DWRITE_TEXT_METRICS metrics;
                        pLayout->GetMetrics(&metrics);
                        float totalW = metrics.width;
                        float lineOffset = (i - virtualIdx) * step;
                        float currentCenterX = centerX + lineOffset;
                        float startX = currentCenterX - totalW / 2.0f;

                        // === 全屏滚动 ===
                        float availableHeight = height - startY - 100.0f;
                        float offsetY = 0.0f;
                        if (i == highlightIdx && metrics.height > availableHeight) {
                            double elapsed = currentTime - renderLyrics[i].time;

                            // 【核心修复】如果正在拖动或吸附，强制从头开始显示
                            if (m_isActuallyDragging || m_isSnapping) elapsed = 0.0;

                            if (elapsed > 0.5) {
                                float overflow = metrics.height - availableHeight + 30.0f;
                                float scrollSpeed = overflow / 3.5f; // 放慢到 3.5 秒
                                offsetY = (float)(elapsed - 0.5) * scrollSpeed;
                                if (offsetY > overflow) offsetY = overflow;
                            }
                        }

                        bool needClip = (offsetY > 0.0f);
                        if (needClip) {
                            D2D1_RECT_F clipRect = D2D1::RectF(0.0f, startY, width, height);
                            m_pRenderTarget->PushAxisAlignedClip(clipRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                        }

                        m_pRenderTarget->DrawTextLayout(D2D1::Point2F(startX, startY - offsetY), pLayout, m_pMongolBrush);

                        if (needClip) {
                            m_pRenderTarget->PopAxisAlignedClip();
                        }

                        pLayout->Release();
                    }
                }
            }
        }
        m_pRenderTarget->EndDraw();
    }
    EndPaint(&ps); return 0;
}

void MongolPanelInstance::LoadLyricsForCurrentTrack() {
    m_lyrics.clear(); metadb_handle_ptr item; if (!playback_control::get()->get_now_playing(item)) return; pfc::string8 lyrics;
    metadb_info_container::ptr info_ref = item->get_info_ref();
    if (info_ref.is_valid()) {
        const file_info& info = info_ref->info(); t_size count = info.meta_get_count();
        for (t_size i = 0; i < count; i++) { const char* name = info.meta_enum_name(i); if (name && (_stricmp(name, "LYRICS") == 0 || _stricmp(name, "UNSYNCEDLYRICS") == 0 || _stricmp(name, "LYRIC") == 0 || _stricmp(name, "UNSYNCED LYRICS") == 0)) { if (info.meta_enum_value_count(i) > 0) { lyrics = info.meta_enum_value(i, 0); break; } } }
    }
    if (lyrics.is_empty()) {
        pfc::string8 filepath = item->get_path(); pfc::stringcvt::string_wide_from_utf8 widePath(filepath); std::wstring wpath = widePath.get_ptr();
        if (wpath.find(L"file://") == 0) { wpath = wpath.substr(7); if (!wpath.empty() && wpath[0] == L'/') wpath = wpath.substr(1); }
        for (wchar_t& c : wpath) { if (c == L'/') c = L'\\'; }
        size_t dotPos = wpath.find_last_of(L'.');
        if (dotPos != std::wstring::npos) {
            std::wstring lrcPath = wpath.substr(0, dotPos) + L".lrc"; std::wstring LRCpath = wpath.substr(0, dotPos) + L".LRC";
            HANDLE hFile = CreateFileW(lrcPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile == INVALID_HANDLE_VALUE) hFile = CreateFileW(LRCpath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                DWORD fileSize = GetFileSize(hFile, NULL);
                if (fileSize > 0 && fileSize < 10 * 1024 * 1024) {
                    char* buffer = new char[fileSize + 1]; DWORD bytesRead = 0;
                    if (ReadFile(hFile, buffer, fileSize, &bytesRead, NULL)) { buffer[fileSize] = '\0'; if (fileSize >= 3 && (unsigned char)buffer[0] == 0xEF && (unsigned char)buffer[1] == 0xBB && (unsigned char)buffer[2] == 0xBF) { lyrics = buffer + 3; } else { lyrics = buffer; } }
                    delete[] buffer;
                }
                CloseHandle(hFile);
            }
        }
    }
    if (!lyrics.is_empty()) {
        std::string content = lyrics.get_ptr(); size_t pos = 0;
        while (pos < content.length()) {
            size_t lineEnd = content.find('\n', pos); if (lineEnd == std::string::npos) lineEnd = content.length();
            std::string line = content.substr(pos, lineEnd - pos); pos = lineEnd + 1; if (!line.empty() && line.back() == '\r') line.pop_back();
            size_t bracketPos = line.find('['); if (bracketPos == std::string::npos) continue; line = line.substr(bracketPos);
            if (line.length() > 2 && line[0] == '[') {
                size_t closeBracket = line.find(']');
                if (closeBracket != std::string::npos) {
                    std::string timeStr = line.substr(1, closeBracket - 1); std::string text = line.substr(closeBracket + 1); int min = 0, sec = 0, ms = 0;
                    if (sscanf_s(timeStr.c_str(), "%d:%d.%d", &min, &sec, &ms) >= 2) {
                        size_t dot = timeStr.find('.'); int msDigits = 2; if (dot != std::string::npos) msDigits = (int)(timeStr.length() - dot - 1); double msFactor = (msDigits == 3) ? 1000.0 : 100.0; double totalSeconds = min * 60.0 + sec + (ms / msFactor);
                        pfc::stringcvt::string_wide_from_utf8 wideText(text.c_str()); std::wstring wText = wideText.get_ptr(); size_t first = wText.find_first_not_of(L" ");
                        if (first != std::wstring::npos) {
                            size_t last = wText.find_last_not_of(L" "); wText = wText.substr(first, (last - first + 1));
                            if (!wText.empty()) {
                                bool hasMongol = false; for (wchar_t c : wText) { if (c >= 0x1800 && c <= 0x18AF) { hasMongol = true; break; } }
                                if (!m_lyrics.empty() && abs(m_lyrics.back().time - totalSeconds) < 1.5) {
                                    if (hasMongol && m_lyrics.back().mongolText.empty()) m_lyrics.back().mongolText = wText; else if (!hasMongol && m_lyrics.back().chineseText.empty()) m_lyrics.back().chineseText = wText;
                                    else { PanelLyricLine nl = { totalSeconds, L"", L"" }; if (hasMongol) nl.mongolText = wText; else nl.chineseText = wText; m_lyrics.push_back(nl); }
                                }
                                else { PanelLyricLine nl = { totalSeconds, L"", L"" }; if (hasMongol) nl.mongolText = wText; else nl.chineseText = wText; m_lyrics.push_back(nl); }
                            }
                        }
                    }
                }
            }
        }
    }
}

void MongolPanelInstance::ReloadLyrics() { LoadLyricsForCurrentTrack(); m_lastRenderIdx = -1; Invalidate(); }
void MongolPanelInstance::OnFullScreenClosed() { m_pFullScreen = nullptr; }
void MongolPanelInstance::OpenFullScreen() { if (m_pFullScreen) return; m_pFullScreen = new FullScreenLyric(this); m_pFullScreen->InitFullScreen(); }

MongolPanelInstance::MongolPanelInstance(ui_element_config::ptr cfg, ui_element_instance_callback::ptr cb) : m_callback(cb) {
    LoadPanelSettings(); m_lastReloadFlag = g_reload_flag;
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&m_pDWriteFactory));
    m_lastFontSize = g_panelFontSize; m_lastMFont = g_panelMongolFont; m_lastCFont = g_panelChineseFont;
    m_pDWriteFactory->CreateTextFormat(g_panelMongolFont.c_str(), nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, (float)g_panelFontSize, L"mn-MN", &m_pMongolFormat);
    m_pMongolFormat->SetReadingDirection(DWRITE_READING_DIRECTION_TOP_TO_BOTTOM);
    m_pMongolFormat->SetFlowDirection(DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT);
    m_pMongolFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    m_pMongolFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    m_pMongolFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    D2D1_STROKE_STYLE_PROPERTIES strokeProps = D2D1::StrokeStyleProperties(
        D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT,
        D2D1_LINE_JOIN_MITER, 10.0f, D2D1_DASH_STYLE_DASH, 0.0f);
    m_pD2DFactory->CreateStrokeStyle(strokeProps, nullptr, 0, &m_pDashedStrokeStyle);
}

MongolPanelInstance::~MongolPanelInstance() {
    if (m_pMongolBrush) m_pMongolBrush->Release(); if (m_pChineseBrush) m_pChineseBrush->Release();
    if (m_pHighlightMongolBrush) m_pHighlightMongolBrush->Release(); if (m_pHighlightChineseBrush) m_pHighlightChineseBrush->Release();
    if (m_pMongolFormat) m_pMongolFormat->Release(); if (m_pDWriteFactory) m_pDWriteFactory->Release();
    if (m_pRenderTarget) m_pRenderTarget->Release(); if (m_pD2DFactory) m_pD2DFactory->Release();
    if (m_pDashedStrokeStyle) m_pDashedStrokeStyle->Release();
}
void MongolPanelInstance::initialize_window(HWND parent) { WIN32_OP(Create(parent) != NULL); SetTimer(1, 16, NULL); }
void MongolPanelInstance::notify(const GUID& p_what, t_size p_param1, const void* p_param2, t_size p_param2size) { if (p_what == ui_element_notify_colors_changed || p_what == ui_element_notify_visibility_changed) Invalidate(); }
LRESULT MongolPanelInstance::OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&) { return 1; }

LRESULT MongolPanelInstance::OnMouseWheel(UINT, WPARAM wParam, LPARAM, BOOL&) {
    if (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) {
        static DWORD lastWheelTime = 0;
        DWORD currentTime = GetTickCount();
        if (currentTime - lastWheelTime < 50) return 0;
        lastWheelTime = currentTime;

        short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (zDelta > 0) { g_panelFontSize += 1; if (g_panelFontSize > 60) g_panelFontSize = 60; }
        else { g_panelFontSize -= 1; if (g_panelFontSize < 10) g_panelFontSize = 10; }
        SavePanelSettings(); Invalidate(); return 0;
    }
    return 0;
}

LRESULT MongolPanelInstance::OnContextMenu(UINT, WPARAM, LPARAM lParam, BOOL&) {
    HMENU hMenu = CreatePopupMenu(); HMENU hSubMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING | (g_isVisible ? MF_CHECKED : MF_UNCHECKED), 1, L"\u542F\u7528\u684C\u9762\u6B4C\u8BCD");
    AppendMenuW(hMenu, MF_STRING | (g_isLocked ? MF_CHECKED : MF_UNCHECKED), 2, L"\u9501\u5B9A\u684C\u9762\u6B4C\u8BCD");
    AppendMenuW(hMenu, MF_STRING | (g_isTopmost ? MF_CHECKED : MF_UNCHECKED), 3, L"\u7F6E\u9876\u684C\u9762\u6B4C\u8BCD");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, 4, m_pFullScreen ? L"\u9000\u51FA\u5168\u5C4F\u6B4C\u8BCD" : L"\u5168\u5C4F\u663E\u793A\u6B4C\u8BCD");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hSubMenu, MF_STRING | (g_panelDisplayMode == 0 ? MF_CHECKED : MF_UNCHECKED), 10, L"\u663E\u793A\u53CC\u8BED\u6B4C\u8BCD");
    AppendMenuW(hSubMenu, MF_STRING | (g_panelDisplayMode == 1 ? MF_CHECKED : MF_UNCHECKED), 11, L"\u4EC5\u663E\u793A\u8499\u53E4\u6587");
    AppendMenuW(hSubMenu, MF_STRING | (g_panelDisplayMode == 2 ? MF_CHECKED : MF_UNCHECKED), 12, L"\u4EC5\u663E\u793A\u4E2D\u6587\u7FFB\u8BD1");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubMenu, L"\u53CC\u8BED\u6B4C\u8BCD\u663E\u793A");
    AppendMenuW(hMenu, MF_STRING, 5, L"\u91CD\u65B0\u8F7D\u5165\u6B4C\u8BCD");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, 6, L"\u9762\u677F\u6B4C\u8BCD\u8BBE\u7F6E");

    POINT pt; GetCursorPos(&pt);
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, m_hWnd, NULL);
    DestroyMenu(hSubMenu); DestroyMenu(hMenu);

    if (cmd == 1) { g_isVisible = !g_isVisible; if (g_hwndLyrics) ::ShowWindow(g_hwndLyrics, g_isVisible ? SW_SHOW : SW_HIDE); }
    else if (cmd == 2) {
        g_isLocked = !g_isLocked;
        if (g_hwndLyrics) {
            LONG_PTR exStyle = ::GetWindowLongPtr(g_hwndLyrics, GWL_EXSTYLE);
            if (g_isLocked) ::SetWindowLongPtr(g_hwndLyrics, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT);
            else ::SetWindowLongPtr(g_hwndLyrics, GWL_EXSTYLE, exStyle & ~WS_EX_TRANSPARENT);
        }
    }
    else if (cmd == 3) { g_isTopmost = !g_isTopmost; if (g_hwndLyrics) ::SetWindowPos(g_hwndLyrics, g_isTopmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); }
    else if (cmd == 4) { if (m_pFullScreen) { m_pFullScreen->PostMessage(WM_CLOSE); } else { OpenFullScreen(); } }
    else if (cmd == 5) { ReloadLyrics(); }
    else if (cmd == 6) { fb2k::inMainThread([] { static_api_ptr_t<ui_control>()->show_preferences(guid_pref_panel); }); }
    else if (cmd >= 10 && cmd <= 12) { g_panelDisplayMode = cmd - 10; SavePanelSettings(); Invalidate(); }
    return 0;
}

LRESULT MongolPanelInstance::OnTimer(UINT, WPARAM, LPARAM, BOOL&) {
    if (m_lastFontSize != g_panelFontSize || m_lastMFont != g_panelMongolFont || m_lastCFont != g_panelChineseFont) {
        m_lastFontSize = g_panelFontSize; m_lastMFont = g_panelMongolFont; m_lastCFont = g_panelChineseFont;
        if (m_pMongolFormat) m_pMongolFormat->Release();
        m_pDWriteFactory->CreateTextFormat(g_panelMongolFont.c_str(), nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, (float)g_panelFontSize, L"mn-MN", &m_pMongolFormat);
        m_pMongolFormat->SetReadingDirection(DWRITE_READING_DIRECTION_TOP_TO_BOTTOM);
        m_pMongolFormat->SetFlowDirection(DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT);
        m_pMongolFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        m_pMongolFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        m_pMongolFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }
    if (m_lastReloadFlag != g_reload_flag) { m_lastReloadFlag = g_reload_flag; ReloadLyrics(); }
    metadb_handle_ptr item; std::wstring currentPath = L"";
    if (playback_control::get()->get_now_playing(item)) {
        pfc::string8 path = item->get_path();
        pfc::stringcvt::string_wide_from_utf8 widePath(path);
        currentPath = widePath.get_ptr();
    }
    if (currentPath != m_currentTrackPath) { m_currentTrackPath = currentPath; LoadLyricsForCurrentTrack(); m_lastRenderIdx = -1; }
    Invalidate(); return 0;
}

LRESULT MongolPanelInstance::OnLButtonDblClk(UINT, WPARAM, LPARAM, BOOL&) { OpenFullScreen(); return 0; }

LRESULT MongolPanelInstance::OnLButtonDown(UINT, WPARAM, LPARAM lParam, BOOL&) {
    if (m_lyrics.empty()) return 0;
    m_isMouseDown = true; m_isActuallyDragging = false; m_isSnapping = false; m_dragStartX = GET_X_LPARAM(lParam);
    double currentTime = playback_control::get()->playback_get_position();
    int currentIdx = -1;
    for (int i = 0; i < (int)m_lyrics.size(); i++) {
        if (currentTime >= m_lyrics[i].time) currentIdx = i;
        else break;
    }
    m_dragStartVirtualIdx = (float)currentIdx; m_currentDragVirtualIdx = m_dragStartVirtualIdx; SetCapture(); return 0;
}

LRESULT MongolPanelInstance::OnMouseMove(UINT, WPARAM, LPARAM lParam, BOOL&) {
    if (m_isMouseDown) {
        int mouseX = GET_X_LPARAM(lParam); int deltaX = mouseX - m_dragStartX;
        if (std::abs(deltaX) > 5) m_isActuallyDragging = true;
        if (m_isActuallyDragging) {
            float step = (float)g_panelFontSize * 4.0f;
            m_currentDragVirtualIdx = m_dragStartVirtualIdx - (deltaX / step);
            if (m_currentDragVirtualIdx < 0.0f) m_currentDragVirtualIdx = 0.0f;
            if (m_currentDragVirtualIdx > (float)(m_lyrics.size() - 1)) m_currentDragVirtualIdx = (float)(m_lyrics.size() - 1);
            Invalidate();
        }
    }
    return 0;
}

LRESULT MongolPanelInstance::OnLButtonUp(UINT, WPARAM, LPARAM, BOOL&) {
    if (m_isMouseDown) {
        m_isMouseDown = false; ReleaseCapture();
        if (m_isActuallyDragging) {
            m_isActuallyDragging = false;
            int targetIdx = (int)round(m_currentDragVirtualIdx);
            if (targetIdx >= 0 && targetIdx < (int)m_lyrics.size()) {
                playback_control::get()->playback_seek(m_lyrics[targetIdx].time);
                if (std::abs(m_currentDragVirtualIdx - targetIdx) > 0.01f) {
                    m_isSnapping = true; m_snapStartIdx = m_currentDragVirtualIdx; m_snapTargetIdx = (float)targetIdx; m_animStartTick = GetTickCount();
                }
                m_lastRenderIdx = targetIdx;
            }
            Invalidate();
        }
    }
    return 0;
}

LRESULT MongolPanelInstance::OnSize(UINT, WPARAM, LPARAM, BOOL&) {
    if (m_pRenderTarget) { RECT rc; GetClientRect(&rc); m_pRenderTarget->Resize(D2D1::SizeU(rc.right, rc.bottom)); }
    return 0;
}

LRESULT MongolPanelInstance::OnPaint(UINT, WPARAM, LPARAM, BOOL&) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(&ps);
    if (!m_pRenderTarget) {
        RECT rc; GetClientRect(&rc);
        m_pD2DFactory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(), D2D1::HwndRenderTargetProperties(m_hWnd, D2D1::SizeU(rc.right, rc.bottom)), &m_pRenderTarget);
        if (m_pRenderTarget) {
            m_pRenderTarget->CreateSolidColorBrush(g_panelMongolColor, &m_pMongolBrush);
            m_pRenderTarget->CreateSolidColorBrush(g_panelChineseColor, &m_pChineseBrush);
            m_pRenderTarget->CreateSolidColorBrush(g_panelHighlightMongolColor, &m_pHighlightMongolBrush);
            m_pRenderTarget->CreateSolidColorBrush(g_panelHighlightChineseColor, &m_pHighlightChineseBrush);
            m_pRenderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        }
    }
    if (m_pRenderTarget) {
        m_pMongolBrush->SetColor(g_panelMongolColor);
        m_pChineseBrush->SetColor(g_panelChineseColor);
        m_pHighlightMongolBrush->SetColor(g_panelHighlightMongolColor);
        m_pHighlightChineseBrush->SetColor(g_panelHighlightChineseColor);

        m_pRenderTarget->BeginDraw();
        m_pRenderTarget->Clear(g_panelBgColor);

        RECT rc; GetClientRect(&rc);
        float width = (float)rc.right;
        float height = (float)rc.bottom;

        std::vector<PanelLyricLine> renderLyrics; int currentIdx = -1; double currentTime = playback_control::get()->playback_get_position(); if (currentTime < 0) currentTime = 0;
        if (m_lyrics.empty()) {
            PanelLyricLine nl; nl.time = 0; nl.mongolText = playback_control::get()->is_playing() ? L"\u672A\u627E\u5230\u6B4C\u8BCD" : L"\u64AD\u653E\u5DF2\u505C\u6B62"; renderLyrics.push_back(nl); currentIdx = 0;
        }
        else {
            int tempIdx = -1; for (int i = 0; i < (int)m_lyrics.size(); i++) { if (currentTime >= m_lyrics[i].time) tempIdx = i; else break; }
            if (tempIdx < 0) { PanelLyricLine nl; nl.time = 0; nl.mongolText = L"\u51C6\u5907\u5C31\u7EEA"; renderLyrics.push_back(nl); currentIdx = 0; }
            else { renderLyrics = m_lyrics; currentIdx = tempIdx; }
        }

        if (m_pMongolFormat && !renderLyrics.empty()) {
            float visualOffsetX = width * -0.075f; float centerX = width / 2.0f + visualOffsetX; float virtualIdx = 0.0f;

            if (m_isActuallyDragging || m_isSnapping) {
                m_pRenderTarget->DrawLine(
                    D2D1::Point2F(centerX, 0.0f),
                    D2D1::Point2F(centerX, height),
                    m_pHighlightMongolBrush,
                    1.0f,
                    m_pDashedStrokeStyle
                );
            }

            if (renderLyrics.size() == 1) { virtualIdx = 0.0f; }
            else {
                if (m_isActuallyDragging) { virtualIdx = m_currentDragVirtualIdx; }
                else if (m_isSnapping) { float snapProgress = (GetTickCount() - m_animStartTick) / 150.0f; if (snapProgress >= 1.0f) { m_isSnapping = false; virtualIdx = m_snapTargetIdx; } else { float smooth = snapProgress * snapProgress * (3.0f - 2.0f * snapProgress); virtualIdx = m_snapStartIdx + (m_snapTargetIdx - m_snapStartIdx) * smooth; } }
                else {
                    if (currentIdx >= 0 && currentIdx < (int)renderLyrics.size()) {
                        if (currentIdx != m_lastRenderIdx) { m_lastRenderIdx = currentIdx; m_animStartTick = GetTickCount(); }
                        float animProgress = (GetTickCount() - m_animStartTick) / 200.0f; if (animProgress > 1.0f) animProgress = 1.0f;
                        float smoothProgress = animProgress * animProgress * (3.0f - 2.0f * animProgress); virtualIdx = (float)currentIdx;
                        if (currentIdx > 0 && animProgress < 1.0f) virtualIdx = (float)currentIdx - 1.0f + smoothProgress;
                    }
                }
            }

            int highlightIdx = currentIdx;
            if (m_isActuallyDragging) highlightIdx = (int)round(m_currentDragVirtualIdx);
            else if (m_isSnapping) highlightIdx = (int)round(m_snapTargetIdx);

            if (highlightIdx >= 0 && highlightIdx < (int)renderLyrics.size()) {
                int startIdx = highlightIdx > 8 ? highlightIdx - 8 : 0;
                int endIdx = highlightIdx + 15 < (int)renderLyrics.size() ? highlightIdx + 15 : (int)renderLyrics.size() - 1;
                float step = (float)g_panelFontSize * 4.0f;

                for (int i = startIdx; i <= endIdx; i++) {
                    std::wstring mText = renderLyrics[i].mongolText; std::wstring cText = renderLyrics[i].chineseText; if (g_panelDisplayMode == 1) cText.clear(); if (g_panelDisplayMode == 2) mText.clear(); if (mText.empty() && cText.empty()) continue;
                    std::wstring fullText; std::vector<DWRITE_TEXT_RANGE> spaceRanges; UINT32 mongEnd = 0; UINT32 chStart = 0; UINT32 chLen = 0;
                    if (!mText.empty()) { fullText += mText; mongEnd = (UINT32)fullText.length(); }
                    if (!mText.empty() && !cText.empty()) { fullText += L"\n"; chStart = (UINT32)fullText.length(); for (size_t j = 0; j < cText.length(); ++j) { fullText += cText[j]; if (j < cText.length() - 1) { spaceRanges.push_back({ (UINT32)fullText.length(), 1 }); fullText += L" "; } } chLen = (UINT32)fullText.length() - chStart; }
                    else if (mText.empty() && !cText.empty()) { chStart = 0; for (size_t j = 0; j < cText.length(); ++j) { fullText += cText[j]; if (j < cText.length() - 1) { spaceRanges.push_back({ (UINT32)fullText.length(), 1 }); fullText += L" "; } } chLen = (UINT32)fullText.length(); }
                    if (fullText.empty()) continue;
                    IDWriteTextLayout* pLayout = nullptr;

                    m_pDWriteFactory->CreateTextLayout(fullText.c_str(), (UINT32)fullText.length(), m_pMongolFormat, 5000.0f, 5000.0f, &pLayout);

                    if (pLayout) {
                        if (!mText.empty()) { DWRITE_TEXT_RANGE rangeM = { 0, mongEnd }; pLayout->SetFontFamilyName(g_panelMongolFont.c_str(), rangeM); pLayout->SetDrawingEffect((i == highlightIdx) ? m_pHighlightMongolBrush : m_pMongolBrush, rangeM); }
                        if (!mText.empty() && !cText.empty()) { DWRITE_TEXT_RANGE rangeNewline = { mongEnd, 1 }; pLayout->SetFontSize(1.0f, rangeNewline); pLayout->SetDrawingEffect((i == highlightIdx) ? m_pHighlightMongolBrush : m_pMongolBrush, rangeNewline); DWRITE_TEXT_RANGE rangeC = { chStart, chLen }; pLayout->SetFontFamilyName(g_panelChineseFont.c_str(), rangeC); pLayout->SetFontWeight(DWRITE_FONT_WEIGHT_NORMAL, rangeC); pLayout->SetDrawingEffect((i == highlightIdx) ? m_pHighlightChineseBrush : m_pChineseBrush, rangeC); }
                        else if (mText.empty() && !cText.empty()) { DWRITE_TEXT_RANGE rangeC = { 0, chLen }; pLayout->SetFontFamilyName(g_panelChineseFont.c_str(), rangeC); pLayout->SetFontWeight(DWRITE_FONT_WEIGHT_NORMAL, rangeC); pLayout->SetDrawingEffect((i == highlightIdx) ? m_pHighlightChineseBrush : m_pChineseBrush, rangeC); }
                        for (const auto& r : spaceRanges) { pLayout->SetFontSize(7.0f, r); }

                        DWRITE_TEXT_METRICS metrics;
                        pLayout->GetMetrics(&metrics);
                        float totalW = metrics.width;
                        float lineOffset = (i - virtualIdx) * step;
                        float currentCenterX = centerX + lineOffset;
                        float startX = currentCenterX - totalW / 2.0f;

                        // === 面板滚动 ===
                        float startY = 20.0f;
                        float availableHeight = height - startY - 40.0f;
                        float offsetY = 0.0f;
                        if (i == highlightIdx && metrics.height > availableHeight) {
                            double elapsed = currentTime - renderLyrics[i].time;

                            // 【核心修复】如果正在拖动或吸附，强制从头开始显示
                            if (m_isActuallyDragging || m_isSnapping) elapsed = 0.0;

                            if (elapsed > 0.5) {
                                float overflow = metrics.height - availableHeight + 20.0f;
                                float scrollSpeed = overflow / 3.5f; // 放慢到 3.5 秒
                                offsetY = (float)(elapsed - 0.5) * scrollSpeed;
                                if (offsetY > overflow) offsetY = overflow;
                            }
                        }

                        bool needClip = (offsetY > 0.0f);
                        if (needClip) {
                            D2D1_RECT_F clipRect = D2D1::RectF(0.0f, startY, width, height);
                            m_pRenderTarget->PushAxisAlignedClip(clipRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                        }

                        m_pRenderTarget->DrawTextLayout(D2D1::Point2F(startX, startY - offsetY), pLayout, m_pMongolBrush);

                        if (needClip) {
                            m_pRenderTarget->PopAxisAlignedClip();
                        }

                        pLayout->Release();
                    }
                }
            }
        }
        m_pRenderTarget->EndDraw();
    }
    EndPaint(&ps); return 0;
}

static service_factory_single_t< ui_element_impl< MongolPanelInstance > > g_panel_factory;