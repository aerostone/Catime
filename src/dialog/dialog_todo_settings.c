/**
 * @file dialog_todo_settings.c
 * @brief TODO sync settings dialog: enable/URL/token/poll + status.
 *
 * Groups: sync GROUPBOX (C3) + sticky GROUPBOX. Enable checkbox is
 * disabled until URL+token are present (C1: never silently swallow).
 * Status line (C2/S2) shows last poll outcome + conflict button.
 * UI debug checkbox toggles the overlay at runtime (TodoUiDebug).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dialog/dialog_todo.h"
#include "../../resource/resource.h"
#include "dialog/dialog_registry.h"
#include "language.h"
#include "todo/todo_stickies.h"
#include "todo/todo_store.h"
#include "todo/todo_sync.h"
#include "todo/todo_sync_internal.h"
#include "todo/todo_sync_status.h"
#include "todo/todo_conflict.h"
#include "todo/todo_ui_debug.h"

static void RefreshStatusLine(HWND hdlg) {
    wchar_t st[96];
    TodoSyncStatus_Label(st, _countof(st));
    SetDlgItemTextW(hdlg, IDC_TODO_SYNC_STATUS, st);
    int n = TodoConflict_Count();
    EnableWindow(GetDlgItem(hdlg, IDC_TODO_CONFLICT_BTN), n > 0);
    if (n > 0) {
        wchar_t cf[64];
#if defined(_MSC_VER)
        _snwprintf_s(cf, _countof(cf), _TRUNCATE, L"Conflicts (%d)...", n);
#else
        swprintf(cf, 64, L"Conflicts (%d)...", n);
#endif
        SetDlgItemTextW(hdlg, IDC_TODO_CONFLICT_BTN, cf);
    } else {
        SetDlgItemTextW(hdlg, IDC_TODO_CONFLICT_BTN, L"Conflicts...");
    }
}

static void RefreshEnableGate(HWND hdlg) {
    wchar_t a[TODO_URL_LEN], b[TODO_TOKEN_LEN];
    GetDlgItemTextW(hdlg, IDC_TODO_SYNC_URL, a, _countof(a));
    GetDlgItemTextW(hdlg, IDC_TODO_SYNC_TOKEN, b, _countof(b));
    /* leading/trailing blanks do not count as a backend */
    size_t la = wcslen(a), lb = wcslen(b);
    while (la && (a[la - 1] == L' ' || a[la - 1] == L'\t')) a[--la] = L'\0';
    while (lb && (b[lb - 1] == L' ' || b[lb - 1] == L'\t')) b[--lb] = L'\0';
    BOOL has = la > 0 && lb > 0;
    EnableWindow(GetDlgItem(hdlg, IDC_TODO_SYNC_ENABLE), has);
    BOOL en = IsDlgButtonChecked(hdlg, IDC_TODO_SYNC_ENABLE) == BST_CHECKED;
    EnableWindow(GetDlgItem(hdlg, IDC_TODO_SETTINGS_SYNC_BTN), has && en);
    if (!has && en)
        CheckDlgButton(hdlg, IDC_TODO_SYNC_ENABLE, BST_UNCHECKED);
}

static void FillFromCurrent(HWND hdlg) {
    char url[TODO_URL_LEN] = "";
    char token[TODO_TOKEN_LEN] = "";
    BOOL en = FALSE;
    int poll = 60;
    TodoSync_GetSettings(&en, url, sizeof(url), token, sizeof(token), &poll);
    wchar_t wu[TODO_URL_LEN], wt[TODO_TOKEN_LEN], wp[16];
    if (MultiByteToWideChar(CP_UTF8, 0, url, -1, wu, _countof(wu)))
        SetDlgItemTextW(hdlg, IDC_TODO_SYNC_URL, wu);
    if (MultiByteToWideChar(CP_UTF8, 0, token, -1, wt, _countof(wt)))
        SetDlgItemTextW(hdlg, IDC_TODO_SYNC_TOKEN, wt);
    _snwprintf_s(wp, _countof(wp), _TRUNCATE, L"%d", poll);
    SetDlgItemTextW(hdlg, IDC_TODO_SYNC_POLL, wp);
    CheckDlgButton(hdlg, IDC_TODO_SYNC_ENABLE, en ? BST_CHECKED : BST_UNCHECKED);
    RefreshEnableGate(hdlg);
    RefreshStatusLine(hdlg);
    CheckDlgButton(hdlg, IDC_TODO_STICKY_TOPMOST,
                   TodoSticky_TopmostGlobal() ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hdlg, IDC_TODO_DEBUG_CHECK,
                   TodoUiDebug_Enabled() ? BST_CHECKED : BST_UNCHECKED);
}

