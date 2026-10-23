#include "stdafx.h"
#include <windows.h>
#include <cmath>
#include <string>
#include <vector>
#include <d2d1.h>
#include <d2d1helper.h> 
#include <dwrite.h>
#include <objbase.h>
#include <SDK/initquit.h>
#include <SDK/playback_control.h> 
#include <SDK/metadb.h>           
#include <SDK/ui.h> 
#include <SDK/core_api.h>

#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d2d1.lib")

extern const GUID guid_preferences_page;

DECLARE_COMPONENT_VERSION(
    "\xE8\x92\x99\xE5\x8F\xA4\xE6\x96\x87\xE6\xAD\x8C\xE8\xAF\x8D",
    "1.0.0",
    "Author: lsfacg\n"
    "A Desktop Lyric plugin for Traditional Mongolian.\n"
    "Special thanks to the Wucai Hulunbuir Children's Choir.\n"
    "Github: https://github.com/lsfacg/foo_mongol_lyric"
);

bool g_isVisible = false;
bool g_isLocked = false;
bool g_isTopmost = true;
HWND g_hwndLyrics = NULL;
int g_displayMode = 0;

int g_windowWidth = 250;
int g_windowHeight = 800;
int g_fontSize = 40;
bool g_isHovered = false;
int g_hoveredButton = -1;

double g_syncOffset = 0.115;

std::wstring g_mongolFont = L"Mongolian Baiti";
std::wstring g_chineseFont = L"Microsoft YaHei";
D2D1_COLOR_F g_mongolColor = D2D1::ColorF(0.0f, 1.0f, 0.0f, 1.0f);
D2D1_COLOR_F g_chineseColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
D2D1_COLOR_F g_mongolOutlineColor = D2D1::ColorF(0, 0, 0, 1);
D2D1_COLOR_F g_chineseOutlineColor = D2D1::ColorF(0, 0, 0, 1);
float g_mongolOutlineSize = 0.7f;
float g_chineseOutlineSize = 0.7f;

struct LyricLine {
    double time;
    std::wstring mongolText;
    std::wstring chineseText;
};

std::vector<LyricLine> g_Lyrics;
std::wstring g_currentMongolText = L"等待播放...";
std::wstring g_currentChineseText = L"";
int g_currentLineIndex = -1;

double g_playbackTime = 0.0;
DWORD g_lastTick = 0;
bool g_isPlaying = false;

CRITICAL_SECTION g_csLyrics;
const wchar_t* REG_PATH = L"Software\\foo_mongol_lyric";

ID2D1Factory* g_pD2DFactory = nullptr;
IDWriteFactory* g_pDWriteFactory = nullptr;
IDWriteTextFormat* g_pMongolFormat = nullptr;
IDWriteTextFormat* g_pUIFontFormat = nullptr;
ID2D1DCRenderTarget* g_pDCRenderTarget = nullptr;

ID2D1SolidColorBrush* g_pMongolBrush = nullptr;
ID2D1SolidColorBrush* g_pChineseBrush = nullptr;
ID2D1SolidColorBrush* g_pMongolOutlineBrush = nullptr;
ID2D1SolidColorBrush* g_pChineseOutlineBrush = nullptr;
ID2D1SolidColorBrush* g_pHoverBrush = nullptr;
ID2D1SolidColorBrush* g_pUIBrush = nullptr;
ID2D1SolidColorBrush* g_pUIHoverBrush = nullptr;

void UpdateFontSize(int delta) {
    g_fontSize += delta;
    if (g_fontSize < 12) g_fontSize = 12;
    if (g_fontSize > 120) g_fontSize = 120;

    if (g_pMongolFormat) g_pMongolFormat->Release();
    g_pDWriteFactory->CreateTextFormat(g_mongolFont.c_str(), nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, (float)g_fontSize, L"mn-MN", &g_pMongolFormat);
    g_pMongolFormat->SetReadingDirection(DWRITE_READING_DIRECTION_TOP_TO_BOTTOM);
    g_pMongolFormat->SetFlowDirection(DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT);
    g_pMongolFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    g_pMongolFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    g_pMongolFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    float scale = (float)g_fontSize / 40.0f;
    g_windowWidth = (int)(250 * scale);
    g_windowHeight = (int)(800 * scale);

    RECT rc;
    GetWindowRect(g_hwndLyrics, &rc);
    int cx = rc.left + (rc.right - rc.left) / 2;
    int cy = rc.top + (rc.bottom - rc.top) / 2;

    SetWindowPos(g_hwndLyrics, NULL, cx - g_windowWidth / 2, cy - g_windowHeight / 2, g_windowWidth, g_windowHeight, SWP_NOZORDER);
}

