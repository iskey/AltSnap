/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright (C) 2015    Stefan Sundin                                   *
 * This program is free software: you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation, either version 3 or later.              *
 * Modified By Raymond Gillibert in 2020                                 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <commctrl.h>
#include <windowsx.h>
#include "resource.h"

BOOL    CALLBACK PropSheetProc(HWND, UINT, LPARAM);
INT_PTR CALLBACK GeneralPageDialogProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK MousePageDialogProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK KeyboardPageDialogProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK BlacklistPageDialogProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK AboutPageDialogProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK AdvancedPageDialogProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK CursorProc(HWND, UINT, WPARAM, LPARAM);

HWND g_cfgwnd = NULL;

/////////////////////////////////////////////////////////////////////////////
// No error reporting since we don't want the user to be interrupted
static void CheckAutostart(int *on, int *hidden, int *elevated)
{
    *on = *hidden = *elevated = 0;
    // Read registry
    HKEY key;
    wchar_t value[MAX_PATH+20] = L"";
    DWORD len = sizeof(value);
    RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &key);
    RegQueryValueEx(key, APP_NAME, NULL, NULL, (LPBYTE)value, &len);
    RegCloseKey(key);
    // Compare
    wchar_t path[MAX_PATH], compare[MAX_PATH+20];
    GetModuleFileName(NULL, path, ARR_SZ(path));
    swprintf(compare, ARR_SZ(compare), L"\"%s\"", path);
    if (wcsstr(value,compare) != value) {
        return;
    }
    // Autostart is on, check arguments
    *on = 1;
    if (wcsstr(value,L" -hide") != NULL) {
        *hidden = 1;
    }
    if (wcsstr(value,L" -elevate") != NULL) {
        *elevated = 1;
    }
}