static BOOL CollectAndSave(HWND hdlg) {
    wchar_t wu[TODO_URL_LEN], wt[TODO_TOKEN_LEN], wp[16];
    GetDlgItemTextW(hdlg, IDC_TODO_SYNC_URL, wu, _countof(wu));
    GetDlgItemTextW(hdlg, IDC_TODO_SYNC_TOKEN, wt, _countof(wt));
    GetDlgItemTextW(hdlg, IDC_TODO_SYNC_POLL, wp, _countof(wp));
    char url[TODO_URL_LEN] = "", token[TODO_TOKEN_LEN] = "";
    WideCharToMultiByte(CP_UTF8, 0, wu, -1, url, sizeof(url), NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, wt, -1, token, sizeof(token), NULL, NULL);
    int poll = _wtoi(wp);
    if (poll <= 0) poll = 60;
    if (poll < 15) poll = 15;
    if (poll > 600) poll = 600;
    BOOL wantEn = IsDlgButtonChecked(hdlg, IDC_TODO_SYNC_ENABLE) == BST_CHECKED;
    BOOL en = wantEn;
    /* C1: never silently swallow -- refuse with a message instead. */
    if (wantEn && (!url[0] || !token[0])) {
        MessageBoxW(hdlg,
            GetLocalizedString(
                L"\u8bf7\u5148\u586b\u5199 Server URL \u548c Token \u518d\u542f\u7528\u540c\u6b65",
                L"Fill in Server URL and Token before enabling sync"),
            GetLocalizedString(L"TODO", L"TODO"), MB_ICONINFORMATION);
        return FALSE;
    }
    size_t n = strlen(url);
    while (n > 0 && url[n - 1] == '/') url[--n] = '\0';
    if (!TodoSync_ApplySettings(en, url, token, poll)) {
        MessageBoxW(hdlg,
            GetLocalizedString(L"\u4fdd\u5b58\u540c\u6b65\u8bbe\u7f6e\u5931\u8d25",
                               L"Failed to save sync settings"),
            GetLocalizedString(L"TODO", L"TODO"), MB_ICONWARNING);
        return FALSE;
    }
    TodoSync_Reload();
    if (en) TodoSync_PollNow();
    BOOL top = IsDlgButtonChecked(hdlg, IDC_TODO_STICKY_TOPMOST) == BST_CHECKED;
    if (top != TodoSticky_TopmostGlobal()) {
        TodoSticky_SetTopmostGlobal(top);
        TodoSticky_RetopAll();
    }
    TodoUiDebug_SetEnabled(
        IsDlgButtonChecked(hdlg, IDC_TODO_DEBUG_CHECK) == BST_CHECKED);
    return TRUE;
}

static INT_PTR CALLBACK TodoSettingsProc(HWND hdlg, UINT msg,
                                         WPARAM wp, LPARAM lp) {
    (void)lp;
    switch (msg) {
    case WM_INITDIALOG:
        Dialog_InitializeInstance(DIALOG_INSTANCE_TODO_SETTINGS, hdlg);
        FillFromCurrent(hdlg);
        TodoUiDebug_DumpDialog(hdlg, "todo-settings");
        return TRUE;
    case WM_COMMAND: {
        WORD id = LOWORD(wp);
        WORD code = HIWORD(wp);
        if (id == IDOK) {
            if (CollectAndSave(hdlg)) DestroyWindow(hdlg);
            return TRUE;
        }
        if (id == IDCANCEL) {
            DestroyWindow(hdlg);
            return TRUE;
        }
        if (id == IDC_TODO_SETTINGS_SYNC_BTN) {
            TodoSync_PollNow();
            RefreshStatusLine(hdlg);
            return TRUE;
        }
        if (id == IDC_TODO_CONFLICT_BTN) {
            TodoConflict_OpenDir(hdlg);
            return TRUE;
        }
        if ((id == IDC_TODO_SYNC_URL || id == IDC_TODO_SYNC_TOKEN) &&
            code == EN_CHANGE) {
            RefreshEnableGate(hdlg);
            return TRUE;
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(hdlg);
        return TRUE;
    case WM_DESTROY:
        Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_TODO_SETTINGS, hdlg);
        break;
    }
    return FALSE;
}

void ShowTodoSettingsDialog(HWND hwndParent) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_TODO_SETTINGS)) {
        HWND ex = Dialog_GetInstance(DIALOG_INSTANCE_TODO_SETTINGS);
        if (ex) SetForegroundWindow(ex);
        return;
    }
    HWND hdlg = CreateDialogW(GetModuleHandleW(NULL),
                              MAKEINTRESOURCEW(IDD_TODO_SETTINGS_DIALOG),
                              hwndParent, TodoSettingsProc);
    if (hdlg) ShowWindow(hdlg, SW_SHOW);
}
