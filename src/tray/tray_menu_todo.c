/**
 * @file tray_menu_todo.c
 * @brief TODO submenu: open tasks, board stickies, sync state, settings.
 *
 * Layout: "TODO (n)" popup -> header info row, top 5 open tasks
 * (click = open the list with that row preselected, never destructive),
 * New Task..., Stickies submenu (per-board show/hide, persisted),
 * Task list..., then sync status / conflicts / Sync now / Settings.
 * Row marks come from TodoRowMark ([A]/[B]/[C]).
 */
#include <stdio.h>
#include <string.h>

#include "tray/tray_menu_todo.h"
#include "../../resource/resource.h"
#include "dialog/dialog_todo.h"
#include "language.h"
#include "todo/todo_board.h"
#include "todo/todo_conflict.h"
#include "todo/todo_rowmark.h"
#include "todo/todo_stickies.h"
#include "todo/todo_store.h"
#include "todo/todo_sync.h"
#include "todo/todo_sync_status.h"

#include "window_procedure/window_commands.h"

/* Row text: mark + optional due date + title + board tag (skipped for the default board). */
static void RowLabel(const TodoTask *t, wchar_t *out, size_t cap) {
    char row[288];
    const char *tag = t->board[0] && strcmp(t->board, TODO_BOARD_DEFAULT) != 0
                          ? t->board
                          : "";
    if (tag[0]) {
        if (t->dueDate[0])
            _snprintf_s(row, sizeof(row), _TRUNCATE, "%s %s %s \u00b7%s",
                        TodoRowMark(t), t->dueDate, t->title, tag);
        else
            _snprintf_s(row, sizeof(row), _TRUNCATE, "%s %s \u00b7%s",
                        TodoRowMark(t), t->title, tag);
    } else if (t->dueDate[0]) {
        _snprintf_s(row, sizeof(row), _TRUNCATE, "%s %s %s", TodoRowMark(t),
                    t->dueDate, t->title);
    } else {
        _snprintf_s(row, sizeof(row), _TRUNCATE, "%s %s", TodoRowMark(t),
                    t->title);
    }
    wchar_t wr[288];
    if (MultiByteToWideChar(CP_UTF8, 0, row, -1, wr, _countof(wr)))
        wcsncpy_s(out, cap, wr, _TRUNCATE);
    else
        out[0] = L'\0';
    if (cap) out[cap - 1] = L'\0';
}

/* Checkable list of boards: click toggles that board's desktop card. */
static void BuildStickySubmenu(HMENU hTodo) {
    HMENU sub = CreatePopupMenu();
    if (!sub) return;
    int n = TodoBoard_Count();
    for (int i = 0; i < n; i++) {
        const char *name = TodoBoard_NameAt(i);
        if (!name[0]) continue;
        wchar_t wname[TODO_STORE_BOARD_LEN];
        MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, TODO_STORE_BOARD_LEN);
        UINT flags = MF_STRING | (TodoStickies_IsVisible(name) ? MF_CHECKED : 0);
        AppendMenuW(sub, flags, (UINT_PTR)(TODO_MENU_BOARD_BASE + i), wname);
    }
    if (GetMenuItemCount(sub) == 0) {
        DestroyMenu(sub);
        return;
    }
    AppendMenuW(sub, MF_SEPARATOR, 0, NULL);
    AppendMenuW(sub, MF_STRING, CLOCK_IDM_TODO_SHOW_ALL,
                GetLocalizedString(L"\u663e\u793a\u5168\u90e8\u4fbf\u7b7e",
                                   L"Show all stickies"));
    if (!AppendMenuW(hTodo, MF_STRING | MF_POPUP, (UINT_PTR)sub,
                     GetLocalizedString(L"\u4fbf\u7b7e", L"Stickies")))
        DestroyMenu(sub);
}

