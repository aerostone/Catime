/**
 * @file dialog_todo_settings.c
 * @brief TODO sync settings dialog: enable/URL/token/poll + Reload.
 *
 * Defaults: empty URL/token, disabled, 60s poll. On OK: writes Sync
 * section via TodoSync_ApplySettings (INI cache layer) then calls
 * TodoSync_Reload so the engine picks it up without restart.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dialog/dialog_todo.h"
#include "../../resource/resource.h"
#include "dialog/dialog_registry.h"
#include "todo/todo_stickies.h"
#include "todo/todo_store.h"
#include "todo/todo_sync.h"
#include "todo/todo_sync_internal.h"

static void FillFromCurrent(HWND hdlg) {
    char url[TODO_URL_LEN] = "";
    char token[TODO_TOKEN_LEN] = "";
    BOOL en = FALSE;
    int poll = 60;
    TodoSync_GetSettings(&en, url, sizeof(url), token, sizeof(token), &poll);
    CheckDlgButton(hdlg, IDC_TODO_SYNC_ENABLE, en ? BST_CHECKED : BST_UNCHECKED);
    wchar_t wu[TODO_URL_LEN], wt[TODO_TOKEN_LEN], wp[16];
    if (MultiByteToWideChar(CP_UTF8, 0, url, -1, wu, _countof(wu)))
        SetDlgItemTextW(hdlg, IDC_TODO_SYNC_URL, wu);
    if (MultiByteToWideChar(CP_UTF8, 0, token, -1, wt, _countof(wt)))
        SetDlgItemTextW(hdlg, IDC_TODO_SYNC_TOKEN, wt);
    _snwprintf_s(wp, _countof(wp), _TRUNCATE, L"%d", poll);
    SetDlgItemTextW(hdlg, IDC_TODO_SYNC_POLL, wp);
    /* empty URL/token -> disable sync controls except enable box */
    BOOL hasBackend = url[0] != '\0' && token[0] != '\0';
    EnableWindow(GetDlgItem(hdlg, IDC_TODO_SETTINGS_SYNC_BTN), hasBackend && en);
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
    BOOL en = IsDlgButtonChecked(hdlg, IDC_TODO_SYNC_ENABLE) == BST_CHECKED;
    /* empty backend forces disabled (user asked: start empty+disabled) */
    if (!url[0] || !token[0]) en = FALSE;
    /* trim trailing '/' */
    size_t n = strlen(url);
    while (n > 0 && url[n - 1] == '/') url[--n] = '\0';
    if (!TodoSync_ApplySettings(en, url, token, poll)) {
        MessageBoxW(hdlg, L"保存同步设置失败", L"TODO", MB_ICONWARNING);
        return FALSE;
    }
    TodoSync_Reload();
    if (en) TodoSync_PollNow();
    return TRUE;
}

static INT_PTR CALLBACK TodoSettingsProc(HWND hdlg, UINT msg,
                                         WPARAM wp, LPARAM lp) {
    (void)lp;
    switch (msg) {
    case WM_INITDIALOG:
        Dialog_InitializeInstance(DIALOG_INSTANCE_TODO_SETTINGS, hdlg);
        FillFromCurrent(hdlg);
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
            return TRUE;
        }
        if ((id == IDC_TODO_SYNC_URL || id == IDC_TODO_SYNC_TOKEN) &&
            code == EN_CHANGE) {
            wchar_t a[TODO_URL_LEN], b[TODO_TOKEN_LEN];
            GetDlgItemTextW(hdlg, IDC_TODO_SYNC_URL, a, _countof(a));
            GetDlgItemTextW(hdlg, IDC_TODO_SYNC_TOKEN, b, _countof(b));
            BOOL has = a[0] && b[0];
            BOOL en = IsDlgButtonChecked(hdlg, IDC_TODO_SYNC_ENABLE) == BST_CHECKED;
            EnableWindow(GetDlgItem(hdlg, IDC_TODO_SETTINGS_SYNC_BTN), has && en);
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