static void SetAutostart(int on, int hide, int elevate)
{
    // Open key
    HKEY key;
    int error = RegCreateKeyEx(HKEY_CURRENT_USER
                              , L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
                              , 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL);
    if (error != ERROR_SUCCESS) return;
    if (on) {
        // Get path
        wchar_t path[MAX_PATH], value[MAX_PATH+20];
        GetModuleFileName(NULL, path, ARR_SZ(path));
        swprintf(value, ARR_SZ(value), L"\"%s\"%s%s", path, (hide?L" -hide":L""), (elevate?L" -elevate":L""));
        // Set autostart
        error = RegSetValueEx(key, APP_NAME, 0, REG_SZ, (LPBYTE)value, (wcslen(value)+1)*sizeof(value[0]));
        if (error != ERROR_SUCCESS) return;

    } else {
        // Remove
        error = RegDeleteValue(key, APP_NAME);
        if (error != ERROR_SUCCESS) return;
    }
    // Close key
    RegCloseKey(key);
}
BOOL ElevateNow(int showconfig)
{
        wchar_t path[MAX_PATH];
        GetModuleFileName(NULL, path, ARR_SZ(path));
        INT_PTR ret;
        if(showconfig)
            ret = (INT_PTR)ShellExecute(NULL, L"runas", path, L"-config -multi", NULL, SW_SHOWNORMAL);
        else
            ret = (INT_PTR)ShellExecute(NULL, L"runas", path, L"-multi", NULL, SW_SHOWNORMAL);

        if (ret > 32) {
            PostMessage(g_hwnd, WM_CLOSE, 0, 0);
        } else {
            MessageBox(NULL, l10n->general_elevation_aborted, APP_NAME, MB_ICONINFORMATION | MB_OK);
        }
        return FALSE;
}
/////////////////////////////////////////////////////////////////////////////
// Entry point
void OpenConfig(int startpage)
{
    if (IsWindow(g_cfgwnd)) {
        PropSheet_SetCurSel(g_cfgwnd, 0, startpage);
        SetForegroundWindow(g_cfgwnd);
        return;
    }
    // Define the pages
    struct {
        int pszTemplate;
        DLGPROC pfnDlgProc;
    } pages[] = {
        { IDD_GENERALPAGE,   GeneralPageDialogProc  },
        { IDD_MOUSEPAGE,     MousePageDialogProc    },
        { IDD_KBPAGE,        KeyboardPageDialogProc },
        { IDD_BLACKLISTPAGE, BlacklistPageDialogProc},
        { IDD_ADVANCEDPAGE,  AdvancedPageDialogProc },
        { IDD_ABOUTPAGE,     AboutPageDialogProc    } };

    PROPSHEETPAGE psp[ARR_SZ(pages)] = { };
    size_t i;
    for (i = 0; i < ARR_SZ(pages); i++) {
        psp[i].dwSize = sizeof(PROPSHEETPAGE);
        psp[i].hInstance = g_hinst;
        psp[i].pszTemplate = MAKEINTRESOURCE(pages[i].pszTemplate);
        psp[i].pfnDlgProc = pages[i].pfnDlgProc;
    }

    // Define the property sheet
    PROPSHEETHEADER psh = { sizeof(PROPSHEETHEADER) };
    psh.dwFlags = PSH_PROPSHEETPAGE | PSH_USECALLBACK | PSH_USEHICON | PSH_NOAPPLYNOW ;
    psh.hwndParent = NULL;
    psh.hInstance = g_hinst;
    psh.hIcon = icon[1];
    psh.pszCaption = APP_NAME;
    psh.nPages = ARR_SZ(pages);
    psh.ppsp = (LPCPROPSHEETPAGE) &psp;
    psh.pfnCallback = PropSheetProc;
    psh.nStartPage = startpage;

    // Open the property sheet
    PropertySheet(&psh);
}
/////////////////////////////////////////////////////////////////////////////
void CloseConfig()
{
    PostMessage(g_cfgwnd, WM_CLOSE, 0, 0);
    UnregisterClass(APP_NAME"-Test", g_hinst);
}
void UpdateSettings()
{
    PostMessage(g_hwnd, WM_UPDATESETTINGS, 1, 0);
}
/////////////////////////////////////////////////////////////////////////////
static void UpdateStrings()
{
    // Update window title
    PropSheet_SetTitle(g_cfgwnd, 0, l10n->title);

    // Update tab titles
    HWND tc = PropSheet_GetTabControl(g_cfgwnd);
    int numrows_prev = TabCtrl_GetRowCount(tc);
    wchar_t *titles[] = { l10n->tab_general, l10n->tab_mouse, l10n->tab_keyboard
                        , l10n->tab_blacklist, l10n->tab_advanced,l10n->tab_about };
    size_t i;
    for (i = 0; i < ARR_SZ(titles); i++) {
        TCITEM ti;
        ti.mask = TCIF_TEXT;
        ti.pszText = titles[i];
        TabCtrl_SetItem(tc, i, &ti);
    }

    // Modify UI if number of rows have changed
    int numrows = TabCtrl_GetRowCount(tc);
    if (numrows_prev != numrows) {
        HWND page = PropSheet_GetCurrentPageHwnd(g_cfgwnd);
        if (page != NULL) {
            int diffrows = numrows - numrows_prev;
            WINDOWPLACEMENT wndpl = { sizeof(WINDOWPLACEMENT) };
            // Resize window
            GetWindowPlacement(g_cfgwnd, &wndpl);
            wndpl.rcNormalPosition.bottom += 18 * diffrows;
            SetWindowPlacement(g_cfgwnd, &wndpl);
            // Resize tabcontrol
            GetWindowPlacement(tc, &wndpl);
            wndpl.rcNormalPosition.bottom += 18 * diffrows;
            SetWindowPlacement(tc, &wndpl);
            // Move button
            HWND button = GetDlgItem(g_cfgwnd, IDOK);
            GetWindowPlacement(button, &wndpl);
            int height = wndpl.rcNormalPosition.bottom - wndpl.rcNormalPosition.top;
            wndpl.rcNormalPosition.top += 18 * diffrows;
            wndpl.rcNormalPosition.bottom = wndpl.rcNormalPosition.top + height;
            SetWindowPlacement(button, &wndpl);
            // Re-select tab
            PropSheet_SetCurSel(g_cfgwnd, page, 0);
            // Invalidate region
            GetWindowPlacement(g_cfgwnd, &wndpl);
            InvalidateRect(g_cfgwnd, &wndpl.rcNormalPosition, TRUE);
        }
    }
}
/////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK PropSheetWinProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    LRESULT ret = DefSubclassProc(hwnd, msg, wParam, lParam);
    if (msg == WM_NCHITTEST && (ret == HTBOTTOM || ret == HTBOTTOMLEFT
       || ret == HTBOTTOMRIGHT || ret == HTLEFT || ret == HTTOPLEFT
       || ret == HTTOPRIGHT || ret == HTRIGHT || ret == HTTOP)) {

        ret = HTCAPTION;

    } else if (msg == WM_UPDATESETTINGS) {
        UpdateStrings();
        HWND page = PropSheet_GetCurrentPageHwnd(g_cfgwnd);
        SendMessage(page, WM_INITDIALOG, 0, 0);
        NMHDR pnmh = { g_cfgwnd, 0, PSN_SETACTIVE };
        SendMessage(page, WM_NOTIFY, 0, (LPARAM) & pnmh);
    }
    return ret;
}
/////////////////////////////////////////////////////////////////////////////
BOOL CALLBACK PropSheetProc(HWND hwnd, UINT msg, LPARAM lParam)
{
      if (msg == PSCB_INITIALIZED) {
        g_cfgwnd = hwnd;
        SetWindowSubclass(g_cfgwnd, PropSheetWinProc, 0, 0);
        UpdateStrings();

        // Set new icon specifically for the taskbar and Alt+Tab, without changing window icon
        HICON taskbar_icon = LoadImage(g_hinst, L"taskbar_icon", IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
        SendMessage(g_cfgwnd, WM_SETICON, ICON_BIG, (LPARAM) taskbar_icon);

        // OK button replaces Cancel button
        SendMessage(g_cfgwnd, PSM_CANCELTOCLOSE, 0, 0);
        HWND cancel = GetDlgItem(g_cfgwnd, IDCANCEL);
        HWND ok = GetDlgItem(g_cfgwnd, IDOK);
        Button_Enable(cancel, TRUE); // Re-enable to enable escape key
        WINDOWPLACEMENT wndpl = { sizeof(WINDOWPLACEMENT) };
        GetWindowPlacement(cancel, &wndpl);
        SetWindowPlacement(ok, &wndpl);
        ShowWindow(cancel, SW_HIDE);

        HWND apply = GetDlgItem(g_cfgwnd, IDAPPLY);
        Button_SetText(apply, L""); // Remove text to remove it's shortcut (Alt+A in English)
    }
    return TRUE;
}
/////////////////////////////////////////////////////////////////////////////
static DWORD IsUACEnabled()
{
    DWORD uac_enabled = 0;
    if (elevated) {
        DWORD len = sizeof(uac_enabled);
        HKEY key;
        RegOpenKeyEx(
            HKEY_LOCAL_MACHINE
          , L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System"
          , 0, KEY_QUERY_VALUE, &key);

        RegQueryValueEx(key, L"EnableLUA", NULL, NULL, (LPBYTE) &uac_enabled, &len);
        RegCloseKey(key);
    }
    return uac_enabled;
}
/////////////////////////////////////////////////////////////////////////////
INT_PTR CALLBACK GeneralPageDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int updatestrings = 0;
    if (msg == WM_INITDIALOG) {
        int ret;
        ret = GetPrivateProfileInt(L"General", L"AutoFocus", 0, inipath);
        Button_SetCheck(GetDlgItem(hwnd, IDC_AUTOFOCUS), ret? BST_CHECKED : BST_UNCHECKED);

        ret = GetPrivateProfileInt(L"General", L"Aero", 1, inipath);
        Button_SetCheck(GetDlgItem(hwnd, IDC_AERO), ret? BST_CHECKED : BST_UNCHECKED);

        ret = GetPrivateProfileInt(L"General", L"InactiveScroll", 0, inipath);
        Button_SetCheck(GetDlgItem(hwnd, IDC_INACTIVESCROLL), ret? BST_CHECKED : BST_UNCHECKED);
        if(WIN10) Button_Enable(GetDlgItem(hwnd, IDC_INACTIVESCROLL), ret);

        ret=GetPrivateProfileInt(L"General", L"MDI", 1, inipath);
        Button_SetCheck(GetDlgItem(hwnd, IDC_MDI), ret? BST_CHECKED : BST_UNCHECKED);

        ret=GetPrivateProfileInt(L"Performance", L"FullWin", 1, inipath);
        Button_SetCheck(GetDlgItem(hwnd, IDC_FULLWIN), ret? BST_CHECKED : BST_UNCHECKED);
        if(HaveDWM()) Button_Enable(GetDlgItem(hwnd, IDC_FULLWIN), !ret);

        ret=GetPrivateProfileInt(L"Advanced", L"ResizeAll", 1, inipath);
        Button_SetCheck(GetDlgItem(hwnd, IDC_RESIZEALL), ret? BST_CHECKED : BST_UNCHECKED);

        ret=GetPrivateProfileInt(L"General", L"ResizeCenter", 1, inipath);
        ret = ret==1? IDC_RZCENTER_NORM: ret==2? IDC_RZCENTER_MOVE: IDC_RZCENTER_BR;
        CheckRadioButton(hwnd, IDC_RZCENTER_NORM, IDC_RZCENTER_MOVE, ret);

        HWND control = GetDlgItem(hwnd, IDC_LANGUAGE);
        ComboBox_ResetContent(control);
        ComboBox_Enable(control, TRUE);
        int i;
        for (i = 0; i < nlanguages; i++) {
            ComboBox_AddString(control, langinfo[i].lang);
            if (langinfo[i].code && !wcsicmp(l10n->code, langinfo[i].code) ) {
                ComboBox_SetCurSel(control, i);
            }
        }

        Button_Enable(GetDlgItem(hwnd, IDC_ELEVATE), VISTA && !elevated);

    } else if (msg == WM_COMMAND) {
        int id = LOWORD(wParam);
        int event = HIWORD(wParam);
        HWND control = GetDlgItem(hwnd, id);
        int val = Button_GetCheck(control);
        wchar_t txt[10];

        if (id == IDC_AUTOFOCUS) {
            WritePrivateProfileString(L"General",    L"AutoFocus", _itow(val, txt, 10), inipath);
        } else if (id == IDC_AUTOSNAP && event == CBN_SELCHANGE) {
            val = ComboBox_GetCurSel(control);
            WritePrivateProfileString(L"General",    L"AutoSnap", _itow(val, txt, 10), inipath);
        } else if (id == IDC_AERO) {
            WritePrivateProfileString(L"General",    L"Aero", _itow(val, txt, 10), inipath);
        } else if (id == IDC_INACTIVESCROLL) {
            WritePrivateProfileString(L"General",    L"InactiveScroll", _itow(val, txt, 10), inipath);
        } else if (id == IDC_MDI) {
            WritePrivateProfileString(L"General",    L"MDI", _itow(val, txt, 10), inipath);
        } else if (id == IDC_FULLWIN) {
            WritePrivateProfileString(L"Performance",L"FullWin", _itow(val, txt, 10), inipath);
        } else if (id == IDC_RESIZEALL) {
            WritePrivateProfileString(L"Advanced",   L"ResizeAll", _itow(val, txt, 10), inipath);

        } else if (id == IDC_RZCENTER_NORM) {
            CheckRadioButton(hwnd, IDC_RZCENTER_NORM, IDC_RZCENTER_MOVE, IDC_RZCENTER_NORM);
            WritePrivateProfileString(L"General",    L"ResizeCenter", L"1", inipath);
        } else if (id == IDC_RZCENTER_BR) {
            CheckRadioButton(hwnd, IDC_RZCENTER_NORM, IDC_RZCENTER_MOVE, IDC_RZCENTER_BR);
            WritePrivateProfileString(L"General",    L"ResizeCenter", L"0", inipath);
        } else if (id == IDC_RZCENTER_MOVE) {
            CheckRadioButton(hwnd, IDC_RZCENTER_NORM, IDC_RZCENTER_MOVE, IDC_RZCENTER_MOVE);
            WritePrivateProfileString(L"General",    L"ResizeCenter", L"2", inipath);

        } else if (id == IDC_LANGUAGE && event == CBN_SELCHANGE) {
            int i = ComboBox_GetCurSel(control);
            if (i == nlanguages) {
                ComboBox_SetCurSel(control, i);
            } else {
                LoadTranslation(langinfo[i].fn);
                WritePrivateProfileString(L"General", L"Language", l10n->code, inipath);
                updatestrings = 1;
                UpdateStrings();
            }
        } else if (id == IDC_AUTOSTART) {
            SetAutostart(val, 0, 0);
            Button_Enable(GetDlgItem(hwnd, IDC_AUTOSTART_HIDE), val);
            Button_Enable(GetDlgItem(hwnd, IDC_AUTOSTART_ELEVATE), val && VISTA);
            if (!val) {
                Button_SetCheck(GetDlgItem(hwnd, IDC_AUTOSTART_HIDE), BST_UNCHECKED);
                Button_SetCheck(GetDlgItem(hwnd, IDC_AUTOSTART_ELEVATE), BST_UNCHECKED);
            }
        } else if (id == IDC_AUTOSTART_HIDE) {
            int elevate = Button_GetCheck(GetDlgItem(hwnd, IDC_AUTOSTART_ELEVATE));
            SetAutostart(1, val, elevate);
        } else if (id == IDC_AUTOSTART_ELEVATE) {
            int hide = Button_GetCheck(GetDlgItem(hwnd, IDC_AUTOSTART_HIDE));
            SetAutostart(1, hide, val);
            if (val && IsUACEnabled()) {
                MessageBox(NULL, l10n->general_autostart_elevate_tip, APP_NAME, MB_ICONINFORMATION | MB_OK);
            }
        } else if (id == IDC_ELEVATE) {
            return ElevateNow(1);
        }
        UpdateSettings();
    } else if (msg == WM_NOTIFY) {
        LPNMHDR pnmh = (LPNMHDR) lParam;
        if (pnmh->code == PSN_SETACTIVE) {
            updatestrings = 1;

            // Autostart
            int autostart = 0, hidden = 0, elevated = 0;
            CheckAutostart(&autostart, &hidden, &elevated);
            Button_SetCheck(GetDlgItem(hwnd, IDC_AUTOSTART), autostart ? BST_CHECKED : BST_UNCHECKED);
            Button_SetCheck(GetDlgItem(hwnd, IDC_AUTOSTART_HIDE), hidden ? BST_CHECKED : BST_UNCHECKED);
            Button_SetCheck(GetDlgItem(hwnd, IDC_AUTOSTART_ELEVATE), elevated ? BST_CHECKED : BST_UNCHECKED);
            Button_Enable(GetDlgItem(hwnd, IDC_AUTOSTART_HIDE), autostart);
            Button_Enable(GetDlgItem(hwnd, IDC_AUTOSTART_ELEVATE), autostart && VISTA);

        }
    }
    if (updatestrings) {
        // Update text
        SetDlgItemText(hwnd, IDC_GENERAL_BOX,       l10n->general_box);
        SetDlgItemText(hwnd, IDC_AUTOFOCUS,         l10n->general_autofocus);
        SetDlgItemText(hwnd, IDC_AERO,              l10n->general_aero);
        SetDlgItemText(hwnd, IDC_INACTIVESCROLL,    l10n->general_inactivescroll);
        SetDlgItemText(hwnd, IDC_MDI,               l10n->general_mdi);
        SetDlgItemText(hwnd, IDC_AUTOSNAP_HEADER,   l10n->general_autosnap);
        SetDlgItemText(hwnd, IDC_LANGUAGE_HEADER,   l10n->general_language);

        SetDlgItemText(hwnd, IDC_FULLWIN,           l10n->general_fullwin);
        SetDlgItemText(hwnd, IDC_RESIZEALL,         l10n->general_resizeall);
        SetDlgItemText(hwnd, IDC_RESIZECENTER,      l10n->general_resizecenter);
        SetDlgItemText(hwnd, IDC_RZCENTER_NORM,     l10n->general_resizecenter_norm);
        SetDlgItemText(hwnd, IDC_RZCENTER_BR,       l10n->general_resizecenter_br);
        SetDlgItemText(hwnd, IDC_RZCENTER_MOVE,     l10n->general_resizecenter_move);

        SetDlgItemText(hwnd, IDC_AUTOSTART_BOX,     l10n->general_autostart_box);
        SetDlgItemText(hwnd, IDC_AUTOSTART,         l10n->general_autostart);
        SetDlgItemText(hwnd, IDC_AUTOSTART_HIDE,    l10n->general_autostart_hide);
        SetDlgItemText(hwnd, IDC_AUTOSTART_ELEVATE, l10n->general_autostart_elevate);
        SetDlgItemText(hwnd, IDC_ELEVATE, (elevated? l10n->general_elevated: l10n->general_elevate));
        SetDlgItemText(hwnd, IDC_AUTOSAVE, l10n->general_autosave);

        // AutoSnap
        HWND control = GetDlgItem(hwnd, IDC_AUTOSNAP);
        ComboBox_ResetContent(control);
        ComboBox_AddString(control, l10n->general_autosnap0);
        ComboBox_AddString(control, l10n->general_autosnap1);
        ComboBox_AddString(control, l10n->general_autosnap2);
        ComboBox_AddString(control, l10n->general_autosnap3);
        wchar_t txt[10];
        GetPrivateProfileString(L"General", L"AutoSnap", L"0", txt, ARR_SZ(txt), inipath);
        ComboBox_SetCurSel(control, _wtoi(txt));

        // Language
        control = GetDlgItem(hwnd, IDC_LANGUAGE);
        ComboBox_DeleteString(control, nlanguages);
    }
    return FALSE;
}

