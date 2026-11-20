#include "stdafx.h"
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <uxtheme.h>
#include <SDK/playback_control.h>
#include <SDK/metadb.h>
#include <SDK/core_api.h>
#include <SDK/file_info_impl.h>
#include <SDK/coreDarkMode.h>
#include <string>
#include <vector>
#include <cmath>
#include <map>
#include <utility>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "uxtheme.lib")

#define REG_PATH L"Software\\MongolLyricEditor"
#define MAX_UNDO_STEPS 50
#define SLIDER_FIXED_WIDTH 300
#define TIME_LABEL_WIDTH 130

#define IDC_EDIT_LYRIC      1001
#define IDC_BTN_PAUSE       1002
#define IDC_BTN_INSERT      1003
#define IDC_CHK_DUAL        1005
#define IDC_BTN_APPLY       1006
#define IDC_BTN_OK          1007
#define IDC_BTN_CANCEL      1008
#define IDC_SLIDER_PROGRESS 1009
#define IDC_LBL_TIME        1010
#define IDC_BTN_GOTO_LINE   1011
#define IDC_BTN_BACK        1012
#define IDC_BTN_UNDO        1013
#define IDC_BTN_REMOVE_CUR  1014
#define IDC_BTN_REMOVE_ALL  1015

#define IDM_FILE_SAVE       4001
#define IDM_FILE_EXIT       4002
#define IDM_EDIT_DUAL       4004
#define IDM_EDIT_FIND_REPLACE 4005
#define IDM_EDIT_REMOVE_ORIG 4007
#define IDM_EDIT_REMOVE_TRANS 4008
#define IDM_FILE_SAVE_TAG   4009
#define IDM_FILE_SAVE_AS    4010
#define IDM_FONT_CHANGE     4011
#define IDM_FONT_ZOOM_IN    4012
#define IDM_FONT_ZOOM_OUT   4013
#define IDM_EDIT_SHIFT_TIME 4014

#define IDC_FR_FIND_TEXT    2001
#define IDC_FR_REPLACE_TEXT 2002
#define IDC_FR_BTN_FIND     2003
#define IDC_FR_BTN_REPLACE  2004
#define IDC_FR_BTN_REPLACE_ALL 2005
#define IDC_FR_BTN_CLOSE    2006

#define IDC_ST_EDIT     3001
#define IDC_ST_OK       3002
#define IDC_ST_CANCEL   3003

static HWND g_hEditorWnd = NULL;
static HWND g_hEditBox = NULL;
static HWND g_hChkDual = NULL;
static HWND g_hBtnPause = NULL;
static HWND g_hSliderProgress = NULL;
static HWND g_hLblTime = NULL;
static std::wstring g_currentTrackPath;
static bool g_isDualMode = false;
static bool g_isSliderDragging = false;
static bool g_sourceIsEmbedded = false;
static bool g_isModified = false;
static HWND g_hFindReplaceDlg = NULL;
static HWND g_hFrFindEdit = NULL;
static HWND g_hFrReplaceEdit = NULL;
static HWND g_hShiftTimeDlg = NULL;
static HWND g_hStEdit = NULL;

static fb2k::CCoreDarkModeHooks* g_pEditorHooks = nullptr;
static fb2k::CCoreDarkModeHooks* g_pFrHooks = nullptr;
static fb2k::CCoreDarkModeHooks* g_pShiftHooks = nullptr;

static int g_editorFontSize = 26;
static HFONT g_hCurrentFont = NULL;
static HFONT g_hButtonFont = NULL;
static LOGFONTW g_logFont = { 0 };
static RECT g_windowRect = { 0, 0, 900, 850 };
static bool g_hasSavedPos = false;
static HBRUSH g_hDarkBrush = NULL;
static HBRUSH g_hLightBrush = NULL;

struct UndoState {
    std::wstring text;
    int caretPos = 0;
    int firstVisibleLine = 0;
};
static std::vector<UndoState> g_undoStack;
static std::vector<UndoState> g_redoStack;

WNDPROC g_OldEditProc = NULL;
WNDPROC g_OldSliderProc = NULL;

inline bool IsDarkMode() {
    return (g_pEditorHooks != nullptr) && (bool)(*g_pEditorHooks);
}

// ★★★ 修复：使用 REG_BINARY 完整保存 LOGFONTW，修复键名空格错误 ★★★
void SaveSettings() {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_PATH, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"FontSize", 0, REG_DWORD, (BYTE*)&g_editorFontSize, sizeof(g_editorFontSize));
        // 完整保存字体结构体（包含 CharSet, PitchAndFamily 等关键属性）
        RegSetValueExW(hKey, L"LogFontData", 0, REG_BINARY, (const BYTE*)&g_logFont, sizeof(LOGFONTW));

        RegSetValueExW(hKey, L"WinLeft", 0, REG_DWORD, (BYTE*)&g_windowRect.left, sizeof(LONG));
        RegSetValueExW(hKey, L"WinTop", 0, REG_DWORD, (BYTE*)&g_windowRect.top, sizeof(LONG));
        RegSetValueExW(hKey, L"WinRight", 0, REG_DWORD, (BYTE*)&g_windowRect.right, sizeof(LONG));
        RegSetValueExW(hKey, L"WinBottom", 0, REG_DWORD, (BYTE*)&g_windowRect.bottom, sizeof(LONG));
        RegCloseKey(hKey);
    }
}

void LoadSettings() {
    HKEY hKey; g_hasSavedPos = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_PATH, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(g_editorFontSize);
        RegQueryValueExW(hKey, L"FontSize", 0, NULL, (BYTE*)&g_editorFontSize, &size);

        // 读取完整字体结构体
        size = sizeof(LOGFONTW);
        if (RegQueryValueExW(hKey, L"LogFontData", 0, NULL, (BYTE*)&g_logFont, &size) != ERROR_SUCCESS) {
            // 首次运行或读取失败，初始化默认字体
            memset(&g_logFont, 0, sizeof(LOGFONTW));
            g_logFont.lfHeight = -g_editorFontSize;
            g_logFont.lfWeight = FW_NORMAL;
            g_logFont.lfCharSet = DEFAULT_CHARSET;
            wcscpy_s(g_logFont.lfFaceName, L"Microsoft YaHei");
        }

        size = sizeof(LONG);
        RegQueryValueExW(hKey, L"WinLeft", 0, NULL, (BYTE*)&g_windowRect.left, &size);
        RegQueryValueExW(hKey, L"WinTop", 0, NULL, (BYTE*)&g_windowRect.top, &size);
        RegQueryValueExW(hKey, L"WinRight", 0, NULL, (BYTE*)&g_windowRect.right, &size);
        RegQueryValueExW(hKey, L"WinBottom", 0, NULL, (BYTE*)&g_windowRect.bottom, &size);

        g_hasSavedPos = true;
        RegCloseKey(hKey);
    }
    else {
        // 注册表不存在，初始化默认
        memset(&g_logFont, 0, sizeof(LOGFONTW));
        g_logFont.lfHeight = -g_editorFontSize;
        g_logFont.lfWeight = FW_NORMAL;
        g_logFont.lfCharSet = DEFAULT_CHARSET;
        wcscpy_s(g_logFont.lfFaceName, L"Microsoft YaHei");
    }
}

std::wstring FormatTime(double seconds) {
    if (seconds < 0) seconds = 0;
    int totalSec = (int)seconds; int min = totalSec / 60; int sec = totalSec % 60;
    int ms = (int)((seconds - totalSec) * 100);
    wchar_t buf[20]; swprintf_s(buf, L"%02d:%02d.%02d", min, sec, ms); return std::wstring(buf);
}

double GetCurrentPlaybackTime() { double t = playback_control::get()->playback_get_position(); return t < 0 ? 0 : t; }
double GetTrackLength() { double len = 0; metadb_handle_ptr item; if (playback_control::get()->get_now_playing(item)) len = item->get_length(); return len < 0 ? 0 : len; }

std::wstring GetLineText(int lineIndex) {
    int lineLen = (int)SendMessage(g_hEditBox, EM_LINELENGTH, (WPARAM)lineIndex, 0);
    if (lineLen == 0) return L"";
    std::vector<wchar_t> buffer(lineLen + 2); buffer[0] = (wchar_t)lineLen; buffer[1] = 0;
    SendMessage(g_hEditBox, EM_GETLINE, (WPARAM)lineIndex, (LPARAM)buffer.data());
    return std::wstring(buffer.data(), lineLen);
}

int GetCurrentLineIndex() {
    DWORD sel = (DWORD)SendMessage(g_hEditBox, EM_GETSEL, 0, 0);
    int caret = HIWORD(sel);
    return (int)SendMessage(g_hEditBox, EM_LINEFROMCHAR, caret, 0);
}

double ParseTimestamp(const std::wstring& timeStr) {
    int min = 0, sec = 0, ms = 0;
    if (swscanf_s(timeStr.c_str(), L"%d:%d.%d", &min, &sec, &ms) >= 2) {
        double msFactor = 100.0; size_t dot = timeStr.find(L'.');
        if (dot != std::wstring::npos) { size_t digits = timeStr.length() - dot - 1; if (digits == 3) msFactor = 1000.0; }
        return min * 60.0 + sec + (ms / msFactor);
    }
    return -1.0;
}

void UpdateProgressDisplay() {
    if (!g_hSliderProgress || !g_hLblTime) return;
    if (g_isSliderDragging) return;
    double currentTime = GetCurrentPlaybackTime(); double trackLength = GetTrackLength();
    if (trackLength > 0) { int pos = (int)((currentTime / trackLength) * 10000); if (pos > 10000) pos = 10000; SendMessage(g_hSliderProgress, TBM_SETPOS, TRUE, pos); }
    else SendMessage(g_hSliderProgress, TBM_SETPOS, TRUE, 0);
    SetWindowTextW(g_hLblTime, (FormatTime(currentTime) + L" / " + FormatTime(trackLength)).c_str());
}

