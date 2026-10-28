#include "stdafx.h"
#include <SDK/menu.h> 
#include <SDK/playback_control.h>
#include <SDK/metadb.h>
#include <SDK/ui.h>
#include <SDK/core_api.h>

extern bool g_isVisible;
extern bool g_isLocked;
extern bool g_isTopmost;
extern HWND g_hwndLyrics;
extern int g_displayMode;
extern const GUID guid_preferences_page;
extern const GUID guid_pref_desktop;
extern const GUID guid_pref_panel;
extern void FetchLyricsNow(metadb_handle_ptr item);
extern int g_reload_flag;

// 新增：引用面板歌词的相关变量和函数
extern int g_panelDisplayMode;
extern void SavePanelSettings();

// 【第四处修改 1/2】引入桌面歌词的保存函数
extern void SaveSettings(HWND hwnd);

static const GUID guid_menu_group = { 0x77777777, 0x7777, 0x7777,{ 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77 } };
static const GUID guid_menu_display = { 0x88888888, 0x8888, 0x8888,{ 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88 } };

static const GUID guid_cmd_toggle = { 0x11111111, 0x1111, 0x1111,{ 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11 } };
static const GUID guid_cmd_lock = { 0x12121212, 0x1212, 0x1212,{ 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12 } };
static const GUID guid_cmd_topmost = { 0x13131313, 0x1313, 0x1313,{ 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13 } };
static const GUID guid_cmd_settings = { 0x55555555, 0x5555, 0x5555,{ 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 } };
static const GUID guid_cmd_reload = { 0x66666666, 0x6666, 0x6666,{ 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66 } };

static const GUID guid_cmd_mode_dual = { 0x22222222, 0x2222, 0x2222,{ 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22 } };
static const GUID guid_cmd_mode_mon = { 0x33333333, 0x3333, 0x3333,{ 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33 } };
static const GUID guid_cmd_mode_cn = { 0x44444444, 0x4444, 0x4444,{ 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44 } };

static mainmenu_group_popup_factory g_menu_group(guid_menu_group, mainmenu_groups::view, mainmenu_commands::sort_priority_dontcare, "\xE8\x92\x99\xE5\x8F\xA4\xE6\x96\x87\xE6\xAD\x8C\xE8\xAF\x8D");
static mainmenu_group_popup_factory g_menu_display(guid_menu_display, guid_menu_group, 20, "\xE5\x8F\x8C\xE8\xAF\xAD\xE6\xAD\x8C\xE8\xAF\x8D\xE6\x98\xBE\xE7\xA4\xBA");

class MongolMenuTop : public mainmenu_commands {
public:
    t_uint32 get_command_count() override { return 3; }
    t_uint32 get_sort_priority() override { return 10; }

    GUID get_command(t_uint32 p_index) override {
        if (p_index == 0) return guid_cmd_toggle;
        if (p_index == 1) return guid_cmd_lock;
        return guid_cmd_topmost;
    }

    void get_name(t_uint32 p_index, pfc::string_base& p_out) override {
        if (p_index == 0) p_out = "\xE5\x90\xAF\xE7\x94\xA8\xE6\xA1\x8C\xE9\x9D\xA2\xE6\xAD\x8C\xE8\xAF\x8D";
        else if (p_index == 1) p_out = "\xE9\x94\x81\xE5\xAE\x9A\xE6\xA1\x8C\xE9\x9D\xA2\xE6\xAD\x8C\xE8\xAF\x8D";
        else p_out = "\xE7\xBD\xAE\xE9\xA1\xB6\xE6\xA1\x8C\xE9\x9D\xA2\xE6\xAD\x8C\xE8\xAF\x8D";
    }

    bool get_description(t_uint32 p_index, pfc::string_base& p_out) override {
        p_out = "";
        return true;
    }

    bool get_display(t_uint32 p_index, pfc::string_base& p_text, t_uint32& p_flags) override {
        p_flags = 0;
        get_name(p_index, p_text);

        if (p_index == 0 && g_isVisible) p_flags |= mainmenu_commands::flag_checked;
        if (p_index == 1 && g_isLocked) p_flags |= mainmenu_commands::flag_checked;
        if (p_index == 2 && g_isTopmost) p_flags |= mainmenu_commands::flag_checked;

        return true;
    }

    GUID get_parent() override { return guid_menu_group; }

