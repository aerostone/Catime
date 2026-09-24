/**
 * @file dialog_todo_books.c
 * @brief Task-book manager dialog (RD3/RD4).
 *
 * One place for: board list + New/Rename/Del + show/hide panel +
 * sync status + Sync Now + jump to sync settings. The task manager
 * dialog keeps only per-task editing; everything book-level lives here.
 */
#include <string.h>

#include "../../resource/resource.h"
#include "dialog/dialog_todo.h"
#include "dialog/dialog_registry.h"
#include "language.h"
#include "todo/todo_board.h"
#include "todo/todo_conflict.h"
#include "todo/todo_stickies.h"
#include "todo/todo_sync.h"
#include "todo/todo_sync_internal.h"
#include "todo/todo_sync_status.h"
#include "todo/todo_task_sync.h"
#include "window_procedure/window_helpers.h"

static void RefreshSyncCalCombo(HWND hdlg); /* fwd (#28b) */

static void RefreshList(HWND hdlg) {
    HWND list = GetDlgItem(hdlg, IDC_TODO_BOOKS_LIST);
    if (!list) return;
    LRESULT sel = SendMessageW(list, LB_GETCURSEL, 0, 0);
    wchar_t selName[TODO_STORE_BOARD_LEN] = L"";
    if (sel >= 0) SendMessageW(list, LB_GETTEXT, (WPARAM)sel,
                               (LPARAM)selName);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    int n = TodoBoard_Count();
    int keep = 0;
    for (int i = 0; i < n; i++) {
        const char *name = TodoBoard_NameAt(i);
        if (!name[0]) continue;
        wchar_t w[TODO_STORE_BOARD_LEN];
        MultiByteToWideChar(CP_UTF8, 0, name, -1, w,
                            TODO_STORE_BOARD_LEN);
        int idx = (int)SendMessageW(list, LB_ADDSTRING, 0, (LPARAM)w);
        if (selName[0] && wcscmp(w, selName) == 0) keep = idx;
    }
    SendMessageW(list, LB_SETCURSEL, (WPARAM)keep, 0);
    wchar_t st[96];
    TodoSyncStatus_Label(st, _countof(st));
    SetDlgItemTextW(hdlg, IDC_TODO_BOOK_STATUS, st);
    RefreshSyncCalCombo(hdlg); /* #28b */
}

/* #28b: sync-book picker. Options come from the last pull; the choice
 * (tweek calendar id) persists in [Sync] CalendarId and re-pulls. */
static TaskSyncCal s_syncCals[32];
static int s_syncCalCount = 0;

static void RefreshSyncCalCombo(HWND hdlg) {
    HWND cb = GetDlgItem(hdlg, IDC_TODO_BOOK_SYNC_CAL);
    if (!cb) return;
    char cur[TODO_STORE_UUID_LEN] = "";
    TodoSync_GetCalendarId(cur, sizeof(cur));
    SendMessageW(cb, CB_RESETCONTENT, 0, 0);
    BOOL en = FALSE;
    char server[TODO_URL_LEN] = "", token[TODO_TOKEN_LEN] = "";
    int poll = 60;
    TodoSync_GetSettings(&en, server, sizeof(server), token, sizeof(token),
                         &poll);
    int sel = -1, n = 0;
    if (en && server[0] && token[0]) {
        char url[TODO_URL_LEN + 64];
        _snprintf_s(url, sizeof(url), _TRUNCATE, "%s/api/catime/sync?top=1",
                    server);
        char *resp = TodoSyncHttp_Get(url, token);
        if (resp) {
            n = TaskSync_ParseCalendars(resp, s_syncCals, 32);
            s_syncCalCount = n;
            for (int i = 0; i < n; i++) {
                wchar_t w[TODO_STORE_BOARD_LEN];
                MultiByteToWideChar(CP_UTF8, 0, s_syncCals[i].name[0] ? s_syncCals[i].name : s_syncCals[i].id,
                                    -1, w, TODO_STORE_BOARD_LEN);
                int idx = (int)SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)w);
                if (cur[0] && strcmp(s_syncCals[i].id, cur) == 0) sel = idx;
            }
            free(resp);
        }
    }
    if (sel < 0 && n > 0 && !cur[0]) sel = 0; /* first run: default first */
    if (sel >= 0) SendMessageW(cb, CB_SETCURSEL, (WPARAM)sel, 0);
    if (n == 0 && cur[0]) {
        /* offline: show the stored id so the choice stays visible */
        wchar_t w[TODO_STORE_UUID_LEN];
        MultiByteToWideChar(CP_UTF8, 0, cur, -1, w, TODO_STORE_UUID_LEN);
        SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)w);
        SendMessageW(cb, CB_SETCURSEL, 0, 0);
    }
    EnableWindow(cb, n > 0);
}