void SetEditTextNoFlicker(const std::wstring& text, int caretPos = -1, int firstVisibleLine = -1) {
    if (!g_hEditBox) return;
    SendMessage(g_hEditBox, WM_SETREDRAW, FALSE, 0);
    SendMessage(g_hEditBox, EM_SETSEL, 0, -1);
    SendMessage(g_hEditBox, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
    if (caretPos >= 0) { if (caretPos > (int)text.length()) caretPos = (int)text.length(); SendMessage(g_hEditBox, EM_SETSEL, caretPos, caretPos); }
    SendMessage(g_hEditBox, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_hEditBox, NULL, FALSE);
    UpdateWindow(g_hEditBox);
    if (firstVisibleLine >= 0) { int curFirst = (int)SendMessage(g_hEditBox, EM_GETFIRSTVISIBLELINE, 0, 0); SendMessage(g_hEditBox, EM_LINESCROLL, 0, firstVisibleLine - curFirst); }
}

std::wstring GetFullText() {
    int len = GetWindowTextLengthW(g_hEditBox);
    std::wstring text(len + 1, L'\0'); GetWindowTextW(g_hEditBox, &text[0], len + 1); text.resize(len); return text;
}

void PushUndoState() {
    UndoState state; state.text = GetFullText();
    DWORD sel = (DWORD)SendMessage(g_hEditBox, EM_GETSEL, 0, 0); state.caretPos = HIWORD(sel);
    state.firstVisibleLine = (int)SendMessage(g_hEditBox, EM_GETFIRSTVISIBLELINE, 0, 0);
    g_undoStack.push_back(state);
    if (g_undoStack.size() > MAX_UNDO_STEPS) g_undoStack.erase(g_undoStack.begin());
    g_redoStack.clear();
}

void DoUndo() {
    if (g_undoStack.empty()) return;
    UndoState cur; cur.text = GetFullText();
    DWORD sel = (DWORD)SendMessage(g_hEditBox, EM_GETSEL, 0, 0); cur.caretPos = HIWORD(sel);
    cur.firstVisibleLine = (int)SendMessage(g_hEditBox, EM_GETFIRSTVISIBLELINE, 0, 0);
    g_redoStack.push_back(cur);
    UndoState prev = g_undoStack.back(); g_undoStack.pop_back();
    SetEditTextNoFlicker(prev.text, prev.caretPos, prev.firstVisibleLine);
}

void DoRedo() {
    if (g_redoStack.empty()) return;
    UndoState cur; cur.text = GetFullText();
    DWORD sel = (DWORD)SendMessage(g_hEditBox, EM_GETSEL, 0, 0); cur.caretPos = HIWORD(sel);
    cur.firstVisibleLine = (int)SendMessage(g_hEditBox, EM_GETFIRSTVISIBLELINE, 0, 0);
    g_undoStack.push_back(cur);
    UndoState next = g_redoStack.back(); g_redoStack.pop_back();
    SetEditTextNoFlicker(next.text, next.caretPos, next.firstVisibleLine);
}

void GotoCurrentLine() {
    int curLine = GetCurrentLineIndex(); std::wstring lineText = GetLineText(curLine);
    size_t start = lineText.find(L'['); size_t end = lineText.find(L']');
    if (start != std::wstring::npos && end != std::wstring::npos && end > start) {
        std::wstring timeStr = lineText.substr(start + 1, end - start - 1);
        double time = ParseTimestamp(timeStr); if (time >= 0) playback_control::get()->playback_seek(time);
    }
}

// ★★★ 滚动优化：基于像素计算，只有不足4行时才微调，消除跳跃感 ★★★
void ScrollToMakeVisible(int lineIndex) {
    if (!g_hEditBox || lineIndex < 0) return;
    RECT rc; SendMessage(g_hEditBox, EM_GETRECT, 0, (LPARAM)&rc);
    int bottomY = rc.bottom;

    int firstVisible = (int)SendMessage(g_hEditBox, EM_GETFIRSTVISIBLELINE, 0, 0);
    int charIdx1 = (int)SendMessage(g_hEditBox, EM_LINEINDEX, firstVisible, 0);
    int charIdx2 = (int)SendMessage(g_hEditBox, EM_LINEINDEX, firstVisible + 1, 0);
    int lineHeight = g_editorFontSize + 4;
    if (charIdx1 >= 0 && charIdx2 >= 0) {
        LPARAM pos1 = SendMessage(g_hEditBox, EM_POSFROMCHAR, charIdx1, 0);
        LPARAM pos2 = SendMessage(g_hEditBox, EM_POSFROMCHAR, charIdx2, 0);
        int y1 = HIWORD(pos1); int y2 = HIWORD(pos2);
        if (y2 > y1) lineHeight = y2 - y1;
    }
    if (lineHeight <= 0) lineHeight = 20;

    int charIdx = (int)SendMessage(g_hEditBox, EM_LINEINDEX, lineIndex, 0);
    if (charIdx < 0) return;
    LPARAM pos = SendMessage(g_hEditBox, EM_POSFROMCHAR, charIdx, 0);
    int lineY = HIWORD(pos);

    int spaceBelow = bottomY - (lineY + lineHeight);
    int linesBelow = spaceBelow / lineHeight;

    // 只有当下方不足 4 行时，才向上滚动补足
    if (linesBelow < 4) {
        int scrollLines = 4 - linesBelow;
        SendMessage(g_hEditBox, EM_LINESCROLL, 0, scrollLines);
    }
    // 如果目标行在视图上方（看不到了），则向下滚动
    else if (lineY < rc.top) {
        SendMessage(g_hEditBox, EM_LINESCROLL, 0, -(firstVisible - lineIndex));
    }
}

void ScrollToRevealTop(int lineIndex) {
    if (!g_hEditBox || lineIndex < 0) return;
    RECT rc; SendMessage(g_hEditBox, EM_GETRECT, 0, (LPARAM)&rc);
    int topY = rc.top;

    int firstVisible = (int)SendMessage(g_hEditBox, EM_GETFIRSTVISIBLELINE, 0, 0);
    int charIdx1 = (int)SendMessage(g_hEditBox, EM_LINEINDEX, firstVisible, 0);
    int charIdx2 = (int)SendMessage(g_hEditBox, EM_LINEINDEX, firstVisible + 1, 0);
    int lineHeight = g_editorFontSize + 4;
    if (charIdx1 >= 0 && charIdx2 >= 0) {
        LPARAM pos1 = SendMessage(g_hEditBox, EM_POSFROMCHAR, charIdx1, 0);
        LPARAM pos2 = SendMessage(g_hEditBox, EM_POSFROMCHAR, charIdx2, 0);
        int y1 = HIWORD(pos1); int y2 = HIWORD(pos2);
        if (y2 > y1) lineHeight = y2 - y1;
    }
    if (lineHeight <= 0) lineHeight = 20;

    int charIdx = (int)SendMessage(g_hEditBox, EM_LINEINDEX, lineIndex, 0);
    if (charIdx < 0) return;
    LPARAM pos = SendMessage(g_hEditBox, EM_POSFROMCHAR, charIdx, 0);
    int lineY = HIWORD(pos);

    int spaceAbove = lineY - topY;
    int linesAbove = spaceAbove / lineHeight;

    // 只有当上方不足 4 行时，才向下滚动补足
    if (linesAbove < 4) {
        int scrollLines = -(4 - linesAbove);
        SendMessage(g_hEditBox, EM_LINESCROLL, 0, scrollLines);
    }
    // 如果目标行在视图下方，则向上滚动
    else if (lineY > rc.bottom) {
        int targetFirst = lineIndex - (rc.bottom - rc.top) / lineHeight + 1;
        if (targetFirst < 0) targetFirst = 0;
        SendMessage(g_hEditBox, EM_LINESCROLL, 0, targetFirst - firstVisible);
    }
}

void CustomRedo() {
    int curLine = GetCurrentLineIndex();
    int totalLines = (int)SendMessage(g_hEditBox, EM_GETLINECOUNT, 0, 0);
    auto GetLineTime = [&](int lineIdx) -> std::wstring {
        if (lineIdx < 0 || lineIdx >= totalLines) return L"";
        std::wstring line = GetLineText(lineIdx);
        size_t bpos = line.find(L'['); size_t epos = line.find(L']');
        if (bpos != std::wstring::npos && bpos < 10 && epos != std::wstring::npos && epos > bpos)
            return line.substr(bpos + 1, epos - bpos - 1);
        return L"";
        };
    std::wstring curTime = GetLineTime(curLine);
    if (curTime.empty()) {
        int target = curLine - 1;
        while (target >= 0 && GetLineTime(target).empty()) target--;
        if (target < 0) return;
        curTime = GetLineTime(target);
        curLine = target;
    }
    int groupStart = curLine;
    int groupEnd = curLine;
    if (g_isDualMode) {
        while (groupStart > 0 && GetLineTime(groupStart - 1) == curTime) groupStart--;
        while (groupEnd < totalLines - 1 && GetLineTime(groupEnd + 1) == curTime) groupEnd++;
    }
    int seekLine = groupStart - 1;
    while (seekLine >= 0 && GetLineTime(seekLine).empty()) seekLine--;
    std::wstring seekTime = (seekLine >= 0) ? GetLineTime(seekLine) : L"";
    PushUndoState();
    g_isModified = true;
    for (int i = groupEnd; i >= groupStart; i--) {
        int lineStartChar = (int)SendMessage(g_hEditBox, EM_LINEINDEX, i, 0);
        std::wstring lineContent = GetLineText(i);
        size_t bpos = lineContent.find(L'['); size_t epos = lineContent.find(L']');
        if (bpos != std::wstring::npos && bpos < 10 && epos != std::wstring::npos && epos > bpos) {
            SendMessage(g_hEditBox, EM_SETSEL, lineStartChar + bpos, lineStartChar + epos + 1);
            SendMessage(g_hEditBox, EM_REPLACESEL, FALSE, (LPARAM)L"");
        }
    }
    if (!seekTime.empty()) {
        double t = ParseTimestamp(seekTime);
        if (t >= 0) playback_control::get()->playback_seek(t);
    }
    ScrollToRevealTop(groupStart);
    if (GetForegroundWindow() == g_hEditorWnd) SetFocus(g_hEditBox);
}

void RemoveCurrentTimestamp() {
    int curLine = GetCurrentLineIndex(); std::wstring text = GetFullText();
    int lineStartChar = (int)SendMessage(g_hEditBox, EM_LINEINDEX, curLine, 0);
    int lineLen = (int)SendMessage(g_hEditBox, EM_LINELENGTH, (WPARAM)curLine, 0);
    DWORD sel = (DWORD)SendMessage(g_hEditBox, EM_GETSEL, 0, 0); int oldCaret = HIWORD(sel);
    int firstVisible = (int)SendMessage(g_hEditBox, EM_GETFIRSTVISIBLELINE, 0, 0);
    if (lineStartChar >= 0 && lineLen > 0) {
        std::wstring lineText = text.substr(lineStartChar, lineLen);
        size_t bpos = lineText.find(L'['); size_t end = lineText.find(L']');
        if (bpos != std::wstring::npos && bpos < 10 && end != std::wstring::npos) {
            PushUndoState(); int delLen = (int)(end - bpos + 1); text.erase(lineStartChar + bpos, delLen);
            int newCaret = oldCaret; if (oldCaret > lineStartChar + (int)bpos) { newCaret -= delLen; if (newCaret < lineStartChar + (int)bpos) newCaret = lineStartChar + (int)bpos; }
            SetEditTextNoFlicker(text, newCaret, firstVisible);
            g_isModified = true;
            ScrollToMakeVisible(curLine);
        }
    }
}

void InsertCurrentTime() {
    if (!g_hEditBox || !playback_control::get()->is_playing()) return;
    double pos = playback_control::get()->playback_get_position(); if (pos < 0) pos = 0;
    int min = (int)(pos / 60); int sec = (int)pos % 60; int ms = (int)((pos - (int)pos) * 100);
    wchar_t timeStr[32]; swprintf_s(timeStr, L"[%02d:%02d.%02d]", min, sec, ms); // 无空格
    std::wstring ts = timeStr;
    int curLine = GetCurrentLineIndex();
    PushUndoState();
    g_isModified = true;

    auto ApplyToLine = [&](int lineIdx) {
        int startChar = (int)SendMessage(g_hEditBox, EM_LINEINDEX, lineIdx, 0);
        if (startChar < 0) return;
        std::wstring lt = GetLineText(lineIdx);
        size_t bp = lt.find(L'['); size_t ep = lt.find(L']');
        if (bp != std::wstring::npos && ep != std::wstring::npos && ep > bp && (ep - bp) < 15) {
            SendMessage(g_hEditBox, EM_SETSEL, startChar + (int)bp, startChar + (int)ep + 1);
            SendMessage(g_hEditBox, EM_REPLACESEL, FALSE, (LPARAM)ts.c_str());
        }
        else {
            SendMessage(g_hEditBox, EM_SETSEL, startChar, startChar);
            SendMessage(g_hEditBox, EM_REPLACESEL, FALSE, (LPARAM)ts.c_str());
        }
        };

    if (g_isDualMode) ApplyToLine(curLine + 1);
    ApplyToLine(curLine);

    int caretLine = curLine + (g_isDualMode ? 2 : 1);
    int caretPos = (int)SendMessage(g_hEditBox, EM_LINEINDEX, caretLine, 0);
    if (caretPos < 0) caretPos = GetWindowTextLengthW(g_hEditBox);
    SendMessage(g_hEditBox, EM_SETSEL, caretPos, caretPos);

    ScrollToMakeVisible(caretLine);
    if (GetForegroundWindow() == g_hEditorWnd) SetFocus(g_hEditBox);
}

void RemoveAllTimestamps() {
    std::wstring text = GetFullText(); std::wstring originalText = text;
    DWORD sel = (DWORD)SendMessage(g_hEditBox, EM_GETSEL, 0, 0); int oldCaret = HIWORD(sel);
    int firstVisible = (int)SendMessage(g_hEditBox, EM_GETFIRSTVISIBLELINE, 0, 0);
    int deletedBeforeCaret = 0; size_t searchStart = 0;
    while (searchStart < (size_t)oldCaret && searchStart < originalText.size()) {
        size_t bpos = originalText.find(L'[', searchStart); if (bpos == std::wstring::npos || bpos >= (size_t)oldCaret) break;
        size_t epos = originalText.find(L']', bpos);
        if (epos != std::wstring::npos) { size_t lineStart = originalText.rfind(L'\n', bpos); lineStart = (lineStart == std::wstring::npos) ? 0 : lineStart + 1; if (bpos - lineStart < 10) deletedBeforeCaret += (int)(epos - bpos + 1); }
        searchStart = bpos + 1;
    }
    std::wstring result; size_t start = 0;
    while (start <= text.size()) {
        size_t nl = text.find(L'\n', start);
        std::wstring line = (nl == std::wstring::npos) ? text.substr(start) : text.substr(start, nl - start);
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        size_t bpos = line.find(L'[');
        if (bpos != std::wstring::npos && bpos < 10) { size_t epos = line.find(L']', bpos); if (epos != std::wstring::npos) { line = line.substr(epos + 1); size_t fs = line.find_first_not_of(L" \t"); if (fs != std::wstring::npos) line = line.substr(fs); else line.clear(); } }
        result += line; if (nl == std::wstring::npos) break; result += L"\r\n"; start = nl + 1;
    }
    PushUndoState(); int newCaret = oldCaret - deletedBeforeCaret; if (newCaret < 0) newCaret = 0;
    SetEditTextNoFlicker(result, newCaret, firstVisible);
    g_isModified = true;
}

void RemoveOriginalLyrics() {
    std::wstring text = GetFullText(); std::vector<std::wstring> lines; size_t start = 0;
    while (start <= text.size()) { size_t nl = text.find(L'\n', start); std::wstring line = (nl == std::wstring::npos) ? text.substr(start) : text.substr(start, nl - start); if (!line.empty() && line.back() == L'\r') line.pop_back(); lines.push_back(line); if (nl == std::wstring::npos) break; start = nl + 1; }
    std::wstring result;
    for (size_t i = 0; i < lines.size(); i++) {
        size_t bpos = lines[i].find(L'[');
        if (bpos != std::wstring::npos && bpos < 10) {
            size_t epos = lines[i].find(L']', bpos);
            if (epos != std::wstring::npos) {
                std::wstring timeStr = lines[i].substr(bpos + 1, epos - bpos - 1);
                if (i + 1 < lines.size()) {
                    size_t bpos2 = lines[i + 1].find(L'['); size_t epos2 = lines[i + 1].find(L']', bpos2);
                    if (bpos2 != std::wstring::npos && bpos2 < 10 && epos2 != std::wstring::npos) {
                        std::wstring timeStr2 = lines[i + 1].substr(bpos2 + 1, epos2 - bpos2 - 1);
                        if (timeStr == timeStr2) { std::wstring content2 = lines[i + 1].substr(epos2 + 1); if (content2.find_first_not_of(L" \t") != std::wstring::npos) { result += lines[i + 1]; if (i + 2 < lines.size()) result += L"\r\n"; i++; continue; } }
                    }
                }
            }
        }
        size_t eposCheck = lines[i].find(L']'); std::wstring contentCheck = (eposCheck != std::wstring::npos) ? lines[i].substr(eposCheck + 1) : lines[i];
        if (contentCheck.find_first_not_of(L" \t") == std::wstring::npos && bpos != std::wstring::npos) continue;
        result += lines[i]; if (i + 1 < lines.size()) result += L"\r\n";
    }
    PushUndoState(); SetEditTextNoFlicker(result);
    g_isModified = true;
}

void RemoveTranslationLyrics() {
    std::wstring text = GetFullText(); std::vector<std::wstring> lines; size_t start = 0;
    while (start <= text.size()) { size_t nl = text.find(L'\n', start); std::wstring line = (nl == std::wstring::npos) ? text.substr(start) : text.substr(start, nl - start); if (!line.empty() && line.back() == L'\r') line.pop_back(); lines.push_back(line); if (nl == std::wstring::npos) break; start = nl + 1; }
    std::wstring result;
    for (size_t i = 0; i < lines.size(); i++) {
        size_t bpos = lines[i].find(L'[');
        if (bpos != std::wstring::npos && bpos < 10) {
            size_t epos = lines[i].find(L']', bpos);
            if (epos != std::wstring::npos) {
                std::wstring timeStr = lines[i].substr(bpos + 1, epos - bpos - 1);
                if (i + 1 < lines.size()) {
                    size_t bpos2 = lines[i + 1].find(L'['); size_t epos2 = lines[i + 1].find(L']', bpos2);
                    if (bpos2 != std::wstring::npos && bpos2 < 10 && epos2 != std::wstring::npos) {
                        std::wstring timeStr2 = lines[i + 1].substr(bpos2 + 1, epos2 - bpos2 - 1);
                        if (timeStr == timeStr2) { std::wstring content2 = lines[i + 1].substr(epos2 + 1); if (content2.find_first_not_of(L" \t") != std::wstring::npos) { result += lines[i]; if (i + 2 < lines.size()) result += L"\r\n"; i++; continue; } }
                    }
                }
            }
        }
        size_t eposCheck = lines[i].find(L']'); std::wstring contentCheck = (eposCheck != std::wstring::npos) ? lines[i].substr(eposCheck + 1) : lines[i];
        if (contentCheck.find_first_not_of(L" \t") == std::wstring::npos && bpos != std::wstring::npos) continue;
        result += lines[i]; if (i + 1 < lines.size()) result += L"\r\n";
    }
    PushUndoState(); SetEditTextNoFlicker(result);
    g_isModified = true;
}

// ★★★ 终极字体刷新：彻底解决所有字体（蒙科立/Unicode）不立即生效、不持久化的问题 ★★★
// ★★★ 修复：补回 WM_SETFONT（蒙科立等字体依赖控件基准字体的字符集），并强制重排 ★★★
void ApplyNewFont() {
    if (!g_hEditBox) return;

    // 使用 CHARFORMAT2W 以支持颜色设置（CHARFORMATW 不支持 crTextColor）
    CHARFORMAT2W cf;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(CHARFORMAT2W);
    cf.dwMask = CFM_FACE | CFM_SIZE | CFM_BOLD | CFM_COLOR; // ★ 增加 CFM_COLOR

    wcscpy_s(cf.szFaceName, g_logFont.lfFaceName[0] ? g_logFont.lfFaceName : L"Microsoft YaHei");
    cf.yHeight = g_editorFontSize * 15;
    if (g_logFont.lfWeight >= FW_BOLD) cf.dwEffects |= CFE_BOLD;

    // ★ 核心修改：根据 foobar2000 深色模式设置字体颜色
    // 深色模式：白色/浅灰色 (240,240,240)；浅色模式：黑色 (0,0,0)
    cf.crTextColor = IsDarkMode() ? RGB(240, 240, 240) : RGB(0, 0, 0);
    cf.dwEffects &= ~CFE_AUTOCOLOR; // 禁用自动颜色，强制使用 crTextColor

    // SCF_ALL 会同时应用到已有文本和默认格式（新输入的文本也会是这个颜色）
    SendMessage(g_hEditBox, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

    PARAFORMAT2 pf;
    ZeroMemory(&pf, sizeof(pf));
    pf.cbSize = sizeof(PARAFORMAT2);
    pf.dwMask = PFM_LINESPACING;
    pf.bLineSpacingRule = 0;
    SendMessage(g_hEditBox, EM_SETPARAFORMAT, 0, (LPARAM)&pf);

    // 强制立即重绘
    InvalidateRect(g_hEditBox, NULL, TRUE);
    UpdateWindow(g_hEditBox);
}

LRESULT CALLBACK SliderSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_LBUTTONDOWN) {
        int mouseX = GET_X_LPARAM(lp); RECT rcChannel; SendMessage(hwnd, TBM_GETCHANNELRECT, 0, (LPARAM)&rcChannel);
        float channelWidth = (float)(rcChannel.right - rcChannel.left); float ratio = (float)(mouseX - rcChannel.left) / channelWidth;
        if (ratio < 0.0f) ratio = 0.0f; if (ratio > 1.0f) ratio = 1.0f;
        SendMessage(hwnd, TBM_SETPOS, TRUE, (int)(ratio * 10000));
        double trackLength = GetTrackLength(); if (trackLength > 0) playback_control::get()->playback_seek(ratio * trackLength);
        g_isSliderDragging = true; SetCapture(hwnd); return 0;
    }
    else if (msg == WM_MOUSEMOVE) {
        if (g_isSliderDragging) {
            int mouseX = GET_X_LPARAM(lp); RECT rcChannel; SendMessage(hwnd, TBM_GETCHANNELRECT, 0, (LPARAM)&rcChannel);
            float channelWidth = (float)(rcChannel.right - rcChannel.left); float ratio = (float)(mouseX - rcChannel.left) / channelWidth;
            if (ratio < 0.0f) ratio = 0.0f; if (ratio > 1.0f) ratio = 1.0f;
            SendMessage(hwnd, TBM_SETPOS, TRUE, (int)(ratio * 10000));
            double trackLength = GetTrackLength(); if (trackLength > 0) SetWindowTextW(g_hLblTime, (FormatTime(ratio * trackLength) + L" / " + FormatTime(trackLength)).c_str());
        } return 0;
    }
    else if (msg == WM_LBUTTONUP) {
        if (g_isSliderDragging) { g_isSliderDragging = false; ReleaseCapture(); int pos = (int)SendMessage(hwnd, TBM_GETPOS, 0, 0); double trackLength = GetTrackLength(); if (trackLength > 0) playback_control::get()->playback_seek((pos / 10000.0) * trackLength); } return 0;
    }
    return CallWindowProc(g_OldSliderProc, hwnd, msg, wp, lp);
}

