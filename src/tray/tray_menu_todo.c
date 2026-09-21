/**
 * @file tray_menu_todo.c
 * @brief TODO submenu: open tasks, board stickies, sync state, settings.
 *
 * Layout: "TODO (n)" popup -> header info row, top 3 open tasks
 * + overflow row (T1: menu stays under 8 rows)
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
                GetLocalizedString(L"\u663e\u793a\u5168\u90e8\u4efb\u52a1\u672c",
                                   L"Show all task books"));
    if (!AppendMenuW(hTodo, MF_STRING | MF_POPUP, (UINT_PTR)sub,
                     GetLocalizedString(L"\u4efb\u52a1\u672c", L"Task books")))
        DestroyMenu(sub);
}

void BuildTodoMenu(HMENU hMenu) {
    /* RD4: no task rows, no New Task, no Task List duplicate, no TODO
     * settings. Three entries only: task manager, task-book manager,
     * show-task-books submenu; then the sync/conflict status rows. */
    if (!hMenu) return;
    int open = TodoStore_OpenCount();

    HMENU hTodo = CreatePopupMenu();
    if (!hTodo) return;

    wchar_t head[128];
    _snwprintf_s(head, _countof(head), _TRUNCATE,
                 L"TODO (%d \u5F00\u653E)", open);
    AppendMenuW(hTodo, MF_STRING | MF_DISABLED | MF_GRAYED, 0, head);
    AppendMenuW(hTodo, MF_SEPARATOR, 0, NULL);

    AppendMenuW(hTodo, MF_STRING, CLOCK_IDM_TODO_LIST,
                GetLocalizedString(L"\u4EFB\u52A1\u7BA1\u7406\u2026",
                                   L"Task manager..."));
    AppendMenuW(hTodo, MF_STRING, CLOCK_IDM_TODO_BOARDS,
                GetLocalizedString(L"\u4EFB\u52A1\u672C\u7BA1\u7406\u2026",
                                   L"Task-book manager..."));
    BuildStickySubmenu(hTodo);
    AppendMenuW(hTodo, MF_SEPARATOR, 0, NULL);
    wchar_t stLabel[96];
    TodoSyncStatus_Label(stLabel, _countof(stLabel));
    AppendMenuW(hTodo, MF_STRING | MF_DISABLED | MF_GRAYED, 0, stLabel);
    int conflicts = TodoConflict_Count();
    if (conflicts > 0) {
        wchar_t cf[96];
        _snwprintf_s(cf, _countof(cf), _TRUNCATE,
                     L"\u51B2\u7A81 (%d) \u00B7 \u6253\u5F00\u76EE\u5F55",
                     conflicts);
        AppendMenuW(hTodo, MF_STRING, TODO_MENU_CONFLICT_BASE, cf);
    }

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