/////////////////////////////////////////////////////////////////////////////
INT_PTR CALLBACK MousePageDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Mouse actions
    struct {
        int control;
        wchar_t *option;
    } mouse_buttons[] = {
        {
        IDC_LMB, L"LMB"}, {
        IDC_MMB, L"MMB"}, {
        IDC_RMB, L"RMB"}, {
        IDC_MB4, L"MB4"}, {
    IDC_MB5, L"MB5"},};

    struct action {
        wchar_t *action;
        wchar_t *l10n;
    };
    struct action mouse_actions[] = {
        {L"Move",        l10n->input_actions_move},
        {L"Resize",      l10n->input_actions_resize},
        {L"Close",       l10n->input_actions_close},
        {L"Kill",        l10n->input_actions_kill},
        {L"Minimize",    l10n->input_actions_minimize},
        {L"Maximize",    l10n->input_actions_maximize},
        {L"Lower",       l10n->input_actions_lower},
        {L"Roll",        l10n->input_actions_roll},
        {L"AlwaysOnTop", l10n->input_actions_alwaysontop},
        {L"Borderless",  l10n->input_actions_borderless},
        {L"Center",      l10n->input_actions_center},
        {L"Menu",        l10n->input_actions_menu},
        {L"Nothing",     l10n->input_actions_nothing},
    };

    // Scroll
    struct action scroll_actions[] = {
        {L"AltTab",       l10n->input_actions_alttab},
        {L"Volume",       l10n->input_actions_volume},
        {L"Transparency", l10n->input_actions_transparency},
        {L"Lower",        l10n->input_actions_lower},
        {L"Roll",         l10n->input_actions_roll},
        {L"Maximize",     l10n->input_actions_maximize},
        {L"Nothing",      l10n->input_actions_nothing},
    };

    // Hotkeys
    struct {
        unsigned control;
        unsigned vkey;
    } hotclicks [] = {
        { IDC_MMB_HC, 0x04 },
        { IDC_MB4_HC, 0x05 },
        { IDC_MB5_HC, 0X06 },
    };

    if (msg == WM_INITDIALOG) {
        // LowerWithMMB
        int ret = GetPrivateProfileInt(L"Input", L"LowerWithMMB", 0, inipath);
        Button_SetCheck(GetDlgItem(hwnd, IDC_LOWERWITHMMB), ret? BST_CHECKED: BST_UNCHECKED);

        // Roll/Unroll
        ret = GetPrivateProfileInt(L"Input",  L"RollWithTBScroll", 0, inipath);
        Button_SetCheck(GetDlgItem(hwnd, IDC_ROLLWITHTBSCROLL), ret? BST_CHECKED: BST_UNCHECKED);

        // Hotkeys
        size_t i;
        unsigned temp;
        int numread;
        wchar_t txt[50];
        GetPrivateProfileString(L"Input", L"Hotclicks", L"", txt, ARR_SZ(txt), inipath);
        wchar_t *pos = txt;
        while (*pos != '\0' && swscanf(pos, L"%02X%n", &temp, &numread) != EOF) {
            pos += numread;
            // What key was that?
            for (i = 0; i < ARR_SZ(hotclicks); i++) {
                if (temp == hotclicks[i].vkey) {
                    Button_SetCheck(GetDlgItem(hwnd, hotclicks[i].control), BST_CHECKED);
                    break;
                }
            }
        }
    } else if (msg == WM_COMMAND) {
        int id = LOWORD(wParam);
        int event = HIWORD(wParam);
        wchar_t txt[50] = L"";
        size_t i;
        if (event == CBN_SELCHANGE) {
            HWND control = GetDlgItem(hwnd, id);
            // Mouse actions
            for (i = 0; i < ARR_SZ(mouse_buttons); i++) {
                if (id == mouse_buttons[i].control) {
                    int j = ComboBox_GetCurSel(control);
                    WritePrivateProfileString(L"Input", mouse_buttons[i].option, mouse_actions[j].action, inipath);
                    break;
                }
            }
            // Scroll
            if (id == IDC_SCROLL) {
                int j = ComboBox_GetCurSel(control);
                WritePrivateProfileString(L"Input", L"Scroll", scroll_actions[j].action, inipath);
            }
        } else if (LOWORD(wParam) == IDC_LOWERWITHMMB) {
            int val = Button_GetCheck(GetDlgItem(hwnd, IDC_LOWERWITHMMB));
            WritePrivateProfileString(L"Input", L"LowerWithMMB", _itow(val, txt, 10), inipath);
        } else if (id == IDC_ROLLWITHTBSCROLL) {
            int val = Button_GetCheck(GetDlgItem(hwnd, IDC_ROLLWITHTBSCROLL));
            WritePrivateProfileString(L"Input",   L"RollWithTBScroll", _itow(val, txt, 10), inipath);
        } else {
            // hotclicks
            unsigned vkey = 0;
            for (i = 0; i < ARR_SZ(hotclicks); i++) {
                if (wParam == hotclicks[i].control) {
                    vkey = hotclicks[i].vkey;
                    break;
                }
            }
            if (!vkey)
                return FALSE;

            wchar_t keys[50];
            GetPrivateProfileString(L"Input", L"Hotclicks", L"", keys, ARR_SZ(keys), inipath);
            int add = Button_GetCheck(GetDlgItem(hwnd, wParam));
            if (add) {
                if (*keys != '\0') {
                    wcscat(keys, L" ");
                }
                swprintf(txt, ARR_SZ(txt), L"%s%02X", keys, vkey);
            } else {
                unsigned int temp;
                int numread;
                wchar_t *pos = keys;
                while (*pos != '\0' && swscanf(pos, L"%02X%n", &temp, &numread) != EOF) {
                    if (temp == vkey) {
                        wcsncpy(txt, keys, pos - keys);
                        wcscat(txt, pos + numread);
                        break;
                    }
                    pos += numread;
                }
            }
            WritePrivateProfileString(L"Input", L"Hotclicks", txt, inipath);
        }
        UpdateSettings();
    } else if (msg == WM_NOTIFY) {
        LPNMHDR pnmh = (LPNMHDR) lParam;
        if (pnmh->code == PSN_SETACTIVE) {
            wchar_t txt[50];
            size_t i, j, sel;

            // Mouse actions
            for (i = 0; i < ARR_SZ(mouse_buttons); i++) {
                HWND control = GetDlgItem(hwnd, mouse_buttons[i].control);
                ComboBox_ResetContent(control);
                GetPrivateProfileString(L"Input", mouse_buttons[i].option, L"Nothing", txt, ARR_SZ(txt), inipath);
                sel = ARR_SZ(mouse_actions) - 1;
                for (j = 0; j < ARR_SZ(mouse_actions); j++) {
                    wchar_t action_name[256];
                    wcscpy_noaccel(action_name, mouse_actions[j].l10n, ARR_SZ(action_name));
                    ComboBox_AddString(control, action_name);
                    if (!wcscmp(txt, mouse_actions[j].action)) {
                        sel = j;
                    }
                }
                ComboBox_SetCurSel(control, sel);
            }

            // Scroll
            HWND control = GetDlgItem(hwnd, IDC_SCROLL);
            ComboBox_ResetContent(control);
            GetPrivateProfileString(L"Input", L"Scroll", L"Nothing", txt, ARR_SZ(txt), inipath);
            sel = ARR_SZ(scroll_actions) - 1;
            for (j = 0; j < ARR_SZ(scroll_actions); j++) {
                wchar_t action_name[256];
                wcscpy_noaccel(action_name, scroll_actions[j].l10n, ARR_SZ(action_name));
                ComboBox_AddString(control, action_name);
                if (!wcscmp(txt, scroll_actions[j].action)) {
                    sel = j;
                }
            }
            ComboBox_SetCurSel(control, sel);

            // Update text
            SetDlgItemText(hwnd, IDC_MOUSE_BOX,       l10n->input_mouse_box);
            SetDlgItemText(hwnd, IDC_LMB_HEADER,      l10n->input_mouse_lmb);
            SetDlgItemText(hwnd, IDC_MMB_HEADER,      l10n->input_mouse_mmb);
            SetDlgItemText(hwnd, IDC_RMB_HEADER,      l10n->input_mouse_rmb);
            SetDlgItemText(hwnd, IDC_MB4_HEADER,      l10n->input_mouse_mb4);
            SetDlgItemText(hwnd, IDC_MB5_HEADER,      l10n->input_mouse_mb5);
            SetDlgItemText(hwnd, IDC_SCROLL_HEADER,   l10n->input_mouse_scroll);

            SetDlgItemText(hwnd, IDC_LOWERWITHMMB,    l10n->input_mouse_lowerwithmmb);
            SetDlgItemText(hwnd, IDC_ROLLWITHTBSCROLL,l10n->input_mouse_rollwithtbscroll);

            SetDlgItemText(hwnd, IDC_HOTCLICKS_BOX,   l10n->input_hotclicks_box);
            SetDlgItemText(hwnd, IDC_HOTCLICKS_MORE,  l10n->input_hotclicks_more);
            SetDlgItemText(hwnd, IDC_MMB_HC,          l10n->input_mouse_mmb_hc);
            SetDlgItemText(hwnd, IDC_MB4_HC,          l10n->input_mouse_mb4_hc);
            SetDlgItemText(hwnd, IDC_MB5_HC,          l10n->input_mouse_mb5_hc);
        }
    }

    return FALSE;
}
/////////////////////////////////////////////////////////////////////////////
INT_PTR CALLBACK KeyboardPageDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{

    // Hotkeys
    struct {
        unsigned control;
        unsigned vkey;
    } hotkeys[] = {
        { IDC_LEFTALT,     VK_LMENU    },
        { IDC_RIGHTALT,    VK_RMENU    },
        { IDC_LEFTWINKEY,  VK_LWIN     },
        { IDC_RIGHTWINKEY, VK_RWIN     },
        { IDC_LEFTCTRL,    VK_LCONTROL },
        { IDC_RIGHTCTRL,   VK_RCONTROL },

    };
    struct action {
        wchar_t *action;
        wchar_t *l10n;
    };
    struct action kb_actions[] = {
        {L"Move",        l10n->input_actions_move},
        {L"Resize",      l10n->input_actions_resize},
        {L"Close",       l10n->input_actions_close},
        {L"Minimize",    l10n->input_actions_minimize},
        {L"Maximize",    l10n->input_actions_maximize},
        {L"Lower",       l10n->input_actions_lower},
        {L"Roll",        l10n->input_actions_roll},
        {L"AlwaysOnTop", l10n->input_actions_alwaysontop},
        {L"Borderless",  l10n->input_actions_borderless},
        {L"Center",      l10n->input_actions_center},
        {L"Menu",        l10n->input_actions_menu},
        {L"Nothing",     l10n->input_actions_nothing},
    };
    // Hotkeys
    struct {
        wchar_t *action;
        wchar_t *l10n;
    } togglekeys[] = {
        { L"",   l10n->input_actions_nothing},
        { L"A4", l10n->input_hotkeys_leftalt},
        { L"A5", l10n->input_hotkeys_rightalt},
        { L"5B", l10n->input_hotkeys_leftwinkey},
        { L"5C", l10n->input_hotkeys_rightwinkey},
        { L"A2", l10n->input_hotkeys_leftctrl},
        { L"A3", l10n->input_hotkeys_rightctrl},
        { L"A0", l10n->input_hotkeys_leftshift},
        { L"A1", l10n->input_hotkeys_rightshift},
    };

    if (msg == WM_INITDIALOG) {
        // Agressive Pause
        int ret = GetPrivateProfileInt(L"Input", L"AggressivePause", 0, inipath);
        Button_SetCheck(GetDlgItem(hwnd, IDC_AGGRESSIVEPAUSE), ret? BST_CHECKED: BST_UNCHECKED);
        Button_Enable(GetDlgItem(hwnd, IDC_AGGRESSIVEPAUSE), HaveProc("NTDLL.DLL", "NtResumeProcess"));

        ret = GetPrivateProfileInt(L"Input", L"AggressiveKill", 0, inipath);
        Button_SetCheck(GetDlgItem(hwnd, IDC_AGGRESSIVEKILL), ret? BST_CHECKED: BST_UNCHECKED);

        ret = GetPrivateProfileInt(L"Input", L"KeyCombo", 0, inipath);
        Button_SetCheck(GetDlgItem(hwnd, IDC_KEYCOMBO), ret? BST_CHECKED: BST_UNCHECKED);

        // Hotkeys
        size_t i;
        unsigned temp;
        int numread;
        wchar_t txt[50];
        GetPrivateProfileString(L"Input", L"Hotkeys", L"A4 A5", txt, ARR_SZ(txt), inipath);
        wchar_t *pos = txt;
        while (*pos != '\0' && swscanf(pos, L"%02X%n", &temp, &numread) != EOF) {
            pos += numread;
            // What key was that?
            for (i = 0; i < ARR_SZ(hotkeys); i++) {
                if (temp == hotkeys[i].vkey) {
                    Button_SetCheck(GetDlgItem(hwnd, hotkeys[i].control), BST_CHECKED);
                    break;
                }
            }
        }
    } else if (msg == WM_COMMAND) {
        int id = LOWORD(wParam);
        int event = HIWORD(wParam);
        wchar_t txt[50] = L"";
        size_t i;

        HWND control = GetDlgItem(hwnd, id);
        int val = Button_GetCheck(control);

        if (id == IDC_AGGRESSIVEPAUSE) {
            WritePrivateProfileString(L"Input", L"AggressivePause", _itow(val, txt, 10), inipath);
        } else if (id == IDC_AGGRESSIVEKILL) {
            WritePrivateProfileString(L"Input", L"AggressiveKill", _itow(val, txt, 10), inipath);
        } else if (id == IDC_KEYCOMBO) {
            WritePrivateProfileString(L"Input", L"KeyCombo", _itow(val, txt, 10), inipath);

        } else if (event == CBN_SELCHANGE) {
            int j = ComboBox_GetCurSel(control);
            if (id == IDC_GRABWITHALT) {
                WritePrivateProfileString(L"Input", L"GrabWithAlt", kb_actions[j].action, inipath);
            } else if (id == IDC_TOGGLERZMVKEY) {
                WritePrivateProfileString(L"Input", L"ToggleRzMvKey", togglekeys[j].action, inipath);
            }
        } else {
            // Hotkeys
            unsigned vkey = 0;
            for (i = 0; i < ARR_SZ(hotkeys); i++) {
                if (wParam == hotkeys[i].control) {
                    vkey = hotkeys[i].vkey;
                    break;
                }
            }
            if (!vkey)
                return FALSE;

            wchar_t keys[50];
            GetPrivateProfileString(L"Input", L"Hotkeys", L"", keys, ARR_SZ(keys), inipath);
            int add = Button_GetCheck(GetDlgItem(hwnd, wParam));
            if (add) {
                if (*keys != '\0') {
                    wcscat(keys, L" ");
                }
                swprintf(txt, ARR_SZ(txt), L"%s%02X", keys, vkey);
            } else {
                unsigned int temp;
                int numread;
                wchar_t *pos = keys;
                while (*pos != '\0' && swscanf(pos, L"%02X%n", &temp, &numread) != EOF) {
                    if (temp == vkey) {
                        wcsncpy(txt, keys, pos - keys);
                        wcscat(txt, pos + numread);
                        break;
                    }
                    pos += numread;
                }
            }
            WritePrivateProfileString(L"Input", L"Hotkeys", txt, inipath);
        }
        UpdateSettings();
    } else if (msg == WM_NOTIFY) {
        LPNMHDR pnmh = (LPNMHDR) lParam;
        if (pnmh->code == PSN_SETACTIVE) {
            // GrabWithAlt
            wchar_t txt[32];
            HWND control = GetDlgItem(hwnd, IDC_GRABWITHALT);
            ComboBox_ResetContent(control);
            GetPrivateProfileString(L"Input", L"GrabWithAlt", L"Nothing", txt, ARR_SZ(txt), inipath);
            int sel = ARR_SZ(kb_actions) - 1;
            unsigned j;
            for (j = 0; j < ARR_SZ(kb_actions); j++) {
                wchar_t action_name[256];
                wcscpy_noaccel(action_name, kb_actions[j].l10n, ARR_SZ(action_name));
                ComboBox_AddString(control, action_name);
                if (!wcscmp(txt, kb_actions[j].action)) {
                    sel = j;
                }
            }
            ComboBox_SetCurSel(control, sel);

            // ToggleRzMvKey init
            control = GetDlgItem(hwnd, IDC_TOGGLERZMVKEY);
            ComboBox_ResetContent(control);
            GetPrivateProfileString(L"Input", L"ToggleRzMvKey", L"", txt, ARR_SZ(txt), inipath);
            sel = ARR_SZ(togglekeys) - 1;
            for (j = 0; j < ARR_SZ(togglekeys); j++) {
                wchar_t key_name[256];
                wcscpy_noaccel(key_name, togglekeys[j].l10n, ARR_SZ(key_name));
                ComboBox_AddString(control, key_name);
                if (!wcscmp(txt, togglekeys[j].action)) {
                    sel = j;
                }
            }
            ComboBox_SetCurSel(control, sel);

            // Update text
            SetDlgItemText(hwnd, IDC_KEYBOARD_BOX,    l10n->tab_keyboard);
            SetDlgItemText(hwnd, IDC_AGGRESSIVEPAUSE, l10n->input_aggressive_pause);
            SetDlgItemText(hwnd, IDC_AGGRESSIVEKILL,  l10n->input_aggressive_kill);
            SetDlgItemText(hwnd, IDC_HOTKEYS_BOX,     l10n->input_hotkeys_box);
            SetDlgItemText(hwnd, IDC_TOGGLERZMVKEY_H, l10n->input_hotkeys_togglerzmvkey);
            SetDlgItemText(hwnd, IDC_LEFTALT,         l10n->input_hotkeys_leftalt);
            SetDlgItemText(hwnd, IDC_RIGHTALT,        l10n->input_hotkeys_rightalt);
            SetDlgItemText(hwnd, IDC_LEFTWINKEY,      l10n->input_hotkeys_leftwinkey);
            SetDlgItemText(hwnd, IDC_RIGHTWINKEY,     l10n->input_hotkeys_rightwinkey);
            SetDlgItemText(hwnd, IDC_LEFTCTRL,        l10n->input_hotkeys_leftctrl);
            SetDlgItemText(hwnd, IDC_RIGHTCTRL,       l10n->input_hotkeys_rightctrl);
            SetDlgItemText(hwnd, IDC_HOTKEYS_MORE,    l10n->input_hotkeys_more);
            SetDlgItemText(hwnd, IDC_KEYCOMBO,        l10n->input_keycombo);
            SetDlgItemText(hwnd, IDC_GRABWITHALT_H,   l10n->input_grabwithalt);
        }
    }

    return FALSE;
}