void ShowFindReplaceDialog();
void ShowShiftTimeDialog();

LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN) {
        if (wp == VK_F3) { InsertCurrentTime(); return 0; }
        if (wp == VK_F1) { GotoCurrentLine(); return 0; }
        if (wp == VK_F2) { CustomRedo(); return 0; }
        if (wp == 'F' && (GetKeyState(VK_CONTROL) & 0x8000)) { ShowFindReplaceDialog(); return 0; }
        if (wp == 'Z' && (GetKeyState(VK_CONTROL) & 0x8000)) { DoUndo(); return 0; }
        if (wp == 'Y' && (GetKeyState(VK_CONTROL) & 0x8000)) { DoRedo(); return 0; }
        if (wp == 'S' && (GetKeyState(VK_CONTROL) & 0x8000)) { SendMessage(GetParent(hwnd), WM_COMMAND, IDM_FILE_SAVE, 0); return 0; }
    }
    if (msg == WM_CHAR) {
        if (wp >= 32 || wp == VK_RETURN || wp == VK_TAB) {
            g_isModified = true;
        }
    }
    if (msg == WM_MOUSEWHEEL) {
        short zDelta = GET_WHEEL_DELTA_WPARAM(wp);
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            if (zDelta > 0) g_editorFontSize += 1; else g_editorFontSize -= 1;
            if (g_editorFontSize < 8) g_editorFontSize = 8; if (g_editorFontSize > 72) g_editorFontSize = 72; ApplyNewFont(); return 0;
        }
        else {
            int lines = (zDelta > 0) ? -2 : 2;
            SendMessage(hwnd, EM_LINESCROLL, 0, lines);
            return 0;
        }
    }
    return CallWindowProc(g_OldEditProc, hwnd, msg, wp, lp);
}

