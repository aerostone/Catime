/**
 * @file dialog_todo_boards.c
 * @brief Board selector for the TODO list dialog.
 *
 * The dialog always edits one board at a time: the combo picks the
 * board, the list shows only that board's tasks, new tasks land on it,
 * and the sticky button shows/hides its desktop card.
 */
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "../../resource/resource.h"
#include "dialog/dialog_todo.h"
#include "language.h"
#include "todo/todo_board.h"
#include "todo/todo_stickies.h"
#include "todo/todo_store.h"
#include "window_procedure/window_helpers.h"

#include "dialog/dialog_todo_parts.h"

static const wchar_t *BoardLabel(const char *name, wchar_t *buf, size_t cap) {
    MultiByteToWideChar(CP_UTF8, 0, name ? name : "", -1, buf, (int)cap);
    return buf;
}

void TodoDlg_GetBoard(HWND hdlg, char *out, size_t cap) {
    if (out && cap) out[0] = '\0';
    if (!hdlg || !out || cap == 0) return;
    HWND c = GetDlgItem(hdlg, IDC_TODO_BOARD_SEL);
    if (!c) return;
    int sel = (int)SendMessageW(c, CB_GETCURSEL, 0, 0);
    wchar_t w[TODO_STORE_BOARD_LEN];
    w[0] = L'\0';
    if (sel < 0) {
        if (TodoBoard_Count() > 0) {
            strcpy_s(out, cap, TodoBoard_NameAt(0));
            return;
        }
        strcpy_s(out, cap, TODO_BOARD_DEFAULT);
        return;
    }
    SendMessageW(c, CB_GETLBTEXT, (WPARAM)sel, (LPARAM)w);
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)cap, NULL, NULL);
}

/* L2: sync board is pull-only - the Add row is visibly disabled. */
static void SyncAddRowGate(HWND hdlg) {
    char b[TODO_STORE_BOARD_LEN];
    TodoDlg_GetBoard(hdlg, b, sizeof(b));
    BOOL sync = (strcmp(b, TODO_BOARD_SYNC) == 0);
    EnableWindow(GetDlgItem(hdlg, IDC_TODO_NEW_EDIT), !sync);
    EnableWindow(GetDlgItem(hdlg, IDC_TODO_NEW_DUE), !sync);
    EnableWindow(GetDlgItem(hdlg, IDC_TODO_NEW_IMPORTANCE), !sync);
    EnableWindow(GetDlgItem(hdlg, IDC_TODO_ADD_BUTTON), !sync);
    if (sync)
        SetDlgItemTextW(hdlg, IDC_TODO_NEW_EDIT,
            GetLocalizedString(L"\u540C\u6B65\u677F\u53EA\u8BFB\uFF0C\u8BF7\u5207\u6362\u5230\u672C\u5730\u677F\u6DFB\u52A0",
                               L"Sync board is read-only"));
    else if (!GetWindowTextLengthW(GetDlgItem(hdlg, IDC_TODO_NEW_EDIT)))
        SetDlgItemTextW(hdlg, IDC_TODO_NEW_EDIT, L"");
}

void TodoDlg_RefreshAddGate(HWND hdlg) { SyncAddRowGate(hdlg); }

void TodoDlg_SyncStickyButton(HWND hdlg) {
    char b[TODO_STORE_BOARD_LEN];
    TodoDlg_GetBoard(hdlg, b, sizeof(b));
    HWND btn = GetDlgItem(hdlg, IDC_TODO_BOARD_SHOW);
    if (!btn) return;
    SetWindowTextW(btn, TodoStickies_IsVisible(b)
                            ? GetLocalizedString(L"\u9690\u85cf\u4fbf\u7b7e",
                                                 L"Hide sticky")
                            : GetLocalizedString(L"\u663e\u793a\u4fbf\u7b7e",
                                                 L"Show sticky"));
}

void TodoDlg_InitBoardCombo(HWND hdlg) {
    HWND c = GetDlgItem(hdlg, IDC_TODO_BOARD_SEL);
    if (!c) return;
    SendMessageW(c, CB_RESETCONTENT, 0, 0);
    int n = TodoBoard_Count();
    for (int i = 0; i < n; i++) {
        wchar_t w[TODO_STORE_BOARD_LEN];
        SendMessageW(c, CB_ADDSTRING, 0,
                     (LPARAM)BoardLabel(TodoBoard_NameAt(i), w,
                                        TODO_STORE_BOARD_LEN));
    }
    SendMessageW(c, CB_SETCURSEL, 0, 0);
    TodoDlg_SyncStickyButton(hdlg);
    SyncAddRowGate(hdlg);
}

