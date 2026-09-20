/**
 * @file window_commands_todo.c
 * @brief TODO tray command handlers: list/new/sticky/settings/sync-now.
 */
#include "window_commands_internal.h"
#include "dialog/dialog_todo.h"
#include "todo/todo_stickies.h"
#include "todo/todo_sync.h"

LRESULT CmdTodoList(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ShowTodoListDialog(hwnd);
    return 0;
}

LRESULT CmdTodoNew(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    /* A2 fix: real difference -- open list focused on the title edit. */
    ShowTodoListDialogForNew(hwnd);
    return 0;
}

LRESULT CmdTodoNewSticky(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    /* "New task": open the task list focused on the title edit.
     * The list is the only create/edit entry; pinning shows the sticky. */
    ShowTodoListDialogForNew(hwnd);
    return 0;
}

LRESULT CmdTodoShowAll(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)hwnd; (void)wp; (void)lp;
    TodoStickies_ShowAll();
    return 0;
}

LRESULT CmdTodoSettings(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ShowTodoSettingsDialog(hwnd);
    return 0;
}

LRESULT CmdTodoSyncNow(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)hwnd; (void)wp; (void)lp;
    TodoSync_PollNow();
    return 0;
}