LRESULT CALLBACK FindReplaceDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        RECT rc; GetClientRect(hwnd, &rc); int margin = 15; int y = margin; int labelW = 50; int editW = rc.right - margin * 2 - labelW - 10; int btnH = 28; int btnW = 85; int btnGap = 8;
        CreateWindowW(L"STATIC", L"查找:", WS_CHILD | WS_VISIBLE, margin, y + 4, labelW, 20, hwnd, NULL, NULL, NULL);
        g_hFrFindEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, margin + labelW, y, editW, 24, hwnd, (HMENU)IDC_FR_FIND_TEXT, NULL, NULL); y += 35;
        CreateWindowW(L"STATIC", L"替换:", WS_CHILD | WS_VISIBLE, margin, y + 4, labelW, 20, hwnd, NULL, NULL, NULL);
        g_hFrReplaceEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, margin + labelW, y, editW, 24, hwnd, (HMENU)IDC_FR_REPLACE_TEXT, NULL, NULL); y += 45;
        int bx = margin;
        CreateWindowW(L"BUTTON", L"查找下一个", WS_CHILD | WS_VISIBLE, bx, y, btnW, btnH, hwnd, (HMENU)IDC_FR_BTN_FIND, NULL, NULL); bx += btnW + btnGap;
        CreateWindowW(L"BUTTON", L"替换", WS_CHILD | WS_VISIBLE, bx, y, btnW, btnH, hwnd, (HMENU)IDC_FR_BTN_REPLACE, NULL, NULL); bx += btnW + btnGap;
        CreateWindowW(L"BUTTON", L"全部替换", WS_CHILD | WS_VISIBLE, bx, y, btnW, btnH, hwnd, (HMENU)IDC_FR_BTN_REPLACE_ALL, NULL, NULL); bx += btnW + btnGap;
        CreateWindowW(L"BUTTON", L"关闭", WS_CHILD | WS_VISIBLE, bx, y, btnW, btnH, hwnd, (HMENU)IDC_FR_BTN_CLOSE, NULL, NULL);
        HFONT f = (HFONT)GetStockObject(DEFAULT_GUI_FONT); HWND c = GetWindow(hwnd, GW_CHILD); while (c) { SendMessage(c, WM_SETFONT, (WPARAM)f, TRUE); c = GetNextWindow(c, GW_HWNDNEXT); }
        if (!g_pFrHooks) { g_pFrHooks = new fb2k::CCoreDarkModeHooks(); g_pFrHooks->AddDialogWithControls(hwnd); }
        g_pFrHooks->AddCtrlAuto(g_hFrFindEdit); g_pFrHooks->AddCtrlAuto(g_hFrReplaceEdit);
        return 0;
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam; RECT rc; GetClientRect(hwnd, &rc);
        HBRUSH br = IsDarkMode() ? g_hDarkBrush : g_hLightBrush; FillRect(hdc, &rc, br); return 1;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        if (IsDarkMode()) { SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, RGB(240, 240, 240)); return (LRESULT)GetStockObject(NULL_BRUSH); }
        SetBkMode(hdc, OPAQUE); SetBkColor(hdc, GetSysColor(COLOR_WINDOW)); SetTextColor(hdc, RGB(0, 0, 0)); return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        if (wmId == IDC_FR_FIND_TEXT || wmId == IDC_FR_REPLACE_TEXT) return 0;
        if (wmId == IDC_FR_BTN_CLOSE) { ShowWindow(hwnd, SW_HIDE); return 0; }
        int fl = GetWindowTextLengthW(g_hFrFindEdit); std::wstring findText(fl + 1, L'\0'); GetWindowTextW(g_hFrFindEdit, &findText[0], fl + 1); findText.resize(fl);
        int rl = GetWindowTextLengthW(g_hFrReplaceEdit); std::wstring replaceText(rl + 1, L'\0'); GetWindowTextW(g_hFrReplaceEdit, &replaceText[0], rl + 1); replaceText.resize(rl);
        if (findText.empty()) { MessageBoxW(hwnd, L"请输入查找内容", L"提示", MB_OK); return 0; }
        if (wmId == IDC_FR_BTN_FIND) {
            std::wstring text = GetFullText(); DWORD sel = (DWORD)SendMessage(g_hEditBox, EM_GETSEL, 0, 0); int startPos = HIWORD(sel); size_t pos = text.find(findText, startPos);
            if (pos != std::wstring::npos) { SendMessage(g_hEditBox, EM_SETSEL, (WPARAM)pos, (LPARAM)(pos + findText.length())); SendMessage(g_hEditBox, EM_SCROLLCARET, 0, 0); SetFocus(g_hEditBox); }
            else { pos = text.find(findText, 0); if (pos != std::wstring::npos) { SendMessage(g_hEditBox, EM_SETSEL, (WPARAM)pos, (LPARAM)(pos + findText.length())); SendMessage(g_hEditBox, EM_SCROLLCARET, 0, 0); SetFocus(g_hEditBox); } else MessageBoxW(hwnd, (L"找不到 " + findText).c_str(), L"查找", MB_OK); }
        }
        else if (wmId == IDC_FR_BTN_REPLACE) {
            DWORD sel = (DWORD)SendMessage(g_hEditBox, EM_GETSEL, 0, 0); int start = LOWORD(sel); int end = HIWORD(sel);
            if (start != end) {
                std::wstring text2 = GetFullText(); std::wstring selText = text2.substr(start, end - start);
                if (selText == findText) { PushUndoState(); text2.replace(start, end - start, replaceText); SetEditTextNoFlicker(text2); int ns = start + (int)replaceText.length(); SendMessage(g_hEditBox, EM_SETSEL, ns, ns); SetFocus(g_hEditBox); g_isModified = true; }
                else { SendMessage(g_hEditBox, EM_SETSEL, end, end); SendMessage(hwnd, WM_COMMAND, IDC_FR_BTN_FIND, 0); }
            }
            else SendMessage(hwnd, WM_COMMAND, IDC_FR_BTN_FIND, 0);
        }
        else if (wmId == IDC_FR_BTN_REPLACE_ALL) {
            std::wstring text = GetFullText(); int count = 0; size_t pos = 0; while ((pos = text.find(findText, pos)) != std::wstring::npos) { text.replace(pos, findText.length(), replaceText); pos += replaceText.length(); count++; }
            if (count > 0) { PushUndoState(); SetEditTextNoFlicker(text); MessageBoxW(hwnd, (L"成功替换 " + std::to_wstring(count) + L" 处").c_str(), L"替换", MB_OK); SetFocus(g_hEditBox); g_isModified = true; }
            else MessageBoxW(hwnd, (L"找不到 " + findText).c_str(), L"替换", MB_OK);
        } return 0;
    }
    case WM_CLOSE: ShowWindow(hwnd, SW_HIDE); return 0;
    } return DefWindowProc(hwnd, msg, wParam, lParam);
}