/* #28b: picking a book persists CalendarId and re-pulls under it. */
static void OnSyncCalChanged(HWND hdlg) {
    HWND cb = GetDlgItem(hdlg, IDC_TODO_BOOK_SYNC_CAL);
    if (!cb) return;
    int idx = (int)SendMessageW(cb, CB_GETCURSEL, 0, 0);
    if (idx < 0 || idx >= s_syncCalCount) return;
    TodoSync_SetCalendarId(s_syncCals[idx].id);
    RefreshList(hdlg);
}

static BOOL SelName(HWND hdlg, char *out, size_t cap) {
    if (out && cap) out[0] = '\0';
    HWND list = GetDlgItem(hdlg, IDC_TODO_BOOKS_LIST);
    if (!list) return FALSE;
    LRESULT sel = SendMessageW(list, LB_GETCURSEL, 0, 0);
    if (sel < 0) return FALSE;
    wchar_t w[TODO_STORE_BOARD_LEN];
    SendMessageW(list, LB_GETTEXT, (WPARAM)sel, (LPARAM)w);
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)cap, NULL, NULL);
    return out[0] != '\0';
}

static void OnNew(HWND hdlg) {
    wchar_t wname[TODO_STORE_BOARD_LEN] = L"";
    InputBox(hdlg, GetLocalizedString(L"TODO \u4EFB\u52A1\u672C", L"TODO task book"),
             GetLocalizedString(L"\u65B0\u4EFB\u52A1\u672C\u540D\u79F0\uFF1A",
                                L"New task book name:"),
             L"", wname, TODO_STORE_BOARD_LEN);
    if (!wname[0]) return;
    char name[TODO_STORE_BOARD_LEN * 2];
    WideCharToMultiByte(CP_UTF8, 0, wname, -1, name, sizeof(name),
                        NULL, NULL);
    if (!TodoBoard_Add(name)) {
        MessageBoxW(hdlg,
            GetLocalizedString(L"\u65E0\u6CD5\u521B\u5EFA\uFF08\u91CD\u540D\u6216\u6570\u91CF\u4E0A\u9650\uFF09",
                               L"Cannot create (duplicate or limit)"),
            L"TODO", MB_ICONWARNING);
        return;
    }
    RefreshList(hdlg);
}

static void OnRename(HWND hdlg) {
    char old[TODO_STORE_BOARD_LEN];
    if (!SelName(hdlg, old, sizeof(old))) return;
    int idx = TodoBoard_IndexByName(old);
    if (idx < 0) return;
    if (TodoBoard_IsSync(idx)) {
        MessageBoxW(hdlg,
            GetLocalizedString(L"\u540C\u6B65\u4EFB\u52A1\u672C\u540D\u79F0\u56FA\u5B9A",
                               L"The sync book keeps its name"),
            L"TODO", MB_ICONINFORMATION);
        return;
    }
    wchar_t wname[TODO_STORE_BOARD_LEN] = L"";
    InputBox(hdlg, GetLocalizedString(L"TODO \u4EFB\u52A1\u672C", L"TODO task book"),
             GetLocalizedString(L"\u4EFB\u52A1\u672C\u65B0\u540D\u79F0\uFF1A",
                                L"Rename book to:"),
             L"", wname, TODO_STORE_BOARD_LEN);
    if (!wname[0]) return;
    char name[TODO_STORE_BOARD_LEN * 2];
    WideCharToMultiByte(CP_UTF8, 0, wname, -1, name, sizeof(name),
                        NULL, NULL);
    BOOL wasVisible = TodoStickies_IsVisible(old);
    if (!TodoBoard_Rename(idx, name)) {
        MessageBoxW(hdlg,
            GetLocalizedString(L"\u91CD\u547D\u540D\u5931\u8D25", L"Rename failed"),
            L"TODO", MB_ICONWARNING);
        return;
    }
    if (wasVisible) {
        TodoStickies_HideBoard(old);
        TodoStickies_ShowBoard(name);
    }
    RefreshList(hdlg);
}