void TodoDlg_ApplyBoardFilter(HWND hdlg, TodoFilter *f) {
    if (!f) return;
    char b[TODO_STORE_BOARD_LEN];
    TodoDlg_GetBoard(hdlg, b, sizeof(b));
    if (!b[0]) return;
    strcpy_s(f->board, sizeof(f->board), b);
    /* the sync board is pull-only; local boards never show pulled rows */
    if (strcmp(b, TODO_BOARD_SYNC) == 0) {
        f->showLocal = FALSE;
        f->showSync = TRUE;
    } else {
        f->showLocal = TRUE;
        f->showSync = FALSE;
    }
}

void TodoDlg_SyncScopeCombo(HWND hdlg) {
    char b[TODO_STORE_BOARD_LEN];
    TodoDlg_GetBoard(hdlg, b, sizeof(b));
    HWND cb = GetDlgItem(hdlg, IDC_TODO_FILTER_DUE_SCOPE);
    if (!cb || !b[0]) return;
    TodoDueScope sc = TodoBoard_Scope(b);
    int idx = (sc == TODO_DUE_SCOPE_ALL) ? 3 : 0;
    SendMessageW(cb, CB_SETCURSEL, (WPARAM)idx, 0);
    BOOL custom = (idx == 2);
    EnableWindow(GetDlgItem(hdlg, IDC_TODO_FILTER_DUE_FROM), custom);
    EnableWindow(GetDlgItem(hdlg, IDC_TODO_FILTER_DUE_TO), custom);
}

void TodoDlg_PersistScope(HWND hdlg, TodoDueScope scope) {
    if (scope == TODO_DUE_SCOPE_CUSTOM || scope == TODO_DUE_SCOPE_MONTH) return;
    char b[TODO_STORE_BOARD_LEN];
    TodoDlg_GetBoard(hdlg, b, sizeof(b));
    if (!b[0] || strcmp(b, TODO_BOARD_SYNC) == 0) return;
    TodoBoard_SetScope(b, scope);
}

static void AskName(HWND hdlg, const wchar_t *prompt, const char *initial,
                    wchar_t *out, size_t cap) {
    wchar_t winit[TODO_STORE_BOARD_LEN];
    MultiByteToWideChar(CP_UTF8, 0, initial ? initial : "", -1, winit,
                        TODO_STORE_BOARD_LEN);
    out[0] = L'\0';
    InputBox(hdlg, GetLocalizedString(L"TODO \u4fbf\u7b3e", L"TODO Board"),
             prompt, winit, out, cap);
}

static void AddBoard(HWND hdlg) {
    wchar_t wname[TODO_STORE_BOARD_LEN];
    AskName(hdlg, GetLocalizedString(L"\u65b0\u4fbf\u7b3e\u540d\u79f0\uff1a",
                                     L"New board name:"),
            "", wname, TODO_STORE_BOARD_LEN);
    if (!wname[0]) return;
    char name[TODO_STORE_BOARD_LEN * 2];
    WideCharToMultiByte(CP_UTF8, 0, wname, -1, name, sizeof(name), NULL, NULL);
    if (!TodoBoard_Add(name)) {
        MessageBoxW(hdlg,
                    GetLocalizedString(
                        L"\u65e0\u6cd5\u521b\u5efa\uff08\u91cd\u540d\u6216\u6570\u91cf\u4e0a\u9650\uff09",
                        L"Cannot create (duplicate name or limit)"),
                    L"TODO", MB_ICONWARNING);
        return;
    }
    TodoDlg_InitBoardCombo(hdlg);
    HWND c = GetDlgItem(hdlg, IDC_TODO_BOARD_SEL);
    int idx = TodoBoard_IndexByName(name);
    if (c && idx >= 0) SendMessageW(c, CB_SETCURSEL, (WPARAM)idx, 0);
    TodoDlg_SyncStickyButton(hdlg);
    SyncAddRowGate(hdlg);
    TodoDlg_RefreshList(hdlg);
}