void InitDWrite() {
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_pD2DFactory);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&g_pDWriteFactory));

    g_pDWriteFactory->CreateTextFormat(
        g_mongolFont.c_str(), nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        (float)g_fontSize, L"mn-MN", &g_pMongolFormat
    );
    g_pMongolFormat->SetReadingDirection(DWRITE_READING_DIRECTION_TOP_TO_BOTTOM);
    g_pMongolFormat->SetFlowDirection(DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT);
    g_pMongolFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    g_pMongolFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    g_pMongolFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    g_pDWriteFactory->CreateTextFormat(
        g_chineseFont.c_str(), nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        20.0f, L"zh-CN", &g_pUIFontFormat
    );
    g_pUIFontFormat->SetReadingDirection(DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    g_pUIFontFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    g_pUIFontFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );
    g_pD2DFactory->CreateDCRenderTarget(&props, &g_pDCRenderTarget);

    g_pDCRenderTarget->CreateSolidColorBrush(g_mongolColor, &g_pMongolBrush);
    g_pDCRenderTarget->CreateSolidColorBrush(g_chineseColor, &g_pChineseBrush);
    g_pDCRenderTarget->CreateSolidColorBrush(g_mongolOutlineColor, &g_pMongolOutlineBrush);
    g_pDCRenderTarget->CreateSolidColorBrush(g_chineseOutlineColor, &g_pChineseOutlineBrush);
    g_pDCRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.2f, 0.2f, 0.2f, 0.5f), &g_pHoverBrush);
    g_pDCRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.85f, 0.85f, 0.85f, 1.0f), &g_pUIBrush);
    g_pDCRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &g_pUIHoverBrush);
}

void LoadSettings(HWND hwnd) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_PATH, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type, size;
        int x = 100, y = 100, vis = 0, fs = 40, locked = 0, topmost = 1;
        size = sizeof(int);
        RegQueryValueExW(hKey, L"X", NULL, &type, (LPBYTE)&x, &size);
        RegQueryValueExW(hKey, L"Y", NULL, &type, (LPBYTE)&y, &size);
        RegQueryValueExW(hKey, L"Visible", NULL, &type, (LPBYTE)&vis, &size);
        RegQueryValueExW(hKey, L"FontSize", NULL, &type, (LPBYTE)&fs, &size);
        RegQueryValueExW(hKey, L"Locked", NULL, &type, (LPBYTE)&locked, &size);
        RegQueryValueExW(hKey, L"Topmost", NULL, &type, (LPBYTE)&topmost, &size);

        g_isVisible = (vis == 1);
        g_fontSize = fs;
        g_isLocked = (locked == 1);
        g_isTopmost = (topmost == 1);

        wchar_t fontBuf[256] = { 0 };
        size = sizeof(fontBuf);
        if (RegQueryValueExW(hKey, L"MongolFont", NULL, &type, (LPBYTE)fontBuf, &size) == ERROR_SUCCESS && type == REG_SZ) g_mongolFont = fontBuf;
        size = sizeof(fontBuf);
        if (RegQueryValueExW(hKey, L"ChineseFont", NULL, &type, (LPBYTE)fontBuf, &size) == ERROR_SUCCESS && type == REG_SZ) g_chineseFont = fontBuf;

        DWORD col = 0;
        size = sizeof(DWORD);
        if (RegQueryValueExW(hKey, L"MongolColor", NULL, &type, (LPBYTE)&col, &size) == ERROR_SUCCESS && type == REG_DWORD) g_mongolColor = D2D1::ColorF(GetRValue(col) / 255.0f, GetGValue(col) / 255.0f, GetBValue(col) / 255.0f);
        if (RegQueryValueExW(hKey, L"ChineseColor", NULL, &type, (LPBYTE)&col, &size) == ERROR_SUCCESS && type == REG_DWORD) g_chineseColor = D2D1::ColorF(GetRValue(col) / 255.0f, GetGValue(col) / 255.0f, GetBValue(col) / 255.0f);
        if (RegQueryValueExW(hKey, L"MongolOutlineColor", NULL, &type, (LPBYTE)&col, &size) == ERROR_SUCCESS && type == REG_DWORD) g_mongolOutlineColor = D2D1::ColorF(GetRValue(col) / 255.0f, GetGValue(col) / 255.0f, GetBValue(col) / 255.0f);
        if (RegQueryValueExW(hKey, L"ChineseOutlineColor", NULL, &type, (LPBYTE)&col, &size) == ERROR_SUCCESS && type == REG_DWORD) g_chineseOutlineColor = D2D1::ColorF(GetRValue(col) / 255.0f, GetGValue(col) / 255.0f, GetBValue(col) / 255.0f);

        int outlineSizeInt = 7;
        size = sizeof(int);
        if (RegQueryValueExW(hKey, L"MongolOutlineSize", NULL, &type, (LPBYTE)&outlineSizeInt, &size) == ERROR_SUCCESS) g_mongolOutlineSize = outlineSizeInt / 10.0f;
        if (RegQueryValueExW(hKey, L"ChineseOutlineSize", NULL, &type, (LPBYTE)&outlineSizeInt, &size) == ERROR_SUCCESS) g_chineseOutlineSize = outlineSizeInt / 10.0f;

        float scale = (float)g_fontSize / 40.0f;
        g_windowWidth = (int)(250 * scale);
        g_windowHeight = (int)(800 * scale);

        SetWindowPos(hwnd, NULL, x, y, g_windowWidth, g_windowHeight, SWP_NOZORDER);
        RegCloseKey(hKey);
    }
    else {
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int defaultX = screenWidth - g_windowWidth - 50;
        SetWindowPos(hwnd, NULL, defaultX, 150, g_windowWidth, g_windowHeight, SWP_NOZORDER);
    }
}