static void OnDelete(HWND hdlg) {
    char name[TODO_STORE_BOARD_LEN];
    if (!SelName(hdlg, name, sizeof(name))) return;
    int idx = TodoBoard_IndexByName(name);
    if (idx < 0 || TodoBoard_IsSync(idx)) return;
    wchar_t wname[TODO_STORE_BOARD_LEN], msg[256];
    MultiByteToWideChar(CP_UTF8, 0, name, -1, wname,
                        TODO_STORE_BOARD_LEN);
    _snwprintf_s(msg, 256, _TRUNCATE,
        L"\u5220\u9664\u4EFB\u52A1\u672C\u201C%s\u201D\uFF1F\u4EFB\u52A1\u5C06\u79FB\u5230\u7B2C\u4E00\u4E2A\u4EFB\u52A1\u672C\u3002",
        wname);
    if (MessageBoxW(hdlg, msg, L"TODO",
                    MB_OKCANCEL | MB_ICONWARNING) != IDOK)
        return;
    TodoStickies_HideBoard(name);
    TodoBoard_Remove(idx);
    RefreshList(hdlg);
}

static void OnShow(HWND hdlg) {
    char name[TODO_STORE_BOARD_LEN];
    if (!SelName(hdlg, name, sizeof(name))) return;
    TodoStickies_ToggleBoard(name);
}

static INT_PTR CALLBACK BooksProc(HWND hdlg, UINT msg, WPARAM wp,
                                  LPARAM lp) {
    (void)lp;
    switch (msg) {
    case WM_INITDIALOG:
        Dialog_InitializeInstance(DIALOG_INSTANCE_TODO_BOOKS, hdlg);
        RefreshList(hdlg);
        SetDlgItemTextW(hdlg, IDC_TODO_BOOK_NEW,
            GetLocalizedString(L"\u65B0\u5EFA", L"New"));
        SetDlgItemTextW(hdlg, IDC_TODO_BOOK_RENAME,
            GetLocalizedString(L"\u91CD\u547D\u540D", L"Rename"));
        SetDlgItemTextW(hdlg, IDC_TODO_BOOK_DEL,
            GetLocalizedString(L"\u5220\u9664", L"Delete"));
        SetDlgItemTextW(hdlg, IDC_TODO_BOOK_SHOW,
            GetLocalizedString(L"\u663E\u793A\u9762\u677F", L"Show panel"));
        return TRUE;
    case WM_COMMAND: {
        WORD id = LOWORD(wp);
        WORD code = HIWORD(wp);
        if (id == IDOK || id == IDCANCEL) {
            DestroyWindow(hdlg);
            return TRUE;
        }
        if (id == IDC_TODO_BOOK_NEW) { OnNew(hdlg); return TRUE; }
        if (id == IDC_TODO_BOOK_RENAME) { OnRename(hdlg); return TRUE; }
        if (id == IDC_TODO_BOOK_DEL) { OnDelete(hdlg); return TRUE; }
        if (id == IDC_TODO_BOOK_SHOW) { OnShow(hdlg); return TRUE; }
        if (id == IDC_TODO_BOOK_SYNC_CAL && code == CBN_SELCHANGE) {
            OnSyncCalChanged(hdlg);
            return TRUE;
        }
        if (id == IDC_TODO_BOOK_SYNC_NOW) {
            TodoSync_PollNow();
            RefreshList(hdlg);
            return TRUE;
        }
        if (id == IDC_TODO_BOOK_SETTINGS) {
            ShowTodoSettingsDialog(hdlg);
            return TRUE;
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(hdlg);
        return TRUE;
    case WM_DESTROY:
        Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_TODO_BOOKS,
                                           hdlg);
        break;
    }
    return FALSE;
}

void ShowTodoBooksDialog(HWND hwndParent) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_TODO_BOOKS)) {
        HWND ex = Dialog_GetInstance(DIALOG_INSTANCE_TODO_BOOKS);
        if (ex) SetForegroundWindow(ex);
        return;
    }
    HWND hdlg = CreateDialogW(GetModuleHandleW(NULL),
                              MAKEINTRESOURCEW(IDD_TODO_BOARDS_DIALOG),
                              hwndParent, BooksProc);
    if (hdlg) ShowWindow(hdlg, SW_SHOW);
}