void ShowFindReplaceDialog() {
    if (!g_hFindReplaceDlg) {
        WNDCLASSW wc = { 0 }; wc.lpfnWndProc = FindReplaceDlgProc; wc.hInstance = GetModuleHandle(NULL); wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); wc.lpszClassName = L"FindReplaceDlgClass";
        HWND mainWnd = core_api::get_main_window(); wc.hIcon = (HICON)SendMessage(mainWnd, WM_GETICON, ICON_BIG, 0); if (!wc.hIcon) wc.hIcon = LoadIcon(NULL, IDI_APPLICATION); RegisterClassW(&wc);
        g_hFindReplaceDlg = CreateWindowExW(WS_EX_TOPMOST | WS_CLIPCHILDREN, L"FindReplaceDlgClass", L"查找和替换", WS_CAPTION | WS_SYSMENU | WS_BORDER, 100, 100, 500, 180, g_hEditorWnd, NULL, GetModuleHandle(NULL), NULL);
    }
    RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0); RECT wr; GetWindowRect(g_hFindReplaceDlg, &wr);
    int w = wr.right - wr.left, h = wr.bottom - wr.top;
    SetWindowPos(g_hFindReplaceDlg, HWND_TOPMOST, wa.left + (wa.right - wa.left - w) / 2, wa.top + (wa.bottom - wa.top - h) / 2, 0, 0, SWP_NOSIZE);
    ShowWindow(g_hFindReplaceDlg, SW_SHOW); SetForegroundWindow(g_hFindReplaceDlg); SetFocus(g_hFrFindEdit);
}

double ParseUserTimeInput(const std::wstring& input) {
    std::wstring clean = input; size_t b = clean.find(L'['); if (b != std::wstring::npos) clean = clean.substr(b + 1); size_t e = clean.find(L']'); if (e != std::wstring::npos) clean = clean.substr(0, e);
    int min = 0, sec = 0, ms = 0, sign = 1; std::wstring ps = clean; if (!ps.empty() && ps[0] == L'-') { sign = -1; ps = ps.substr(1); }
    if (swscanf_s(ps.c_str(), L"%d:%d.%d", &min, &sec, &ms) >= 2) {
        double msFactor = 100.0; size_t dot = ps.find(L'.');
        if (dot != std::wstring::npos) { size_t digits = ps.length() - dot - 1; if (digits == 3) msFactor = 1000.0; else if (digits == 1) msFactor = 10.0; }
        return sign * (min * 60.0 + sec + (ms / msFactor));
    } return _wtof(clean.c_str());
}

std::wstring ApplyTimeShift(const std::wstring& text, double offsetSec) {
    int offsetMs = (int)(offsetSec * 1000); if (offsetMs == 0) return text;
    std::wstring result = text; size_t searchStart = 0;
    while (true) {
        size_t bpos = result.find(L'[', searchStart); if (bpos == std::wstring::npos) break;
        size_t epos = result.find(L']', bpos); if (epos == std::wstring::npos) break;
        std::wstring tag = result.substr(bpos + 1, epos - bpos - 1); int min = 0, sec = 0, ms = 0;
        if (swscanf_s(tag.c_str(), L"%d:%d.%d", &min, &sec, &ms) >= 2) {
            int actualMs = ms; size_t dotPos = tag.find(L'.');
            if (dotPos != std::wstring::npos) { int digits = (int)(tag.length() - dotPos - 1); if (digits == 2) actualMs = ms * 10; else if (digits == 1) actualMs = ms * 100; else if (digits == 3) actualMs = ms; else actualMs = ms * 10; }
            int totalMs = min * 60000 + sec * 1000 + actualMs + offsetMs; if (totalMs < 0) totalMs = 0;
            int newMin = totalMs / 60000, newSec = (totalMs % 60000) / 1000, newMs = totalMs % 1000; wchar_t buf[32];
            if (dotPos != std::wstring::npos && (tag.length() - dotPos - 1) == 3) swprintf_s(buf, L"%02d:%02d.%03d", newMin, newSec, newMs); else swprintf_s(buf, L"%02d:%02d.%02d", newMin, newSec, newMs / 10);
            std::wstring newTag = buf; size_t rp = bpos + 1, rl = epos - bpos - 1; result.replace(rp, rl, newTag); searchStart = rp + newTag.length();
        }
        else searchStart = bpos + 1;
    } return result;
}

LRESULT CALLBACK ShiftTimeDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        RECT rc; GetClientRect(hwnd, &rc); int margin = 15; int y = margin; int labelW = 100; int editW = rc.right - margin * 2 - labelW - 10; int btnH = 28; int btnW = 85; int btnGap = 8;
        CreateWindowW(L"STATIC", L"偏移秒数:", WS_CHILD | WS_VISIBLE, margin, y + 4, labelW, 20, hwnd, NULL, NULL, NULL);
        g_hStEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"00:00.00", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, margin + labelW, y, editW, 24, hwnd, (HMENU)IDC_ST_EDIT, NULL, NULL); y += 50;
        int bx = margin;
        CreateWindowW(L"BUTTON", L"确定", WS_CHILD | WS_VISIBLE, bx, y, btnW, btnH, hwnd, (HMENU)IDC_ST_OK, NULL, NULL); bx += btnW + btnGap;
        CreateWindowW(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE, bx, y, btnW, btnH, hwnd, (HMENU)IDC_ST_CANCEL, NULL, NULL);
        HFONT f = (HFONT)GetStockObject(DEFAULT_GUI_FONT); HWND c = GetWindow(hwnd, GW_CHILD); while (c) { SendMessage(c, WM_SETFONT, (WPARAM)f, TRUE); c = GetNextWindow(c, GW_HWNDNEXT); }
        if (!g_pShiftHooks) { g_pShiftHooks = new fb2k::CCoreDarkModeHooks(); g_pShiftHooks->AddDialogWithControls(hwnd); }
        g_pShiftHooks->AddCtrlAuto(g_hStEdit);
        SetFocus(g_hStEdit); SendMessage(g_hStEdit, EM_SETSEL, 0, -1); return 0;
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam; RECT rc; GetClientRect(hwnd, &rc);
        HBRUSH br = IsDarkMode() ? g_hDarkBrush : g_hLightBrush; FillRect(hdc, &rc, br); return 1;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        if (IsDarkMode()) { SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, RGB(240, 240, 240)); return (LRESULT)GetStockObject(NULL_BRUSH); }
        SetBkMode(hdc, OPAQUE); SetBkColor(hdc, GetSysColor(COLOR_WINDOW)); SetTextColor(hdc, RGB(0, 0, 0)); return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        if (wmId == IDC_ST_OK) {
            wchar_t buf[64] = { 0 }; GetWindowTextW(g_hStEdit, buf, 64); double offset = ParseUserTimeInput(buf);
            if (offset != 0.0) { PushUndoState(); std::wstring text = GetFullText(); std::wstring newText = ApplyTimeShift(text, offset); int caret = (int)SendMessage(g_hEditBox, EM_GETSEL, 0, 0) & 0xFFFF; int fv = (int)SendMessage(g_hEditBox, EM_GETFIRSTVISIBLELINE, 0, 0); SetEditTextNoFlicker(newText, caret, fv); g_isModified = true; }
            ShowWindow(hwnd, SW_HIDE); return 0;
        }
        if (wmId == IDC_ST_CANCEL) { ShowWindow(hwnd, SW_HIDE); return 0; } return 0;
    }
    case WM_CLOSE: ShowWindow(hwnd, SW_HIDE); return 0;
    } return DefWindowProc(hwnd, msg, wParam, lParam);
}

void ShowShiftTimeDialog() {
    if (!g_hShiftTimeDlg) {
        WNDCLASSW wc = { 0 }; wc.lpfnWndProc = ShiftTimeDlgProc; wc.hInstance = GetModuleHandle(NULL); wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); wc.lpszClassName = L"ShiftTimeDlgClass";
        HWND mainWnd = core_api::get_main_window(); wc.hIcon = (HICON)SendMessage(mainWnd, WM_GETICON, ICON_BIG, 0); if (!wc.hIcon) wc.hIcon = LoadIcon(NULL, IDI_APPLICATION); RegisterClassW(&wc);
        g_hShiftTimeDlg = CreateWindowExW(WS_EX_TOPMOST | WS_CLIPCHILDREN, L"ShiftTimeDlgClass", L"时间轴偏移", WS_CAPTION | WS_SYSMENU | WS_BORDER, 100, 100, 500, 160, g_hEditorWnd, NULL, GetModuleHandle(NULL), NULL);
    }
    RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0); RECT wr; GetWindowRect(g_hShiftTimeDlg, &wr);
    int w = wr.right - wr.left, h = wr.bottom - wr.top;
    SetWindowPos(g_hShiftTimeDlg, HWND_TOPMOST, wa.left + (wa.right - wa.left - w) / 2, wa.top + (wa.bottom - wa.top - h) / 2, 0, 0, SWP_NOSIZE);
    ShowWindow(g_hShiftTimeDlg, SW_SHOW); SetForegroundWindow(g_hShiftTimeDlg); SetFocus(g_hStEdit); SendMessage(g_hStEdit, EM_SETSEL, 0, -1);
}

