/**
 * @file window_commands_todo.c
 * @brief TODO tray command handlers: list/new/sticky/settings/sync-now.
 */
#include "window_commands_internal.h"
#include "dialog/dialog_todo.h"
#include "todo/todo_store.h"
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
    /* create placeholder task, pin it, open list for rename */
    if (TodoStore_Add("New note", TODO_IMPORTANCE_NONE, "")) {
        TodoFilter f;
        TodoFilter_InitDefault(&f);
        f.showDone = TRUE;
        f.showSync = FALSE;
        TodoTask buf[TODO_STORE_MAX_TASKS];
        int n = TodoStore_Query(&f, buf, TODO_STORE_MAX_TASKS);
        for (int i = n - 1; i >= 0; i--) {
            if (strcmp(buf[i].title, "New note") == 0) {
                TodoSticky_SetPinned(buf[i].id, TRUE);
                break;
            }
        }
    }
    ShowTodoListDialog(hwnd);
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