static void RenameBoard(HWND hdlg) {
    char oldName[TODO_STORE_BOARD_LEN];
    TodoDlg_GetBoard(hdlg, oldName, sizeof(oldName));
    int idx = TodoBoard_IndexByName(oldName);
    if (idx < 0) return;
    if (TodoBoard_IsSync(idx)) {
        MessageBoxW(hdlg,
                    GetLocalizedString(
                        L"\u540c\u6b65\u4fbf\u7b3e\u540d\u79f0\u56fa\u5b9a",
                        L"The sync board keeps its name"),
                    L"TODO", MB_ICONINFORMATION);
        return;
    }
    wchar_t wname[TODO_STORE_BOARD_LEN];
    AskName(hdlg, GetLocalizedString(L"\u4fbf\u7b3e\u65b0\u540d\u79f0\uff1a",
                                     L"Rename board to:"),
            oldName, wname, TODO_STORE_BOARD_LEN);
    if (!wname[0]) return;
    char name[TODO_STORE_BOARD_LEN * 2];
    WideCharToMultiByte(CP_UTF8, 0, wname, -1, name, sizeof(name), NULL, NULL);
    if (_stricmp(name, oldName) == 0) return;
    BOOL wasVisible = TodoStickies_IsVisible(oldName);
    if (!TodoBoard_Rename(idx, name)) {
        MessageBoxW(hdlg,
                    GetLocalizedString(
                        L"\u91cd\u547d\u540d\u5931\u8d25\uff08\u540d\u79f0\u91cd\u590d\uff1f\uff09",
                        L"Rename failed (duplicate name?)"),
                    L"TODO", MB_ICONWARNING);
        return;
    }
    if (wasVisible) {
        TodoStickies_HideBoard(oldName);
        TodoStickies_ShowBoard(name);
    }
    TodoDlg_InitBoardCombo(hdlg);
    HWND c = GetDlgItem(hdlg, IDC_TODO_BOARD_SEL);
    if (c) SendMessageW(c, CB_SETCURSEL, (WPARAM)idx, 0);
    TodoDlg_SyncStickyButton(hdlg);
    SyncAddRowGate(hdlg);
    TodoDlg_RefreshList(hdlg);
}

static void DeleteBoard(HWND hdlg) {
    char name[TODO_STORE_BOARD_LEN];
    TodoDlg_GetBoard(hdlg, name, sizeof(name));
    int idx = TodoBoard_IndexByName(name);
    if (idx < 0) return;
    if (TodoBoard_IsSync(idx)) return;
    wchar_t wname[TODO_STORE_BOARD_LEN], msg[256];
    MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, TODO_STORE_BOARD_LEN);
    _snwprintf_s(msg, 256, _TRUNCATE,
                 L"\u5220\u9664\u4fbf\u7b3e\u201c%s\u201d\uff1f\u4efb\u52a1\u5c06\u79fb\u5230\u7b2c\u4e00\u4e2a\u4fbf\u7b3e\u3002",
                 wname);
    if (MessageBoxW(hdlg, msg, L"TODO", MB_OKCANCEL | MB_ICONWARNING) != IDOK)
        return;
    TodoStickies_HideBoard(name);
    TodoBoard_Remove(idx);
    TodoDlg_InitBoardCombo(hdlg);
    TodoDlg_SyncStickyButton(hdlg);
    SyncAddRowGate(hdlg);
    TodoDlg_RefreshList(hdlg);
}

static void ToggleBoardSticky(HWND hdlg) {
    char name[TODO_STORE_BOARD_LEN];
    TodoDlg_GetBoard(hdlg, name, sizeof(name));
    TodoStickies_ToggleBoard(name);
    TodoDlg_SyncStickyButton(hdlg);
}

/* L1: board management hides behind ... (popup menu at the button). */
static void BoardManageMenu(HWND hdlg) {
    HMENU m = CreatePopupMenu();
    if (!m) return;
    AppendMenuW(m, MF_STRING, 1,
                GetLocalizedString(L"\u65B0\u5EFA\u4FBF\u7B7E...",
                                   L"New board..."));
    AppendMenuW(m, MF_STRING, 2,
                GetLocalizedString(L"\u91CD\u547D\u540D\u4FBF\u7B7E...",
                                   L"Rename board..."));
    AppendMenuW(m, MF_STRING, 3,
                GetLocalizedString(L"\u5220\u9664\u4FBF\u7B7E",
                                   L"Delete board"));
    HWND btn = GetDlgItem(hdlg, IDC_TODO_BOARD_MORE);
    RECT br = {0, 0, 0, 0};
    if (btn) GetWindowRect(btn, &br);
    int cmd = (int)TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                  br.left, br.top, 0, hdlg, NULL);
    DestroyMenu(m);
    if (cmd == 1) AddBoard(hdlg);
    else if (cmd == 2) RenameBoard(hdlg);
    else if (cmd == 3) DeleteBoard(hdlg);
}

/* TRUE when the command id belonged to the board row. */
BOOL TodoDlg_BoardCommand(HWND hdlg, WORD id, WORD code) {
    switch (id) {
    case IDC_TODO_BOARD_MORE: BoardManageMenu(hdlg); return TRUE;
    case IDC_TODO_BOARD_SHOW: ToggleBoardSticky(hdlg); return TRUE;
    case IDC_TODO_BOARD_SEL:
        /* CBN_SELCHANGE only: ignore paint-time focus notifications */
        if (code != CBN_SELCHANGE) return TRUE;
        TodoDlg_SyncStickyButton(hdlg);
        SyncAddRowGate(hdlg);
        TodoDlg_SyncScopeCombo(hdlg);
        TodoDlg_RefreshList(hdlg);
        return TRUE;
    default: return FALSE;
    }
}