std::wstring NormalizeLyrics(const std::wstring& input) {
    std::vector<std::wstring> lines; size_t start = 0;
    while (start <= input.size()) { size_t nl = input.find(L'\n', start); std::wstring line = (nl == std::wstring::npos) ? input.substr(start) : input.substr(start, nl - start); if (!line.empty() && line.back() == L'\r') line.pop_back(); if (!line.empty()) lines.push_back(line); if (nl == std::wstring::npos) break; start = nl + 1; }
    std::map<std::wstring, std::vector<std::wstring>> timeline; std::vector<std::wstring> headers;
    for (const auto& line : lines) { size_t bpos = line.find(L'['); size_t epos = line.find(L']'); if (bpos != std::wstring::npos && epos != std::wstring::npos && epos > bpos) { std::wstring tag = line.substr(bpos, epos - bpos + 1); std::wstring content = line.substr(epos + 1); if (tag.length() > 1 && iswdigit(tag[1])) timeline[tag].push_back(content); else headers.push_back(line); } else headers.push_back(line); }
    std::wstring result; for (const auto& h : headers) result += h + L"\r\n"; for (const auto& pair : timeline) for (const auto& content : pair.second) result += pair.first + content + L"\r\n"; return result;
}

bool SaveToLrcFile() {
    if (g_currentTrackPath.empty()) { MessageBoxW(g_hEditorWnd, L"无法获取路径", L"错误", MB_ICONERROR); return false; }
    std::wstring text = GetFullText(); text = NormalizeLyrics(text);
    pfc::stringcvt::string_utf8_from_wide utf8Text(text.c_str()); pfc::string8 lyrics_utf8 = utf8Text.get_ptr();
    HANDLE hFile = CreateFileW(g_currentTrackPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) { DWORD written; char bom[] = { (char)0xEF, (char)0xBB, (char)0xBF }; WriteFile(hFile, bom, 3, &written, NULL); WriteFile(hFile, lyrics_utf8.get_ptr(), (DWORD)lyrics_utf8.length(), &written, NULL); CloseHandle(hFile); MessageBoxW(g_hEditorWnd, L"已保存到音频同目录", L"提示", MB_OK); g_isModified = false; return true; }
    else MessageBoxW(g_hEditorWnd, L"保存失败", L"错误", MB_ICONERROR);
    return false;
}

bool SaveToEmbeddedTag() {
    metadb_handle_ptr item; if (!playback_control::get()->get_now_playing(item)) { MessageBoxW(g_hEditorWnd, L"没有播放歌曲", L"错误", MB_OK); return false; }
    std::wstring text = GetFullText(); text = NormalizeLyrics(text);
    pfc::stringcvt::string_utf8_from_wide utf8Text(text.c_str()); pfc::string8 lyrics_utf8 = utf8Text.get_ptr();
    file_info_impl info; item->get_info(info); info.meta_set("LYRICS", lyrics_utf8); info.meta_remove_field("UNSYNCEDLYRICS");
    metadb_handle_list handles; handles.add_item(item); pfc::list_t<const file_info*> infos; infos.add_item(&info);
    try { static_api_ptr_t<metadb_io_v2> io; io->update_info_async_simple(handles, infos, g_hEditorWnd, metadb_io_v2::op_flag_background, nullptr); MessageBoxW(g_hEditorWnd, L"已保存到内嵌标签", L"提示", MB_OK); g_isModified = false; return true; }
    catch (...) { MessageBoxW(g_hEditorWnd, L"保存失败", L"错误", MB_ICONERROR); }
    return false;
}

bool SaveAsLrcFile() {
    wchar_t szFile[MAX_PATH] = { 0 }; OPENFILENAMEW ofn = { 0 }; ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = g_hEditorWnd; ofn.lpstrFilter = L"LRC Files (*.lrc)\0*.lrc\0All Files (*.*)\0*.*\0"; ofn.lpstrFile = szFile; ofn.nMaxFile = MAX_PATH; ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT; ofn.lpstrDefExt = L"lrc";
    if (GetSaveFileNameW(&ofn)) {
        std::wstring text = GetFullText(); text = NormalizeLyrics(text);
        pfc::stringcvt::string_utf8_from_wide utf8Text(text.c_str()); pfc::string8 lyrics_utf8 = utf8Text.get_ptr();
        HANDLE hFile = CreateFileW(szFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) { DWORD written; char bom[] = { (char)0xEF, (char)0xBB, (char)0xBF }; WriteFile(hFile, bom, 3, &written, NULL); WriteFile(hFile, lyrics_utf8.get_ptr(), (DWORD)lyrics_utf8.length(), &written, NULL); CloseHandle(hFile); g_currentTrackPath = szFile; g_sourceIsEmbedded = false; MessageBoxW(g_hEditorWnd, L"保存成功", L"提示", MB_OK); g_isModified = false; return true; }
        else MessageBoxW(g_hEditorWnd, L"保存失败", L"错误", MB_ICONERROR);
    }
    return false;
}

bool SmartSave() {
    bool res = false;
    if (g_sourceIsEmbedded) res = SaveToEmbeddedTag();
    else res = SaveToLrcFile();
    return res;
}

std::wstring GetTrackDisplayTitle(metadb_handle_ptr item) {
    pfc::string8 title, artist; metadb_info_container::ptr info_ref = item->get_info_ref();
    if (info_ref.is_valid()) { const file_info& info = info_ref->info(); t_size count = info.meta_get_count(); for (t_size i = 0; i < count; i++) { const char* name = info.meta_enum_name(i); if (name) { if (_stricmp(name, "TITLE") == 0) { if (info.meta_enum_value_count(i) > 0) title = info.meta_enum_value(i, 0); } else if (_stricmp(name, "ARTIST") == 0) { if (info.meta_enum_value_count(i) > 0) artist = info.meta_enum_value(i, 0); } } } }
    pfc::stringcvt::string_wide_from_utf8 wt(title); pfc::stringcvt::string_wide_from_utf8 wa(artist); std::wstring s; if (!title.is_empty()) s += wt.get_ptr(); if (!artist.is_empty()) { if (!title.is_empty()) s += L" - "; s += wa.get_ptr(); } if (s.empty()) s = L"无标题"; return s;
}

std::wstring LoadLyricsText(metadb_handle_ptr item, const std::wstring& lrcPath) {
    pfc::string8 lyrics; g_sourceIsEmbedded = false;
    if (item.is_valid()) { metadb_info_container::ptr info_ref = item->get_info_ref(); if (info_ref.is_valid()) { const file_info& info = info_ref->info(); t_size count = info.meta_get_count(); for (t_size i = 0; i < count; i++) { const char* name = info.meta_enum_name(i); if (name && (_stricmp(name, "LYRICS") == 0 || _stricmp(name, "UNSYNCEDLYRICS") == 0)) { if (info.meta_enum_value_count(i) > 0) { lyrics = info.meta_enum_value(i, 0); g_sourceIsEmbedded = true; break; } } } } }
    if (lyrics.is_empty() && !lrcPath.empty()) { g_sourceIsEmbedded = false; HANDLE hFile = CreateFileW(lrcPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL); if (hFile != INVALID_HANDLE_VALUE) { DWORD fileSize = GetFileSize(hFile, NULL); if (fileSize > 0 && fileSize < 10 * 1024 * 1024) { char* buffer = new char[fileSize + 1]; if (ReadFile(hFile, buffer, fileSize, NULL, NULL)) { buffer[fileSize] = '\0'; if (fileSize >= 3 && (unsigned char)buffer[0] == 0xEF && (unsigned char)buffer[1] == 0xBB && (unsigned char)buffer[2] == 0xBF) lyrics = buffer + 3; else lyrics = buffer; } delete[] buffer; } CloseHandle(hFile); } }
    pfc::stringcvt::string_wide_from_utf8 wideLyrics(lyrics); return wideLyrics.get_ptr();
}

std::wstring GetLrcPathFromPath(const std::wstring& audioPath) {
    std::wstring wpath = audioPath; if (wpath.find(L"file://") == 0) { wpath = wpath.substr(7); if (!wpath.empty() && wpath[0] == L'/') wpath = wpath.substr(1); } for (wchar_t& c : wpath) { if (c == L'/') c = L'\\'; } size_t dotPos = wpath.find_last_of(L'.'); if (dotPos != std::wstring::npos) return wpath.substr(0, dotPos) + L".lrc"; return L"";
}

static void DoLayout(HWND hwnd) {
    RECT rc; GetClientRect(hwnd, &rc);
    int width = rc.right, height = rc.bottom;
    int margin = 10, row1Y = 10, row2Y = 50, btnH = 32, btnW = 110, gap = 10;
    int x = margin;
    if (g_hBtnPause) SetWindowPos(g_hBtnPause, NULL, x, row1Y, 70, btnH, SWP_NOZORDER); x += 70 + gap;
    HWND hInsert = GetDlgItem(hwnd, IDC_BTN_INSERT);
    if (hInsert) SetWindowPos(hInsert, NULL, x, row1Y, 110, btnH, SWP_NOZORDER); x += 110 + gap;
    if (g_hChkDual) SetWindowPos(g_hChkDual, NULL, x, row1Y + 6, 110, 24, SWP_NOZORDER); x += 110 + gap;
    if (g_hSliderProgress) SetWindowPos(g_hSliderProgress, NULL, x, row1Y + 5, SLIDER_FIXED_WIDTH, 30, SWP_NOZORDER); x += SLIDER_FIXED_WIDTH + 10;
    if (g_hLblTime) SetWindowPos(g_hLblTime, NULL, x, row1Y + 6, TIME_LABEL_WIDTH, 24, SWP_NOZORDER);
    int bx = margin;
    HWND hGoto = GetDlgItem(hwnd, IDC_BTN_GOTO_LINE), hBack = GetDlgItem(hwnd, IDC_BTN_BACK), hUndo = GetDlgItem(hwnd, IDC_BTN_UNDO), hRemCur = GetDlgItem(hwnd, IDC_BTN_REMOVE_CUR), hRemAll = GetDlgItem(hwnd, IDC_BTN_REMOVE_ALL);
    if (hGoto) SetWindowPos(hGoto, NULL, bx, row2Y, btnW, btnH, SWP_NOZORDER); bx += btnW + gap;
    if (hBack) SetWindowPos(hBack, NULL, bx, row2Y, btnW, btnH, SWP_NOZORDER); bx += btnW + gap;
    if (hUndo) SetWindowPos(hUndo, NULL, bx, row2Y, btnW, btnH, SWP_NOZORDER); bx += btnW + gap;
    if (hRemCur) SetWindowPos(hRemCur, NULL, bx, row2Y, btnW, btnH, SWP_NOZORDER); bx += btnW + gap;
    if (hRemAll) SetWindowPos(hRemAll, NULL, bx, row2Y, btnW, btnH, SWP_NOZORDER);
    int editY = row2Y + btnH + margin;
    int editH = height - editY - 60; if (editH < 80) editH = 80;
    if (g_hEditBox) SetWindowPos(g_hEditBox, NULL, margin, editY, width - 2 * margin, editH, SWP_NOZORDER);
    int bottomY = height - 45; int wBtn = 80; int rightX = width - margin;
    HWND hCancel = GetDlgItem(hwnd, IDC_BTN_CANCEL), hOk = GetDlgItem(hwnd, IDC_BTN_OK), hApply = GetDlgItem(hwnd, IDC_BTN_APPLY);
    if (hCancel) SetWindowPos(hCancel, NULL, rightX - wBtn, bottomY, wBtn, 32, SWP_NOZORDER); rightX -= wBtn + 10;
    if (hOk) SetWindowPos(hOk, NULL, rightX - wBtn, bottomY, wBtn, 32, SWP_NOZORDER); rightX -= wBtn + 10;
    if (hApply) SetWindowPos(hApply, NULL, rightX - wBtn, bottomY, wBtn, 32, SWP_NOZORDER);
}