void SaveSettings(HWND hwnd) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_PATH, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RECT rc;
        GetWindowRect(hwnd, &rc);
        int x = rc.left, y = rc.top;
        int vis = g_isVisible ? 1 : 0;
        int fs = g_fontSize;
        int locked = g_isLocked ? 1 : 0;
        int topmost = g_isTopmost ? 1 : 0;

        RegSetValueExW(hKey, L"X", 0, REG_DWORD, (const BYTE*)&x, sizeof(int));
        RegSetValueExW(hKey, L"Y", 0, REG_DWORD, (const BYTE*)&y, sizeof(int));
        RegSetValueExW(hKey, L"Visible", 0, REG_DWORD, (const BYTE*)&vis, sizeof(int));
        RegSetValueExW(hKey, L"FontSize", 0, REG_DWORD, (const BYTE*)&fs, sizeof(int));
        RegSetValueExW(hKey, L"Locked", 0, REG_DWORD, (const BYTE*)&locked, sizeof(int));
        RegSetValueExW(hKey, L"Topmost", 0, REG_DWORD, (const BYTE*)&topmost, sizeof(int));

        RegSetValueExW(hKey, L"MongolFont", 0, REG_SZ, (const BYTE*)g_mongolFont.c_str(), (g_mongolFont.length() + 1) * sizeof(wchar_t));
        RegSetValueExW(hKey, L"ChineseFont", 0, REG_SZ, (const BYTE*)g_chineseFont.c_str(), (g_chineseFont.length() + 1) * sizeof(wchar_t));

        DWORD colM = RGB((BYTE)(g_mongolColor.r * 255), (BYTE)(g_mongolColor.g * 255), (BYTE)(g_mongolColor.b * 255));
        DWORD colC = RGB((BYTE)(g_chineseColor.r * 255), (BYTE)(g_chineseColor.g * 255), (BYTE)(g_chineseColor.b * 255));
        RegSetValueExW(hKey, L"MongolColor", 0, REG_DWORD, (const BYTE*)&colM, sizeof(DWORD));
        RegSetValueExW(hKey, L"ChineseColor", 0, REG_DWORD, (const BYTE*)&colC, sizeof(DWORD));

        DWORD colOM = RGB((BYTE)(g_mongolOutlineColor.r * 255), (BYTE)(g_mongolOutlineColor.g * 255), (BYTE)(g_mongolOutlineColor.b * 255));
        DWORD colOC = RGB((BYTE)(g_chineseOutlineColor.r * 255), (BYTE)(g_chineseOutlineColor.g * 255), (BYTE)(g_chineseOutlineColor.b * 255));
        RegSetValueExW(hKey, L"MongolOutlineColor", 0, REG_DWORD, (const BYTE*)&colOM, sizeof(DWORD));
        RegSetValueExW(hKey, L"ChineseOutlineColor", 0, REG_DWORD, (const BYTE*)&colOC, sizeof(DWORD));

        int osM = (int)(g_mongolOutlineSize * 10);
        int osC = (int)(g_chineseOutlineSize * 10);
        RegSetValueExW(hKey, L"MongolOutlineSize", 0, REG_DWORD, (const BYTE*)&osM, sizeof(int));
        RegSetValueExW(hKey, L"ChineseOutlineSize", 0, REG_DWORD, (const BYTE*)&osC, sizeof(int));

        RegCloseKey(hKey);
    }
}

