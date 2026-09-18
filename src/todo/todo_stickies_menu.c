/**
 * @file todo_stickies_menu.c
 * @brief Sticky right-click menu commands (done/unpin/delete).
 */
#include <string.h>

#include "todo_stickies.h"
#include "todo_store.h"

#define STICKY_MENU_DONE 9101
#define STICKY_MENU_UNPIN 9102
#define STICKY_MENU_DELETE 9103

void TodoSticky_RunMenuCommand(HWND hwnd, const char *taskId,
                                      BOOL done, UINT cmd) {
    StickyWin *sw = FindByHwnd(hwnd);
    if (!sw || !taskId) return;
    if (cmd == STICKY_MENU_DONE) {
        TodoStore_SetDone(taskId, !done);
        InvalidateRect(hwnd, NULL, TRUE);
    } else if (cmd == STICKY_MENU_UNPIN) {
        TodoSticky_SetPinned(taskId, FALSE);
    } else if (cmd == STICKY_MENU_DELETE) {
        char id[TODO_STORE_ID_LEN];
        strcpy_s(id, sizeof(id), taskId);
        TodoStickies_Forget(id);
        TodoStore_Remove(id);
    }
}