/////////////////////////////////////////////////////////////////////////////
INT_PTR CALLBACK BlacklistPageDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_INITDIALOG) {

        wchar_t txt[2048];
        BOOL haveProcessBL = HaveProc("PSAPI.DLL", "GetModuleFileNameExW");
        GetPrivateProfileString(L"Blacklist", L"Processes", L"", txt, ARR_SZ(txt), inipath);
        SetDlgItemText(hwnd, IDC_PROCESSBLACKLIST, txt);
        Button_Enable(GetDlgItem(hwnd, IDC_PROCESSBLACKLIST), haveProcessBL);

        GetPrivateProfileString(L"Blacklist", L"Windows", L"", txt, ARR_SZ(txt), inipath);
        SetDlgItemText(hwnd, IDC_BLACKLIST, txt);

        GetPrivateProfileString(L"Blacklist", L"Scroll", L"", txt, ARR_SZ(txt), inipath);
        SetDlgItemText(hwnd, IDC_SCROLLLIST, txt);

        GetPrivateProfileString(L"Blacklist", L"MDIs", L"", txt, ARR_SZ(txt), inipath);
        SetDlgItemText(hwnd, IDC_MDIS, txt);
        Button_Enable(GetDlgItem(hwnd, IDC_MDIS), GetPrivateProfileInt(L"General", L"MDI", 1, inipath));

        GetPrivateProfileString(L"Blacklist", L"Pause", L"", txt, ARR_SZ(txt), inipath);
        SetDlgItemText(hwnd, IDC_PAUSEBL, txt);
        Button_Enable(GetDlgItem(hwnd, IDC_PAUSEBL), haveProcessBL);

    } else if (msg == WM_COMMAND) {
        wchar_t txt[2048];
        int control = LOWORD(wParam);

        if (HIWORD(wParam) == EN_KILLFOCUS) {
            Edit_GetText(GetDlgItem(hwnd, control), txt, ARR_SZ(txt));
            if (control == IDC_PROCESSBLACKLIST) {
                WritePrivateProfileString(L"Blacklist", L"Processes", txt, inipath);
            } else if (control == IDC_BLACKLIST) {
                WritePrivateProfileString(L"Blacklist", L"Windows", txt, inipath);
            } else if (control == IDC_SCROLLLIST) {
                WritePrivateProfileString(L"Blacklist", L"Scroll", txt, inipath);
            } else if (control == IDC_MDIS) {
                WritePrivateProfileString(L"Blacklist", L"MDIs", txt, inipath);
            } else if (control == IDC_PAUSEBL) {
                WritePrivateProfileString(L"Blacklist", L"Pause", txt, inipath);
            }
            UpdateSettings();
        } else if (HIWORD(wParam) == STN_CLICKED && control == IDC_FINDWINDOW) {
            // Get size of workspace
            int left=0, top=0, width, height;
            if(GetSystemMetrics(SM_CMONITORS) >= 1) {
                left   = GetSystemMetrics(SM_XVIRTUALSCREEN);
                top    = GetSystemMetrics(SM_YVIRTUALSCREEN);
                width  = GetSystemMetrics(SM_CXVIRTUALSCREEN);
                height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
             } else { // NT4...
                 width = GetSystemMetrics(SM_CXFULLSCREEN)+32;
                 height= GetSystemMetrics(SM_CYFULLSCREEN)+256;
             }

            // Create window
            WNDCLASSEX wnd = { sizeof(WNDCLASSEX), 0, CursorProc, 0, 0, g_hinst, NULL, NULL
                             , (HBRUSH) (COLOR_WINDOW + 1), NULL, APP_NAME"-find", NULL };
            wnd.hCursor = LoadCursor(g_hinst, MAKEINTRESOURCE(IDI_FIND));
            RegisterClassEx(&wnd);
            HWND findhwnd = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_TRANSPARENT
                           , wnd.lpszClassName, NULL, WS_POPUP, left, top, width, height, NULL, NULL, g_hinst, NULL);
            ShowWindowAsync(findhwnd, SW_SHOWNA);

            // Hide icon
            ShowWindowAsync(GetDlgItem(hwnd, IDC_FINDWINDOW), SW_HIDE);
        }

    } else if (msg == WM_NOTIFY) {
        LPNMHDR pnmh = (LPNMHDR) lParam;
        if (pnmh->code == PSN_SETACTIVE) {
            // Update text
            SetDlgItemText(hwnd, IDC_BLACKLIST_BOX          , l10n->blacklist_box);
            SetDlgItemText(hwnd, IDC_PROCESSBLACKLIST_HEADER, l10n->blacklist_processblacklist);
            SetDlgItemText(hwnd, IDC_BLACKLIST_HEADER       , l10n->blacklist_blacklist);
            SetDlgItemText(hwnd, IDC_SCROLLLIST_HEADER      , l10n->blacklist_scrolllist);
            SetDlgItemText(hwnd, IDC_MDISBL_HEADER          , l10n->blacklist_mdis);
            SetDlgItemText(hwnd, IDC_PAUSEBL_HEADER         , l10n->blacklist_pause);
            SetDlgItemText(hwnd, IDC_FINDWINDOW_BOX         , l10n->blacklist_findwindow_box);
        }
    }

    return FALSE;
}
/////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK CursorProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_LBUTTONDOWN || msg == WM_MBUTTONDOWN || msg == WM_RBUTTONDOWN) {
        ShowWindow(hwnd, SW_HIDE);
        HWND page = PropSheet_GetCurrentPageHwnd(g_cfgwnd);

        if (msg == WM_LBUTTONDOWN) {
            POINT pt;
            GetCursorPos(&pt);
            HWND window = WindowFromPoint(pt);
            window = GetAncestor(window, GA_ROOT);

            wchar_t title[256], classname[256];
            GetWindowText(window, title, ARR_SZ(title));
            GetClassName(window, classname, ARR_SZ(classname));

            wchar_t txt[1000];
            swprintf(txt, ARR_SZ(txt), L"%s|%s", title, classname);
            SetDlgItemText(page, IDC_NEWRULE, txt);

            if(GetWindowProgName(window, txt, ARR_SZ(txt))) {
                SetDlgItemText(page, IDC_NEWPROGNAME, txt);
            }
        }
        // Show icon again
        ShowWindowAsync(GetDlgItem(page, IDC_FINDWINDOW), SW_SHOW);

        DestroyWindow(hwnd);
        UnregisterClass(APP_NAME"-find", g_hinst);
    } else if (wParam && (msg == WM_PAINT || msg == WM_ERASEBKGND)){
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
/////////////////////////////////////////////////////////////////////////////
INT_PTR CALLBACK AboutPageDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NOTIFY) {
        LPNMHDR pnmh = (LPNMHDR) lParam;
        if (pnmh->code == PSN_SETACTIVE) {
            // Update text
            SetDlgItemText(hwnd, IDC_ABOUT_BOX, l10n->about_box);
            SetDlgItemText(hwnd, IDC_VERSION, l10n->about_version);
            SetDlgItemText(hwnd, IDC_URL, L"https://github.com/RamonUnch/AltDrag");
            SetDlgItemText(hwnd, IDC_AUTHOR, l10n->about_author);
            SetDlgItemText(hwnd, IDC_LICENSE, l10n->about_license);
            SetDlgItemText(hwnd, IDC_TRANSLATIONS_BOX, l10n->about_translation_credit);

            wchar_t txt[1024] = L"";
            int i;
            for (i = 0; i < nlanguages; i++) {
                wcscat(txt, langinfo[i].lang_english);
                wcscat(txt, L": ");
                wcscat(txt, langinfo[i].author);
                if (i + 1 != nlanguages) {
                    wcscat(txt, L"\r\n");
                }
            }
            SetDlgItemText(hwnd, IDC_TRANSLATIONS, txt);
        }
    }

    return FALSE;
}
/////////////////////////////////////////////////////////////////////////////
//
LRESULT CALLBACK TestWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static int centerfrac=24;
    switch (msg) {
    case WM_PAINT:;
        RECT cRect, wRect;
        HPEN pen = (HPEN) CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        GetWindowRect(hwnd, &wRect);
        GetClientRect(hwnd, &cRect);
        POINT Offset = { wRect.left, wRect.top };
        ScreenToClient(hwnd, &Offset);

        SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
        SelectObject(hdc, pen);
        SetROP2(hdc, R2_BLACK);
        int width = wRect.right - wRect.left;
        int height = wRect.bottom - wRect.top;

        FillRect(hdc, &cRect, GetStockObject(WHITE_BRUSH));
        Rectangle(hdc
            , Offset.x+(width-width*centerfrac/100)/2
            , Offset.y
            , (width+width*centerfrac/100)/2 + Offset.x
            , height);
        Rectangle(hdc
            , Offset.x
            , Offset.y+(height-height*centerfrac/100)/2
            , width
            , (height+height*centerfrac/100)/2 + Offset.y);

        DeleteObject(pen);

        EndPaint(hwnd, &ps);
        return 0;
        break;

    case WM_ERASEBKGND:
        return 0;
        break;

    case WM_UPDCFRACTION:
        centerfrac = lParam;
        return 0;
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
/////////////////////////////////////////////////////////////////////////////
INT_PTR CALLBACK AdvancedPageDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_INITDIALOG) {
        wchar_t txt[10];

        int ret = GetPrivateProfileInt(L"Advanced", L"PearceDBClick", 0, inipath);
        Button_SetCheck(GetDlgItem(hwnd, IDC_PEARCEDBCLICK), ret? BST_CHECKED: BST_UNCHECKED);

        ret = GetPrivateProfileInt(L"Advanced", L"AutoRemaximize", 0, inipath);
        Button_SetCheck(GetDlgItem(hwnd, IDC_AUTOREMAXIMIZE), ret? BST_CHECKED: BST_UNCHECKED);

        ret = GetPrivateProfileInt(L"Advanced", L"AeroTopMaximizes", 1, inipath);
        Button_SetCheck(GetDlgItem(hwnd, IDC_AEROTOPMAXIMIZES), ret? BST_CHECKED: BST_UNCHECKED);

        ret = GetPrivateProfileInt(L"Advanced", L"MultipleInstances", 0, inipath);
        Button_SetCheck(GetDlgItem(hwnd, IDC_MULTIPLEINSTANCES), ret? BST_CHECKED: BST_UNCHECKED);

        ret = GetPrivateProfileInt(L"General", L"NormRestore", 0, inipath);
        Button_SetCheck(GetDlgItem(hwnd, IDC_NORMRESTORE), ret? BST_CHECKED: BST_UNCHECKED);

        ret = GetPrivateProfileInt(L"Advanced", L"FullScreen", 1, inipath);
        Button_SetCheck(GetDlgItem(hwnd, IDC_FULLSCREEN), ret? BST_CHECKED: BST_UNCHECKED);

        ret = GetPrivateProfileInt(L"General", L"MMMaximize", 1, inipath);
        Button_SetCheck(GetDlgItem(hwnd, IDC_MAXWITHLCLICK), (ret&1)? BST_CHECKED: BST_UNCHECKED);
        Button_SetCheck(GetDlgItem(hwnd, IDC_RESTOREONCLICK), (ret&2)? BST_CHECKED: BST_UNCHECKED);

        GetPrivateProfileString(L"General", L"CenterFraction", L"", txt, ARR_SZ(txt), inipath);
        SetDlgItemText(hwnd, IDC_CENTERFRACTION, txt);

        GetPrivateProfileString(L"General", L"AeroHoffset", L"", txt, ARR_SZ(txt), inipath);
        SetDlgItemText(hwnd, IDC_AEROHOFFSET, txt);

        GetPrivateProfileString(L"General", L"AeroVoffset", L"", txt, ARR_SZ(txt), inipath);
        SetDlgItemText(hwnd, IDC_AEROVOFFSET, txt);

        GetPrivateProfileString(L"Advanced", L"SnapThreshold", L"", txt, ARR_SZ(txt), inipath);
        SetDlgItemText(hwnd, IDC_SNAPTHRESHOLD, txt);

        GetPrivateProfileString(L"Advanced", L"AeroThreshold", L"", txt, ARR_SZ(txt), inipath);
        SetDlgItemText(hwnd, IDC_AEROTHRESHOLD, txt);

    } else if (msg == WM_COMMAND) {
        int id = LOWORD(wParam);
        int event = HIWORD(wParam);
        HWND control = GetDlgItem(hwnd, id);
        int val = Button_GetCheck(control);
        wchar_t txt[10];
        static HWND testwnd=NULL;

        if (id == IDC_AUTOREMAXIMIZE) {
            WritePrivateProfileString(L"Advanced",L"AutoRemaximize", _itow(val, txt, 10), inipath);
        } else if (id == IDC_AEROTOPMAXIMIZES || id == IDC_AERODBCLICKSHIFT) {
            int ret = GetPrivateProfileInt(L"General", L"MMMaximize", 0, inipath);
            val = (id == IDC_AEROTOPMAXIMIZES)? (val+2) & (ret|1): (val*2+1) & (ret|2);
            WritePrivateProfileString(L"Advanced",L"AeroTopMaximizes", _itow(val, txt, 10), inipath);
        } else if (id == IDC_MULTIPLEINSTANCES) {
            WritePrivateProfileString(L"Advanced",L"MultipleInstances", _itow(val, txt, 10), inipath);
        } else if (id == IDC_NORMRESTORE) {
            WritePrivateProfileString(L"General", L"NormRestore", _itow(val, txt, 10), inipath);
        } else if (id == IDC_FULLSCREEN) {
            WritePrivateProfileString(L"Advanced", L"FullScreen", _itow(val, txt, 10), inipath);
        } else if (id == IDC_MAXWITHLCLICK || id == IDC_RESTOREONCLICK) {
            int ret = GetPrivateProfileInt(L"General", L"MMMaximize", 0, inipath);
            val = (id == IDC_MAXWITHLCLICK)? (val+2) & (ret|1): (val*2+1) & (ret|2);
            WritePrivateProfileString(L"General", L"MMMaximize", _itow(val, txt, 10), inipath);
        } else if (id == IDC_TESTWINDOW) {
            if (testwnd && IsWindow(testwnd)){
                return FALSE;
            }
            WNDCLASSEX wnd =
                { sizeof(WNDCLASSEX)
                , CS_HREDRAW|CS_VREDRAW
                , TestWindowProc
                , 0, 0, g_hinst
                , icon[1]
                , LoadCursor(NULL, IDC_ARROW)
                , (HBRUSH)(COLOR_BACKGROUND+1)
                , NULL, APP_NAME"-Test", NULL };
            RegisterClassEx(&wnd);
            wchar_t wintitle[256];
            wcscpy_noaccel(wintitle, l10n->advanced_testwindow, ARR_SZ(wintitle));
            testwnd = CreateWindowEx(0
                 , wnd.lpszClassName
                 , wintitle, WS_CAPTION|WS_OVERLAPPEDWINDOW
                 , CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT
                 , NULL, NULL, g_hinst, NULL);
            PostMessage(testwnd, WM_UPDCFRACTION, 0
                 , GetPrivateProfileInt(L"General", L"CenterFraction", 24, inipath));
            ShowWindow(testwnd, SW_SHOW);
        }
        if (event == EN_KILLFOCUS) {
            Edit_GetText(control, txt, ARR_SZ(txt));
            if (id == IDC_CENTERFRACTION) {
                WritePrivateProfileString(L"General", L"CenterFraction", txt, inipath);
                if (testwnd && IsWindow(testwnd)) {
                    PostMessage(testwnd, WM_UPDCFRACTION, 0
                       , GetPrivateProfileInt(L"General", L"CenterFraction", 24, inipath));
                }
            } else if (id == IDC_AEROHOFFSET) {
                WritePrivateProfileString(L"General", L"AeroHoffset", txt, inipath);
            } else if (id == IDC_AEROVOFFSET) {
                WritePrivateProfileString(L"General", L"AeroVoffset", txt, inipath);
            } else if (id == IDC_SNAPTHRESHOLD) {
                WritePrivateProfileString(L"Advanced", L"SnapThreshold", txt, inipath);
            } else if (id == IDC_AEROTHRESHOLD) {
                WritePrivateProfileString(L"Advanced", L"AeroThreshold", txt, inipath);
            }
        }
        UpdateSettings();
    } else if (msg == WM_NOTIFY) {
        LPNMHDR pnmh = (LPNMHDR) lParam;
        if (pnmh->code == PSN_SETACTIVE) {
            // Update text
            SetDlgItemText(hwnd, IDC_METRICS_BOX,      l10n->advanced_metrics_box);
            SetDlgItemText(hwnd, IDC_CENTERFRACTION_H, l10n->advanced_centerfraction);
            SetDlgItemText(hwnd, IDC_AEROHOFFSET_H,    l10n->advanced_aerohoffset);
            SetDlgItemText(hwnd, IDC_AEROVOFFSET_H,    l10n->advanced_aerovoffset);
            SetDlgItemText(hwnd, IDC_SNAPTHRESHOLD_H,  l10n->advanced_snapthreshold);
            SetDlgItemText(hwnd, IDC_AEROTHRESHOLD_H,  l10n->advanced_aerothreshold);
            SetDlgItemText(hwnd, IDC_TESTWINDOW,       l10n->advanced_testwindow);

            SetDlgItemText(hwnd, IDC_BEHAVIOR_BOX,     l10n->advanced_behavior_box);
            SetDlgItemText(hwnd, IDC_MULTIPLEINSTANCES,l10n->advanced_multipleinstances);
            SetDlgItemText(hwnd, IDC_AUTOREMAXIMIZE,   l10n->advanced_autoremaximize);
            SetDlgItemText(hwnd, IDC_NORMRESTORE,      l10n->advanced_normrestore);
            SetDlgItemText(hwnd, IDC_AEROTOPMAXIMIZES, l10n->advanced_aerotopmaximizes);
            SetDlgItemText(hwnd, IDC_AERODBCLICKSHIFT, l10n->advanced_aerodbclickshift);
            SetDlgItemText(hwnd, IDC_MAXWITHLCLICK,    l10n->advanced_maxwithlclick);
            SetDlgItemText(hwnd, IDC_RESTOREONCLICK,   l10n->advanced_restoreonclick);
            SetDlgItemText(hwnd, IDC_FULLSCREEN,       l10n->advanced_fullscreen);
        }
    }
    return FALSE;
}