void ApplySettingsAndRefresh() {
    if (g_hwndLyrics) {
        ShowWindow(g_hwndLyrics, g_isVisible ? SW_SHOW : SW_HIDE);

        LONG_PTR exStyle = GetWindowLongPtr(g_hwndLyrics, GWL_EXSTYLE);
        if (g_isLocked) exStyle |= WS_EX_TRANSPARENT;
        else exStyle &= ~WS_EX_TRANSPARENT;
        SetWindowLongPtr(g_hwndLyrics, GWL_EXSTYLE, exStyle);

        SetWindowPos(g_hwndLyrics, g_isTopmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

        if (g_pMongolFormat) g_pMongolFormat->Release();
        g_pDWriteFactory->CreateTextFormat(g_mongolFont.c_str(), nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, (float)g_fontSize, L"mn-MN", &g_pMongolFormat);
        g_pMongolFormat->SetReadingDirection(DWRITE_READING_DIRECTION_TOP_TO_BOTTOM);
        g_pMongolFormat->SetFlowDirection(DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT);
        g_pMongolFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        g_pMongolFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        g_pMongolFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

        float scale = (float)g_fontSize / 40.0f;
        g_windowWidth = (int)(250 * scale);
        g_windowHeight = (int)(800 * scale);

        RECT rc;
        GetWindowRect(g_hwndLyrics, &rc);
        int cx = rc.left + (rc.right - rc.left) / 2;
        int cy = rc.top + (rc.bottom - rc.top) / 2;

        SetWindowPos(g_hwndLyrics, NULL, cx - g_windowWidth / 2, cy - g_windowHeight / 2, g_windowWidth, g_windowHeight, SWP_NOZORDER);
        InvalidateRect(g_hwndLyrics, NULL, TRUE);
        SaveSettings(g_hwndLyrics);
    }
}

void ParseLyrics(const pfc::string8& lyrics) {
    g_Lyrics.clear();
    g_currentLineIndex = -1;
    g_playbackTime = 0.0;
    g_lastTick = GetTickCount();

    std::string str = lyrics.get_ptr();
    size_t pos = 0;

    while (pos < str.length()) {
        size_t lineEnd = str.find('\n', pos);
        if (lineEnd == std::string::npos) lineEnd = str.length();
        std::string line = str.substr(pos, lineEnd - pos);
        pos = lineEnd + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        size_t bracketPos = line.find('[');
        if (bracketPos == std::string::npos) continue;
        line = line.substr(bracketPos);

        if (line.length() > 2 && line[0] == '[') {
            size_t closeBracket = line.find(']');
            if (closeBracket != std::string::npos) {
                std::string timeStr = line.substr(1, closeBracket - 1);
                std::string text = line.substr(closeBracket + 1);

                int min = 0, sec = 0, ms = 0;
                int msDigits = 2;
                if (sscanf_s(timeStr.c_str(), "%d:%d.%d", &min, &sec, &ms) >= 2) {
                    size_t dot = timeStr.find('.');
                    if (dot != std::string::npos) {
                        msDigits = (int)(timeStr.length() - dot - 1);
                    }
                    double msFactor = (msDigits == 3) ? 1000.0 : 100.0;
                    double totalSeconds = min * 60.0 + sec + (ms / msFactor);

                    pfc::stringcvt::string_wide_from_utf8 wideText(text.c_str());
                    std::wstring wText = wideText.get_ptr();

                    size_t first = wText.find_first_not_of(L" ");
                    if (first == std::wstring::npos) continue;
                    size_t last = wText.find_last_not_of(L" ");
                    wText = wText.substr(first, (last - first + 1));
                    if (wText.empty()) continue;

                    bool hasMongol = false;
                    for (wchar_t c : wText) {
                        if (c >= 0x1800 && c <= 0x18AF) { hasMongol = true; break; }
                    }

                    if (!g_Lyrics.empty() && abs(g_Lyrics.back().time - totalSeconds) < 0.2) {
                        if (hasMongol && g_Lyrics.back().mongolText.empty()) {
                            g_Lyrics.back().mongolText = wText;
                        }
                        else if (!hasMongol && g_Lyrics.back().chineseText.empty()) {
                            g_Lyrics.back().chineseText = wText;
                        }
                        else {
                            LyricLine newLine;
                            newLine.time = totalSeconds;
                            if (hasMongol) newLine.mongolText = wText;
                            else newLine.chineseText = wText;
                            g_Lyrics.push_back(newLine);
                        }
                    }
                    else {
                        LyricLine newLine;
                        newLine.time = totalSeconds;
                        if (hasMongol) newLine.mongolText = wText;
                        else newLine.chineseText = wText;
                        g_Lyrics.push_back(newLine);
                    }
                }
            }
        }
    }
}

void FetchLyricsNow(metadb_handle_ptr item) {
    pfc::string8 lyrics = "";
    if (item.is_valid()) {
        metadb_info_container::ptr info_ref = item->get_info_ref();
        if (info_ref.is_valid()) {
            const file_info& info = info_ref->info();
            t_size count = info.meta_get_count();
            for (t_size i = 0; i < count; i++) {
                const char* name = info.meta_enum_name(i);
                if (name && (_stricmp(name, "LYRICS") == 0 || _stricmp(name, "UNSYNCEDLYRICS") == 0 || _stricmp(name, "LYRIC") == 0 || _stricmp(name, "UNSYNCED LYRICS") == 0)) {
                    if (info.meta_enum_value_count(i) > 0) {
                        lyrics = info.meta_enum_value(i, 0);
                        break;
                    }
                }
            }
        }

        if (lyrics.is_empty()) {
            pfc::string8 filepath = item->get_path();
            pfc::stringcvt::string_wide_from_utf8 widePath(filepath);
            std::wstring wpath = widePath.get_ptr();

            if (wpath.find(L"file://") == 0) {
                wpath = wpath.substr(7);
                if (!wpath.empty() && wpath[0] == L'/') wpath = wpath.substr(1);
            }
            for (wchar_t& c : wpath) {
                if (c == L'/') c = L'\\';
            }

            size_t dotPos = wpath.find_last_of(L'.');
            if (dotPos != std::wstring::npos) {
                std::wstring lrcPath = wpath.substr(0, dotPos) + L".lrc";
                std::wstring LRCpath = wpath.substr(0, dotPos) + L".LRC";

                HANDLE hFile = CreateFileW(lrcPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hFile == INVALID_HANDLE_VALUE) {
                    hFile = CreateFileW(LRCpath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                    if (hFile != INVALID_HANDLE_VALUE) lrcPath = LRCpath;
                }

                if (hFile != INVALID_HANDLE_VALUE) {
                    DWORD fileSize = GetFileSize(hFile, NULL);
                    if (fileSize > 0 && fileSize < 10 * 1024 * 1024) {
                        char* buffer = new char[fileSize + 1];
                        DWORD bytesRead = 0;
                        if (ReadFile(hFile, buffer, fileSize, &bytesRead, NULL)) {
                            buffer[fileSize] = '\0';
                            if (fileSize >= 3 && (unsigned char)buffer[0] == 0xEF && (unsigned char)buffer[1] == 0xBB && (unsigned char)buffer[2] == 0xBF) {
                                lyrics = buffer + 3;
                            }
                            else {
                                lyrics = buffer;
                            }
                        }
                        delete[] buffer;
                    }
                    CloseHandle(hFile);
                }
            }
        }
    }

    EnterCriticalSection(&g_csLyrics);
    if (!lyrics.is_empty()) {
        ParseLyrics(lyrics);
        if (g_Lyrics.empty()) {
            g_currentMongolText = L"解析失败";
            g_currentChineseText = L"";
        }
        else {
            g_currentMongolText = L"准备就绪";
            g_currentChineseText = L"";
        }
    }
    else {
        g_Lyrics.clear();
        g_currentMongolText = L"没找到歌词";
        g_currentChineseText = L"";
    }
    LeaveCriticalSection(&g_csLyrics);
}

class myPlayCallback : public play_callback_static {
public:
    unsigned get_flags() override {
        return flag_on_playback_new_track | flag_on_playback_stop | flag_on_playback_time | flag_on_playback_seek | flag_on_playback_pause | flag_on_playback_starting;
    }
    void on_playback_starting(playback_control::t_track_command p_command, bool p_paused) override {
        EnterCriticalSection(&g_csLyrics);
        g_isPlaying = !p_paused;
        g_lastTick = GetTickCount();
        LeaveCriticalSection(&g_csLyrics);
    }
    void on_playback_new_track(metadb_handle_ptr p_track) override {
        FetchLyricsNow(p_track);
        EnterCriticalSection(&g_csLyrics);
        g_playbackTime = 0.0;
        g_lastTick = GetTickCount();
        LeaveCriticalSection(&g_csLyrics);
        if (g_hwndLyrics) InvalidateRect(g_hwndLyrics, NULL, TRUE);
    }
    void on_playback_time(double p_time) override {
        EnterCriticalSection(&g_csLyrics);
        g_playbackTime = p_time;
        g_lastTick = GetTickCount();
        LeaveCriticalSection(&g_csLyrics);
    }
    void on_playback_seek(double p_time) override {
        EnterCriticalSection(&g_csLyrics);
        g_playbackTime = p_time;
        g_lastTick = GetTickCount();
        LeaveCriticalSection(&g_csLyrics);
    }
    void on_playback_pause(bool p_state) override {
        EnterCriticalSection(&g_csLyrics);
        g_isPlaying = !p_state;
        g_lastTick = GetTickCount();
        LeaveCriticalSection(&g_csLyrics);
    }
    void on_playback_stop(playback_control::t_stop_reason p_reason) override {
        EnterCriticalSection(&g_csLyrics);
        g_Lyrics.clear();
        g_currentMongolText = L"播放已停止";
        g_currentChineseText = L"";
        g_currentLineIndex = -1;
        g_isPlaying = false;
        LeaveCriticalSection(&g_csLyrics);
        if (g_hwndLyrics) InvalidateRect(g_hwndLyrics, NULL, TRUE);
    }
    void on_playback_edited(metadb_handle_ptr p_track) override {}
    void on_playback_dynamic_info(const file_info& p_info) override {}
    void on_playback_dynamic_info_track(const file_info& p_info) override {}
    void on_volume_change(float p_new_val) override {}
};
static play_callback_static_factory_t<myPlayCallback> g_play_callback_factory;

LRESULT CALLBACK LyricWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SETCURSOR:
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        return TRUE;
    case WM_CREATE:
        LoadSettings(hwnd);
        InitDWrite();
        ShowWindow(hwnd, g_isVisible ? SW_SHOW : SW_HIDE);
        SetTimer(hwnd, 1, 30, NULL);
        return 0;
    case WM_MOUSEWHEEL:
    {
        if (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) {
            short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            if (zDelta > 0) UpdateFontSize(1);
            else UpdateFontSize(-1);

            InvalidateRect(hwnd, NULL, TRUE);
            SaveSettings(hwnd);
            return 0;
        }
        break;
    }
    case WM_TIMER:
    {
        double currentTime = 0.0;
        EnterCriticalSection(&g_csLyrics);
        if (g_isPlaying) {
            DWORD currentTick = GetTickCount();
            DWORD deltaTick = currentTick - g_lastTick;
            currentTime = g_playbackTime + (deltaTick / 1000.0) + g_syncOffset;
        }
        else {
            currentTime = g_playbackTime;
        }

        bool needsRepaint = false;
        if (!g_Lyrics.empty()) {
            int newIndex = -1;
            for (int i = 0; i < (int)g_Lyrics.size(); i++) {
                if (currentTime >= g_Lyrics[i].time) newIndex = i;
                else break;
            }

            if (newIndex != g_currentLineIndex) {
                g_currentLineIndex = newIndex;
                if (newIndex >= 0) {
                    g_currentMongolText = g_Lyrics[newIndex].mongolText;
                    g_currentChineseText = g_Lyrics[newIndex].chineseText;
                }
                else {
                    g_currentMongolText = L"...";
                    g_currentChineseText = L"";
                }
                needsRepaint = true;
            }
        }
        LeaveCriticalSection(&g_csLyrics);
        if (needsRepaint) InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
    case WM_EXITSIZEMOVE:
        SaveSettings(hwnd);
        return 0;
    case WM_MOUSEMOVE:
    {
        int mouseX = GET_X_LPARAM(lParam);
        int mouseY = GET_Y_LPARAM(lParam);

        int newHovered = -1;
        if (mouseY >= 10 && mouseY <= 50) {
            RECT rcWin;
            GetClientRect(hwnd, &rcWin);
            int width = rcWin.right;
            float btnW = 40.0f;
            float startX = (width - btnW * 4) / 2.0f;

            if (mouseX >= startX && mouseX < startX + btnW * 4) {
                newHovered = (int)((mouseX - startX) / btnW);
            }
        }

        if (newHovered != g_hoveredButton) {
            g_hoveredButton = newHovered;
            InvalidateRect(hwnd, NULL, TRUE);
        }

        if (!g_isHovered) {
            g_isHovered = true;
            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT) };
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        g_isHovered = false;
        g_hoveredButton = -1;
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    case WM_LBUTTONDOWN:
    {
        if (g_isHovered && g_hoveredButton != -1) {
            if (g_hoveredButton == 0) {
                UpdateFontSize(-1); InvalidateRect(hwnd, NULL, TRUE); SaveSettings(hwnd);
            }
            else if (g_hoveredButton == 1) {
                UpdateFontSize(1); InvalidateRect(hwnd, NULL, TRUE); SaveSettings(hwnd);
            }
            else if (g_hoveredButton == 2) {
                fb2k::inMainThread([] {
                    static_api_ptr_t<ui_control>()->show_preferences(guid_preferences_page);
                    });
            }
            else if (g_hoveredButton == 3) {
                g_isVisible = false; ShowWindow(hwnd, SW_HIDE); SaveSettings(hwnd);
            }
            return 0;
        }

        if (!g_isLocked) { ReleaseCapture(); SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0); }
        return 0;
    }
    case WM_CONTEXTMENU:
    {
        if (g_isLocked) return 0;
        HMENU hMenu = CreatePopupMenu();
        AppendMenuW(hMenu, MF_STRING, 1, L"\u5173\u95ED\u684C\u9762\u6B4C\u8BCD");
        AppendMenuW(hMenu, MF_STRING | (g_isLocked ? MF_CHECKED : MF_UNCHECKED), 2, L"\u9501\u5B9A\u684C\u9762\u6B4C\u8BCD");
        AppendMenuW(hMenu, MF_STRING | (g_isTopmost ? MF_CHECKED : MF_UNCHECKED), 3, L"\u7F6E\u9876\u6B4C\u8BCD\u7A97\u53E3");

        POINT pt; GetCursorPos(&pt);
        int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
        DestroyMenu(hMenu);

        if (cmd == 1) { g_isVisible = false; ShowWindow(hwnd, SW_HIDE); SaveSettings(hwnd); }
        if (cmd == 2) {
            g_isLocked = true;
            SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLongPtr(hwnd, GWL_EXSTYLE) | WS_EX_TRANSPARENT);
            SaveSettings(hwnd);
        }
        if (cmd == 3) {
            g_isTopmost = !g_isTopmost;
            SetWindowPos(hwnd, g_isTopmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            SaveSettings(hwnd);
        }
        return 0;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdcScreen = BeginPaint(hwnd, &ps);

        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        RECT rcWin;
        GetClientRect(hwnd, &rcWin);
        int width = rcWin.right;
        int height = rcWin.bottom;

        BITMAPINFO bmi = { 0 };
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* pBits = nullptr;
        HBITMAP hBitmap = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

        RECT rc = { 0, 0, width, height };
        g_pDCRenderTarget->BindDC(hdcMem, &rc);

        g_pDCRenderTarget->BeginDraw();
        g_pDCRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));

        g_pMongolBrush->SetColor(g_mongolColor);
        g_pChineseBrush->SetColor(g_chineseColor);
        g_pMongolOutlineBrush->SetColor(g_mongolOutlineColor);
        g_pChineseOutlineBrush->SetColor(g_chineseOutlineColor);

        if (g_isHovered) {
            D2D1_RECT_F bgRect = D2D1::RectF(10.0f, 10.0f, (float)width - 10.0f, (float)height - 10.0f);
            g_pDCRenderTarget->FillRectangle(bgRect, g_pHoverBrush);
        }

        std::wstring displayMongolian, displayChinese;
        EnterCriticalSection(&g_csLyrics);
        if (g_displayMode == 0) {
            displayMongolian = g_currentMongolText;
            displayChinese = g_currentChineseText;
        }
        else if (g_displayMode == 1) {
            displayMongolian = g_currentMongolText;
            displayChinese = L"";
        }
        else if (g_displayMode == 2) {
            displayMongolian = L"";
            displayChinese = g_currentChineseText;
        }
        LeaveCriticalSection(&g_csLyrics);

        int steps = 16;
        float angleStep = 2.0f * 3.14159265f / steps;
        g_pDCRenderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

        if (!displayMongolian.empty() || !displayChinese.empty()) {
            std::wstring fullText = displayMongolian;
            if (!displayMongolian.empty() && !displayChinese.empty()) {
                fullText += L"\n";
            }

            std::vector<DWRITE_TEXT_RANGE> spaceRanges;
            if (!displayChinese.empty()) {
                for (size_t i = 0; i < displayChinese.length(); ++i) {
                    fullText += displayChinese[i];
                    if (i < displayChinese.length() - 1) {
                        spaceRanges.push_back({ (UINT32)fullText.length(), 1 });
                        fullText += L" ";
                    }
                }
            }

            IDWriteTextLayout* pLayout = nullptr;
            g_pDWriteFactory->CreateTextLayout(
                fullText.c_str(), (UINT32)fullText.length(),
                g_pMongolFormat, (float)width - 20.0f, 5000.0f, &pLayout
            );

            if (!displayChinese.empty()) {
                UINT32 mongLength = (UINT32)displayMongolian.length();
                UINT32 chStart = (mongLength > 0) ? mongLength + 1 : 0;
                UINT32 chLength = (UINT32)fullText.length() - chStart;

                DWRITE_TEXT_RANGE chRange = { chStart, chLength };

                pLayout->SetFontFamilyName(g_chineseFont.c_str(), chRange);
                pLayout->SetFontSize((float)(g_fontSize - 5), chRange);

                for (const auto& range : spaceRanges) {
                    pLayout->SetFontSize(7.0f, range); // 【修改】从 5.0f 调整到 7.0f，字间距再次微调拉大
                }
            }

            DWRITE_TEXT_METRICS metrics;
            pLayout->GetMetrics(&metrics);

            float startX = (width - metrics.width) / 2.0f;
            if (startX < 10.0f) startX = 10.0f;

            D2D1_POINT_2F origin = D2D1::Point2F(startX, 90.0f);

            if (!displayMongolian.empty()) {
                DWRITE_TEXT_RANGE mongRange = { 0, (UINT32)displayMongolian.length() };
                pLayout->SetDrawingEffect(g_pMongolOutlineBrush, mongRange);

                for (int i = 0; i < steps; ++i) {
                    float angle = i * angleStep;
                    D2D1_POINT_2F outlineOrigin = D2D1::Point2F(
                        origin.x + cosf(angle) * g_mongolOutlineSize,
                        origin.y + sinf(angle) * g_mongolOutlineSize
                    );
                    g_pDCRenderTarget->DrawTextLayout(outlineOrigin, pLayout, g_pMongolOutlineBrush);
                }
            }

            if (!displayChinese.empty()) {
                UINT32 mongLength = (UINT32)displayMongolian.length();
                UINT32 chStart = (mongLength > 0) ? mongLength + 1 : 0;
                UINT32 chLength = (UINT32)fullText.length() - chStart;
                DWRITE_TEXT_RANGE chRange = { chStart, chLength };

                pLayout->SetDrawingEffect(g_pChineseOutlineBrush, chRange);

                for (int i = 0; i < steps; ++i) {
                    float angle = i * angleStep;
                    D2D1_POINT_2F outlineOrigin = D2D1::Point2F(
                        origin.x + cosf(angle) * g_chineseOutlineSize,
                        origin.y + sinf(angle) * g_chineseOutlineSize
                    );
                    g_pDCRenderTarget->DrawTextLayout(outlineOrigin, pLayout, g_pChineseOutlineBrush);
                }
            }

            if (!displayMongolian.empty()) {
                DWRITE_TEXT_RANGE mongRange = { 0, (UINT32)displayMongolian.length() };
                pLayout->SetDrawingEffect(g_pMongolBrush, mongRange);
            }
            if (!displayChinese.empty()) {
                UINT32 mongLength = (UINT32)displayMongolian.length();
                UINT32 chStart = (mongLength > 0) ? mongLength + 1 : 0;
                UINT32 chLength = (UINT32)fullText.length() - chStart;
                DWRITE_TEXT_RANGE chRange = { chStart, chLength };
                pLayout->SetDrawingEffect(g_pChineseBrush, chRange);
            }

            g_pDCRenderTarget->DrawTextLayout(origin, pLayout, g_pMongolBrush);
            pLayout->Release();
        }

        if (g_isHovered && g_pUIFontFormat) {
            float btnW = 40.0f;
            float startX = (width - btnW * 4) / 2.0f;
            float startY = 10.0f;

            auto DrawBtn = [&](const wchar_t* text, int index, float fontSize) {
                bool isHover = (g_hoveredButton == index);
                ID2D1SolidColorBrush* brush = isHover ? g_pUIHoverBrush : g_pUIBrush;

                D2D1_RECT_F rect = D2D1::RectF(startX + index * btnW, startY, startX + (index + 1) * btnW, startY + 40.0f);

                IDWriteTextLayout* pBtnLayout = nullptr;
                g_pDWriteFactory->CreateTextLayout(text, (UINT32)wcslen(text), g_pUIFontFormat, btnW, 40.0f, &pBtnLayout);
                DWRITE_TEXT_RANGE range = { 0, (UINT32)wcslen(text) };
                pBtnLayout->SetFontSize(fontSize, range);
                g_pDCRenderTarget->DrawTextLayout(D2D1::Point2F(rect.left, rect.top), pBtnLayout, brush);
                pBtnLayout->Release();
                };

            DrawBtn(L"－", 0, 30.0f);
            DrawBtn(L"＋", 1, 30.0f);
            DrawBtn(L"⚙", 2, 22.0f);
            DrawBtn(L"✕", 3, 22.0f);
        }

        g_pDCRenderTarget->EndDraw();

        POINT ptSrc = { 0, 0 };
        POINT ptDst = { 0, 0 };
        RECT winRect; GetWindowRect(hwnd, &winRect);
        ptDst.x = winRect.left; ptDst.y = winRect.top;

        SIZE sizeWnd = { width, height };
        BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

        UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &sizeWnd, hdcMem, &ptSrc, 0, &bf, ULW_ALPHA);

        SelectObject(hdcMem, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        SaveSettings(hwnd);
        if (g_pMongolBrush) g_pMongolBrush->Release();
        if (g_pChineseBrush) g_pChineseBrush->Release();
        if (g_pMongolOutlineBrush) g_pMongolOutlineBrush->Release();
        if (g_pChineseOutlineBrush) g_pChineseOutlineBrush->Release();
        if (g_pHoverBrush) g_pHoverBrush->Release();
        if (g_pUIBrush) g_pUIBrush->Release();
        if (g_pUIHoverBrush) g_pUIHoverBrush->Release();
        if (g_pDCRenderTarget) g_pDCRenderTarget->Release();
        if (g_pMongolFormat) g_pMongolFormat->Release();
        if (g_pUIFontFormat) g_pUIFontFormat->Release();
        if (g_pDWriteFactory) g_pDWriteFactory->Release();
        if (g_pD2DFactory) g_pD2DFactory->Release();

        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

DWORD WINAPI WindowThread(LPVOID lpParam) {
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = LyricWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MongolLyricClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_LAYERED;
    if (g_isTopmost) exStyle |= WS_EX_TOPMOST;
    if (g_isLocked) exStyle |= WS_EX_TRANSPARENT;

    g_hwndLyrics = CreateWindowExW(
        exStyle,
        L"MongolLyricClass", L"MongolLyric",
        WS_POPUP | WS_VISIBLE,
        100, 100, g_windowWidth, g_windowHeight,
        NULL, NULL, wc.hInstance, NULL
    );

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

class myinitquit : public initquit {
public:
    void on_init() {
        InitializeCriticalSection(&g_csLyrics);
        CreateThread(NULL, 0, WindowThread, NULL, 0, NULL);

        metadb_handle_ptr item;
        if (playback_control::get()->get_now_playing(item)) {
            FetchLyricsNow(item);
        }
    }
    void on_quit() {
        if (g_hwndLyrics) PostMessage(g_hwndLyrics, WM_CLOSE, 0, 0);
        DeleteCriticalSection(&g_csLyrics);
    }
};

static initquit_factory_t<myinitquit> g_myinitquit_factory;