    void execute(t_uint32 p_index, service_ptr_t<service_base> p_callback) override {
        if (p_index == 0) {
            g_isVisible = !g_isVisible;
            if (g_hwndLyrics) ShowWindow(g_hwndLyrics, g_isVisible ? SW_SHOW : SW_HIDE);
        }
        else if (p_index == 1) {
            g_isLocked = !g_isLocked;
            if (g_hwndLyrics) {
                LONG_PTR exStyle = GetWindowLongPtr(g_hwndLyrics, GWL_EXSTYLE);
                if (g_isLocked) SetWindowLongPtr(g_hwndLyrics, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT);
                else SetWindowLongPtr(g_hwndLyrics, GWL_EXSTYLE, exStyle & ~WS_EX_TRANSPARENT);
            }
        }
        else if (p_index == 2) {
            g_isTopmost = !g_isTopmost;
            if (g_hwndLyrics) {
                SetWindowPos(g_hwndLyrics, g_isTopmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
        }
    }
};
static mainmenu_commands_factory_t<MongolMenuTop> g_factory_top;

class MongolMenuBottom : public mainmenu_commands {
public:
    t_uint32 get_command_count() override { return 2; }
    t_uint32 get_sort_priority() override { return 30; }

    GUID get_command(t_uint32 p_index) override {
        if (p_index == 0) return guid_cmd_reload;
        return guid_cmd_settings;
    }

    void get_name(t_uint32 p_index, pfc::string_base& p_out) override {
        if (p_index == 0) p_out = "\xE9\x87\x8D\xE6\x96\xB0\xE8\xBD\xBD\xE5\x85\xA5\xE6\xAD\x8C\xE8\xAF\x8D";
        else p_out = "\xE8\x92\x99\xE5\x8F\xA4\xE6\x96\x87\xE6\xAD\x8C\xE8\xAF\x8D\xE8\xAE\xBE\xE7\xBD\xAE";
    }

    bool get_description(t_uint32 p_index, pfc::string_base& p_out) override {
        p_out = "";
        return true;
    }

    bool get_display(t_uint32 p_index, pfc::string_base& p_text, t_uint32& p_flags) override {
        p_flags = 0;
        get_name(p_index, p_text);
        return true;
    }

    GUID get_parent() override { return guid_menu_group; }

    void execute(t_uint32 p_index, service_ptr_t<service_base> p_callback) override {
        if (p_index == 0) {
            g_reload_flag++;

            metadb_handle_ptr item;
            if (playback_control::get()->get_now_playing(item)) {
                FetchLyricsNow(item);
                if (g_hwndLyrics) InvalidateRect(g_hwndLyrics, NULL, TRUE);
            }
        }
        else if (p_index == 1) {
            fb2k::inMainThread([] {
                static_api_ptr_t<ui_control>()->show_preferences(guid_preferences_page);
                });
        }
    }
};
static mainmenu_commands_factory_t<MongolMenuBottom> g_factory_bottom;

class MongolDisplayMenu : public mainmenu_commands {
public:
    t_uint32 get_command_count() override { return 3; }

    GUID get_command(t_uint32 p_index) override {
        if (p_index == 0) return guid_cmd_mode_dual;
        if (p_index == 1) return guid_cmd_mode_mon;
        return guid_cmd_mode_cn;
    }

    void get_name(t_uint32 p_index, pfc::string_base& p_out) override {
        if (p_index == 0) p_out = "\xE6\x98\xBE\xE7\xA4\xBA\xE5\x8F\x8C\xE8\xAF\xAD\xE6\xAD\x8C\xE8\xAF\x8D";
        else if (p_index == 1) p_out = "\xE4\xBB\x85\xE6\x98\xBE\xE7\xA4\xBA\xE8\x92\x99\xE5\x8F\xA4\xE6\x96\x87";
        else p_out = "\xE4\xBB\x85\xE6\x98\xBE\xE7\xA4\xBA\xE4\xB8\xAD\xE6\x96\x87\xE7\xBF\xBB\xE8\xAF\x91";
    }

    bool get_description(t_uint32 p_index, pfc::string_base& p_out) override {
        p_out = "";
        return true;
    }

    bool get_display(t_uint32 p_index, pfc::string_base& p_text, t_uint32& p_flags) override {
        get_name(p_index, p_text);
        p_flags = 0;
        if (g_displayMode == (int)p_index) {
            p_flags |= mainmenu_commands::flag_radiochecked;
        }
        return true;
    }

    GUID get_parent() override { return guid_menu_display; }

    // 【第四处修改 2/2】加上保存桌面歌词设置的逻辑
    void execute(t_uint32 p_index, service_ptr_t<service_base> p_callback) override {
        // 同时设置桌面和面板的显示模式
        g_displayMode = (int)p_index;
        g_panelDisplayMode = (int)p_index;

        SavePanelSettings(); // 保存面板设置，防止重启后回退

        // 同步保存桌面歌词设置
        if (g_hwndLyrics) {
            SaveSettings(g_hwndLyrics);
            InvalidateRect(g_hwndLyrics, NULL, TRUE);
        }
    }
};
static mainmenu_commands_factory_t<MongolDisplayMenu> g_display_menu_factory;