void BuildTodoMenu(HMENU hMenu) {
    if (!hMenu) return;
    TodoFilter f;
    TodoFilter_InitDefault(&f);
    f.showDone = FALSE;
    TodoTask tasks[TODO_MENU_TASK_COUNT];
    int n = TodoStore_Query(&f, tasks, TODO_MENU_TASK_COUNT);
    int open = TodoStore_OpenCount();

    HMENU hTodo = CreatePopupMenu();
    if (!hTodo) return;

    /* header: info only, disabled */
    wchar_t head[128];
    _snwprintf_s(head, _countof(head), _TRUNCATE,
                 L"%d open \u00b7 click a task to open the list", open);
    AppendMenuW(hTodo, MF_STRING | MF_DISABLED | MF_GRAYED, 0, head);
    AppendMenuW(hTodo, MF_SEPARATOR, 0, NULL);

    for (int i = 0; i < n; i++) {
        wchar_t item[176];
        RowLabel(&tasks[i], item, _countof(item));
        AppendMenuW(hTodo, MF_STRING, TODO_MENU_TASK_BASE + i, item);
    }
    if (n > 0) AppendMenuW(hTodo, MF_SEPARATOR, 0, NULL);

    AppendMenuW(hTodo, MF_STRING, CLOCK_IDM_TODO_NEW,
                GetLocalizedString(L"\u65b0\u5efa\u4efb\u52a1\u2026",
                                   L"New Task..."));
    BuildStickySubmenu(hTodo);
    AppendMenuW(hTodo, MF_STRING, CLOCK_IDM_TODO_LIST,
                GetLocalizedString(L"\u4efb\u52a1\u5217\u8868\u2026",
                                   L"Task List..."));
    AppendMenuW(hTodo, MF_SEPARATOR, 0, NULL);
    wchar_t stLabel[96];
    TodoSyncStatus_Label(stLabel, _countof(stLabel));
    AppendMenuW(hTodo, MF_STRING | MF_DISABLED | MF_GRAYED, 0, stLabel);
    int conflicts = TodoConflict_Count();
    if (conflicts > 0) {
        wchar_t cf[96];
        _snwprintf_s(cf, _countof(cf), _TRUNCATE,
                     L"\u51b2\u7a81 (%d) \u00b7 \u6253\u5f00\u76ee\u5f55",
                     conflicts);
        AppendMenuW(hTodo, MF_STRING, TODO_MENU_CONFLICT_BASE, cf);
    }
    AppendMenuW(hTodo, MF_STRING, CLOCK_IDM_TODO_SYNC_NOW,
                GetLocalizedString(L"\u7acb\u5373\u540c\u6b65", L"Sync Now"));
    AppendMenuW(hTodo, MF_STRING, CLOCK_IDM_TODO_SETTINGS,
                GetLocalizedString(L"TODO \u8bbe\u7f6e\u2026",
                                   L"TODO Settings..."));

    wchar_t label[64];
    if (conflicts > 0)
        _snwprintf_s(label, _countof(label), _TRUNCATE, L"%s (%d)(!)",
                     GetLocalizedString(L"TODO", L"TODO"), open);
    else
        _snwprintf_s(label, _countof(label), _TRUNCATE, L"%s (%d)",
                     GetLocalizedString(L"TODO", L"TODO"), open);
    if (!AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTodo, label)) {
        DestroyMenu(hTodo);
    }
}

/* Range handler: top-5 task row click -> open list preselected (A1 fix).
 * Never destructive from the tray: completion lives in list/sticky. */
BOOL HandleTodoMenuRow(HWND hwnd, UINT cmd, int index) {
    (void)cmd;
    TodoFilter f;
    TodoFilter_InitDefault(&f);
    f.showDone = FALSE;
    TodoTask tasks[TODO_MENU_TASK_COUNT];
    int n = TodoStore_Query(&f, tasks, TODO_MENU_TASK_COUNT);
    if (index < 0 || index >= n) return TRUE;
    TodoDlg_Preselect(tasks[index].id);
    /* open the list already bound to that task's board */
    if (hwnd) {
        if (tasks[index].board[0])
            ShowTodoListDialogForBoard(hwnd, tasks[index].board, FALSE);
        else
            ShowTodoListDialog(hwnd);
    }
    return TRUE;
}

/* Conflict row: open the todo dir in explorer. */
BOOL HandleTodoMenuConflict(HWND hwnd, UINT cmd, int index) {
    (void)cmd;
    (void)index;
    TodoConflict_OpenDir(hwnd);
    return TRUE;
}

/* Board row: toggle that board's sticky card. */
BOOL HandleTodoMenuBoard(HWND hwnd, UINT cmd, int index) {
    (void)cmd;
    (void)hwnd;
    if (index < 0 || index >= TodoBoard_Count()) return TRUE;
    const char *name = TodoBoard_NameAt(index);
    if (name[0]) TodoStickies_ToggleBoard(name);
    return TRUE;
}

UINT TodoMenu_ConflictId(void) { return TODO_MENU_CONFLICT_BASE; }