void TryCloseWindow() {
    if (g_isModified) {
        int ret = MessageBoxW(g_hEditorWnd, L"歌词已修改，是否保存？", L"提示", MB_YESNOCANCEL | MB_ICONWARNING);
        if (ret == IDYES) { if (SmartSave()) DestroyWindow(g_hEditorWnd); }
        else if (ret == IDNO) { DestroyWindow(g_hEditorWnd); }
    }
    else { DestroyWindow(g_hEditorWnd); }
}

LRESULT CALLBACK EditorWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam; RECT rc; GetClientRect(hwnd, &rc);
        HBRUSH br = IsDarkMode() ? g_hDarkBrush : g_hLightBrush; FillRect(hdc, &rc, br); return 1;
    }
    case WM_NOTIFY: {
        LPNMHDR pnmh = (LPNMHDR)lParam;
        if (pnmh->hwndFrom == g_hSliderProgress && pnmh->code == NM_CUSTOMDRAW) {
            LPNMCUSTOMDRAW pnmcd = (LPNMCUSTOMDRAW)lParam;
            if (pnmcd->dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
            else if (pnmcd->dwDrawStage == CDDS_ITEMPREPAINT) {
                if (pnmcd->dwItemSpec == TBCD_THUMB) { RECT rcThumb; SendMessage(g_hSliderProgress, TBM_GETTHUMBRECT, 0, (LPARAM)&rcThumb); HBRUSH hBrush = CreateSolidBrush(RGB(0, 120, 215)); FillRect(pnmcd->hdc, &rcThumb, hBrush); DeleteObject(hBrush); return CDRF_SKIPDEFAULT; }
            }
        } break;
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        if (IsDarkMode()) { SetBkMode(hdc, OPAQUE); SetBkColor(hdc, RGB(25, 25, 25)); SetTextColor(hdc, RGB(240, 240, 240)); return (LRESULT)g_hDarkBrush; }
        SetBkMode(hdc, OPAQUE); SetBkColor(hdc, GetSysColor(COLOR_WINDOW)); SetTextColor(hdc, RGB(0, 0, 0)); return (LRESULT)g_hLightBrush;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        if (IsDarkMode()) { SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, RGB(240, 240, 240)); return (LRESULT)GetStockObject(NULL_BRUSH); }
        SetBkMode(hdc, OPAQUE); SetBkColor(hdc, GetSysColor(COLOR_WINDOW)); SetTextColor(hdc, RGB(0, 0, 0)); return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }
    case WM_SIZE:
    case WM_MOVE: {
        GetWindowRect(hwnd, &g_windowRect); g_hasSavedPos = true;
        if (msg == WM_SIZE) DoLayout(hwnd);
        return 0;
    }
    case WM_CREATE: {
        g_hDarkBrush = CreateSolidBrush(RGB(30, 30, 30));
        g_hLightBrush = GetSysColorBrush(COLOR_WINDOW);
        if (!g_pEditorHooks) g_pEditorHooks = new fb2k::CCoreDarkModeHooks();

        LoadSettings(); // 加载完整字体设置

        metadb_handle_ptr item; if (!playback_control::get()->get_now_playing(item)) { MessageBoxW(hwnd, L"没有播放歌曲", L"提示", MB_ICONINFORMATION); return -1; }
        pfc::string8 filepath = item->get_path(); pfc::stringcvt::string_wide_from_utf8 widePath(filepath); g_currentTrackPath = GetLrcPathFromPath(widePath.get_ptr());
        std::wstring wTitle = L"蒙古文歌词编辑器 - "; wTitle += GetTrackDisplayTitle(item); SetWindowTextW(hwnd, wTitle.c_str());

        HMENU hMenu = CreateMenu(); HMENU hFile = CreatePopupMenu();
        AppendMenuW(hFile, MF_STRING, IDM_FILE_SAVE, L"保存到音频所在位置 (&S)\tCtrl+S");
        AppendMenuW(hFile, MF_STRING, IDM_FILE_SAVE_AS, L"另存为 (&A)...");
        AppendMenuW(hFile, MF_STRING, IDM_FILE_SAVE_TAG, L"保存到内嵌标签 (&T)");
        AppendMenuW(hFile, MF_SEPARATOR, 0, NULL); AppendMenuW(hFile, MF_STRING, IDM_FILE_EXIT, L"退出 (&X)");
        AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFile, L"文件(&F)");
        HMENU hEdit = CreatePopupMenu();
        AppendMenuW(hEdit, MF_STRING | (g_isDualMode ? MF_CHECKED : MF_UNCHECKED), IDM_EDIT_DUAL, L"插入双语歌词 (&D)");
        AppendMenuW(hEdit, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hEdit, MF_STRING, IDM_EDIT_FIND_REPLACE, L"查找和替换 (&F)\tCtrl+F");
        AppendMenuW(hEdit, MF_STRING, IDM_EDIT_SHIFT_TIME, L"时间轴整体偏移 (&S)...");
        AppendMenuW(hEdit, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hEdit, MF_STRING, IDM_EDIT_REMOVE_ORIG, L"移除所有原文歌词");
        AppendMenuW(hEdit, MF_STRING, IDM_EDIT_REMOVE_TRANS, L"移除所有翻译行歌词");
        AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hEdit, L"编辑(&E)");
        HMENU hFont = CreatePopupMenu(); AppendMenuW(hFont, MF_STRING, IDM_FONT_CHANGE, L"字体更换 (&F)..."); AppendMenuW(hFont, MF_STRING, IDM_FONT_ZOOM_IN, L"放大字体 (&I)\tCtrl++"); AppendMenuW(hFont, MF_STRING, IDM_FONT_ZOOM_OUT, L"缩小字体 (&O)\tCtrl+-"); AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFont, L"字体(&T)"); SetMenu(hwnd, hMenu);

        RECT rc; GetClientRect(hwnd, &rc); int width = rc.right, height = rc.bottom;
        if (!g_hButtonFont) g_hButtonFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");

        g_hBtnPause = CreateWindowW(L"BUTTON", L"暂停", WS_CHILD | WS_VISIBLE, 10, 10, 70, 32, hwnd, (HMENU)IDC_BTN_PAUSE, NULL, NULL); SendMessage(g_hBtnPause, WM_SETFONT, (WPARAM)g_hButtonFont, TRUE);
        HWND hInsert = CreateWindowW(L"BUTTON", L"插入时间轴 (F3)", WS_CHILD | WS_VISIBLE, 90, 10, 110, 32, hwnd, (HMENU)IDC_BTN_INSERT, NULL, NULL); SendMessage(hInsert, WM_SETFONT, (WPARAM)g_hButtonFont, TRUE);
        g_hChkDual = CreateWindowW(L"BUTTON", L"插入双语歌词", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 210, 16, 110, 24, hwnd, (HMENU)IDC_CHK_DUAL, NULL, NULL); SendMessage(g_hChkDual, BM_SETCHECK, g_isDualMode ? BST_CHECKED : BST_UNCHECKED, 0); SendMessage(g_hChkDual, WM_SETFONT, (WPARAM)g_hButtonFont, TRUE);
        g_hSliderProgress = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS, 330, 15, SLIDER_FIXED_WIDTH, 30, hwnd, (HMENU)IDC_SLIDER_PROGRESS, NULL, NULL);
        SendMessage(g_hSliderProgress, TBM_SETRANGE, TRUE, MAKELONG(0, 10000));
        g_OldSliderProc = (WNDPROC)SetWindowLongPtr(g_hSliderProgress, GWLP_WNDPROC, (LONG_PTR)SliderSubclassProc);
        g_hLblTime = CreateWindowW(L"STATIC", L"00:00.00 / 00:00.00", WS_CHILD | WS_VISIBLE | SS_RIGHT, 600, 16, TIME_LABEL_WIDTH, 24, hwnd, (HMENU)IDC_LBL_TIME, NULL, NULL);
        SendMessage(g_hLblTime, WM_SETFONT, (WPARAM)g_hButtonFont, TRUE);

        int bx = 10; int btnW = 110; int gap = 10; int row2Y = 50;
        HWND hGoto = CreateWindowW(L"BUTTON", L"跳转当前行 (F1)", WS_CHILD | WS_VISIBLE, bx, row2Y, btnW, 32, hwnd, (HMENU)IDC_BTN_GOTO_LINE, NULL, NULL); SendMessage(hGoto, WM_SETFONT, (WPARAM)g_hButtonFont, TRUE); bx += btnW + gap;
        HWND hBack = CreateWindowW(L"BUTTON", L"回退当前句 (F2)", WS_CHILD | WS_VISIBLE, bx, row2Y, btnW, 32, hwnd, (HMENU)IDC_BTN_BACK, NULL, NULL); SendMessage(hBack, WM_SETFONT, (WPARAM)g_hButtonFont, TRUE); bx += btnW + gap;
        HWND hUndo = CreateWindowW(L"BUTTON", L"撤销 (Ctrl+Z)", WS_CHILD | WS_VISIBLE, bx, row2Y, btnW, 32, hwnd, (HMENU)IDC_BTN_UNDO, NULL, NULL); SendMessage(hUndo, WM_SETFONT, (WPARAM)g_hButtonFont, TRUE); bx += btnW + gap;
        HWND hRemCur = CreateWindowW(L"BUTTON", L"移除当前", WS_CHILD | WS_VISIBLE, bx, row2Y, btnW, 32, hwnd, (HMENU)IDC_BTN_REMOVE_CUR, NULL, NULL); SendMessage(hRemCur, WM_SETFONT, (WPARAM)g_hButtonFont, TRUE); bx += btnW + gap;
        HWND hRemAll = CreateWindowW(L"BUTTON", L"移除全部", WS_CHILD | WS_VISIBLE, bx, row2Y, btnW, 32, hwnd, (HMENU)IDC_BTN_REMOVE_ALL, NULL, NULL); SendMessage(hRemAll, WM_SETFONT, (WPARAM)g_hButtonFont, TRUE);

        int editBoxY = 100; int editBoxHeight = height - editBoxY - 60;
        g_hEditBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"RichEdit50W", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_WANTRETURN | ES_NOHIDESEL, 10, editBoxY, width - 20, editBoxHeight, hwnd, (HMENU)IDC_EDIT_LYRIC, NULL, NULL);
        if (IsDarkMode()) SendMessage(g_hEditBox, EM_SETBKGNDCOLOR, 0, (LPARAM)RGB(25, 25, 25));

        g_OldEditProc = (WNDPROC)SetWindowLongPtr(g_hEditBox, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

        // ★★★ 关键修复：先加载文本，最后再应用字体 ★★★
        std::wstring lyrics = LoadLyricsText(item, g_currentTrackPath);
        SetWindowTextW(g_hEditBox, lyrics.c_str());

        // 最后调用 ApplyNewFont，确保字体能应用到已加载的文本上
        ApplyNewFont();

        if (g_sourceIsEmbedded) SetWindowTextW(hwnd, (wTitle + L" (内嵌标签)").c_str());
        g_pEditorHooks->AddDialogWithControls(hwnd);
        g_pEditorHooks->AddCtrlAuto(g_hChkDual);
        DoLayout(hwnd);
        SetTimer(hwnd, 1, 50, NULL); SetTimer(hwnd, 2, 1000, NULL);
        g_isModified = false;
        return 0;
    }
    case WM_TIMER: {
        if (wParam == 1) UpdateProgressDisplay();
        if (wParam == 2 && g_hBtnPause) { bool playing = playback_control::get()->is_playing(); bool paused = playback_control::get()->is_paused(); SetWindowTextW(g_hBtnPause, (playing && !paused) ? L"暂停" : L"播放"); }
        return 0;
    }
    case WM_COMMAND: {
        if (HIWORD(wParam) == EN_CHANGE && LOWORD(wParam) == IDC_EDIT_LYRIC) g_isModified = true;
        int wmId = LOWORD(wParam);
        switch (wmId) {
        case IDC_BTN_PAUSE: {
            if (playback_control::get()->is_playing()) {
                bool paused = playback_control::get()->is_paused(); playback_control::get()->pause(!paused);
                bool isNowPlaying = playback_control::get()->is_playing(); bool isNowPaused = playback_control::get()->is_paused();
                SetWindowTextW(g_hBtnPause, (isNowPlaying && !isNowPaused) ? L"暂停" : L"播放");
            } break;
        }
        case IDC_BTN_INSERT: InsertCurrentTime(); break;
        case IDC_BTN_GOTO_LINE: GotoCurrentLine(); break;
        case IDC_BTN_BACK: CustomRedo(); break;
        case IDC_BTN_UNDO: DoUndo(); break;
        case IDC_BTN_REMOVE_CUR: RemoveCurrentTimestamp(); break;
        case IDC_BTN_REMOVE_ALL: RemoveAllTimestamps(); break;
        case IDM_EDIT_FIND_REPLACE: ShowFindReplaceDialog(); break;
        case IDM_EDIT_SHIFT_TIME: ShowShiftTimeDialog(); break;
        case IDM_EDIT_REMOVE_ORIG: RemoveOriginalLyrics(); break;
        case IDM_EDIT_REMOVE_TRANS: RemoveTranslationLyrics(); break;
        case IDM_FILE_SAVE_TAG: SaveToEmbeddedTag(); break;
        case IDM_FILE_SAVE_AS: SaveAsLrcFile(); break;
        case IDM_FONT_CHANGE: {
            CHOOSEFONTW cf = { sizeof(cf) };
            cf.hwndOwner = hwnd;
            cf.lpLogFont = &g_logFont;
            cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_NOVERTFONTS;
            if (ChooseFontW(&cf)) {
                g_editorFontSize = abs(g_logFont.lfHeight);
                if (g_editorFontSize < 8) g_editorFontSize = 8;
                ApplyNewFont();
            }
            break;
        }
        case IDM_FONT_ZOOM_IN: { g_editorFontSize += 2; if (g_editorFontSize > 72) g_editorFontSize = 72; ApplyNewFont(); break; }
        case IDM_FONT_ZOOM_OUT: { g_editorFontSize -= 2; if (g_editorFontSize < 8) g_editorFontSize = 8; ApplyNewFont(); break; }
        case IDC_CHK_DUAL: { g_isDualMode = (SendMessage(g_hChkDual, BM_GETCHECK, 0, 0) == BST_CHECKED); HMENU hm = GetMenu(hwnd); if (hm) { HMENU he = GetSubMenu(hm, 1); if (he) CheckMenuItem(he, IDM_EDIT_DUAL, MF_BYCOMMAND | (g_isDualMode ? MF_CHECKED : MF_UNCHECKED)); } break; }
        case IDM_EDIT_DUAL: { g_isDualMode = !g_isDualMode; if (g_hChkDual) SendMessage(g_hChkDual, BM_SETCHECK, g_isDualMode ? BST_CHECKED : BST_UNCHECKED, 0); HMENU hm = GetMenu(hwnd); if (hm) { HMENU he = GetSubMenu(hm, 1); if (he) CheckMenuItem(he, IDM_EDIT_DUAL, MF_BYCOMMAND | (g_isDualMode ? MF_CHECKED : MF_UNCHECKED)); } break; }
        case IDM_FILE_SAVE: SaveToLrcFile(); break;
        case IDC_BTN_APPLY: SmartSave(); break;
        case IDC_BTN_OK: if (SmartSave()) DestroyWindow(hwnd); break;
        case IDC_BTN_CANCEL:
        case IDM_FILE_EXIT: TryCloseWindow(); break;
        }
        return 0;
    }
    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE) { TryCloseWindow(); return 0; }
        if (wParam == 'S' && (GetKeyState(VK_CONTROL) & 0x8000)) { SaveToLrcFile(); return 0; }
        if (wParam == VK_ADD && (GetKeyState(VK_CONTROL) & 0x8000)) { g_editorFontSize += 2; if (g_editorFontSize > 72) g_editorFontSize = 72; ApplyNewFont(); return 0; }
        if (wParam == VK_SUBTRACT && (GetKeyState(VK_CONTROL) & 0x8000)) { g_editorFontSize -= 2; if (g_editorFontSize < 8) g_editorFontSize = 8; ApplyNewFont(); return 0; }
        break;
    }
    case WM_CLOSE: TryCloseWindow(); return 0;
    case WM_DESTROY:
        GetWindowRect(hwnd, &g_windowRect); SaveSettings(); KillTimer(hwnd, 1); KillTimer(hwnd, 2);
        if (g_hDarkBrush) { DeleteObject(g_hDarkBrush); g_hDarkBrush = NULL; }
        if (g_pEditorHooks) { delete g_pEditorHooks; g_pEditorHooks = nullptr; }
        if (g_hShiftTimeDlg) { DestroyWindow(g_hShiftTimeDlg); g_hShiftTimeDlg = NULL; if (g_pShiftHooks) { delete g_pShiftHooks; g_pShiftHooks = nullptr; } }
        if (g_hFindReplaceDlg) { DestroyWindow(g_hFindReplaceDlg); g_hFindReplaceDlg = NULL; if (g_pFrHooks) { delete g_pFrHooks; g_pFrHooks = nullptr; } }
        g_hEditorWnd = NULL; g_hBtnPause = NULL; g_hSliderProgress = NULL; g_hLblTime = NULL;
        g_currentTrackPath.clear(); g_undoStack.clear(); g_redoStack.clear();
        if (g_hCurrentFont) { DeleteObject(g_hCurrentFont); g_hCurrentFont = NULL; }
        if (g_hButtonFont) { DeleteObject(g_hButtonFont); g_hButtonFont = NULL; }
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void ShowLyricEditor(HWND hParent) {
    LoadLibrary(L"msftedit.dll");
    LoadSettings();
    if (g_hEditorWnd && IsWindow(g_hEditorWnd)) { SetForegroundWindow(g_hEditorWnd); return; }
    WNDCLASSW wc = { 0 }; wc.lpfnWndProc = EditorWndProc; wc.hInstance = GetModuleHandle(NULL); wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"MongolLyricEditorClass";
    HWND mainWnd = core_api::get_main_window(); HICON hIcon = (HICON)SendMessage(mainWnd, WM_GETICON, ICON_BIG, 0); if (!hIcon) hIcon = (HICON)GetClassLongPtr(mainWnd, GCLP_HICON); if (!hIcon) hIcon = LoadIcon(NULL, IDI_APPLICATION); wc.hIcon = hIcon; RegisterClassW(&wc);
    int winW = g_windowRect.right - g_windowRect.left, winH = g_windowRect.bottom - g_windowRect.top;
    if (winW < 800) winW = 900; if (winH < 600) winH = 850;
    int posX, posY;
    if (!g_hasSavedPos) { RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0); posX = wa.left + (wa.right - wa.left - winW) / 2; posY = wa.top + (wa.bottom - wa.top - winH) / 2; }
    else { posX = g_windowRect.left; posY = g_windowRect.top; }
    g_hEditorWnd = CreateWindowExW(WS_CLIPCHILDREN, L"MongolLyricEditorClass", L"蒙古文歌词编辑器", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, posX, posY, winW, winH, NULL, NULL, GetModuleHandle(NULL), NULL);
    if (g_hEditorWnd) { ShowWindow(g_hEditorWnd, SW_SHOW); UpdateWindow(g_hEditorWnd); }
}