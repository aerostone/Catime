/**
 * @file todo_stickies_edit.c
 * @brief Sticky inline title editor: open/commit/close helpers.
 *
 * Split from todo_stickies_window.c to respect the 300-line gate.
 * Store update uses remove+add to keep the TodoStore API surface small;
 * the window module remaps its task id after commit.
 */
#include <string.h>

#include "todo_store.h"

BOOL TodoStickyEdit_Commit(const char *oldId, const wchar_t *newTitle,
                           char *newIdOut, size_t newIdCap) {
    if (!oldId || !oldId[0] || !newTitle || !newTitle[0]) return FALSE;
    char utf8[TODO_STORE_TITLE_LEN] = "";
    if (!WideCharToMultiByte(CP_UTF8, 0, newTitle, -1,
                             utf8, sizeof(utf8), NULL, NULL))
        return FALSE;
    if (!utf8[0]) return FALSE;
    TodoFilter f;
    TodoFilter_InitDefault(&f);
    f.showDone = TRUE;
    f.showSync = FALSE;
    TodoTask buf[TODO_STORE_MAX_TASKS];
    int n = TodoStore_Query(&f, buf, TODO_STORE_MAX_TASKS);
    TodoTask old;
    BOOL found = FALSE;
    for (int i = 0; i < n; i++) {
        if (strcmp(buf[i].id, oldId) == 0) {
            old = buf[i];
            found = TRUE;
            break;
        }
    }
    if (!found) return FALSE;
    BOOL wasDone = old.done;
    TodoImportance imp = old.importance;
    char due[TODO_STORE_DATE_LEN];
    strcpy_s(due, sizeof(due), old.dueDate);
    TodoStore_Remove(oldId);
    if (!TodoStore_Add(utf8, imp, due)) return FALSE;
    /* newest task with same title is ours (ids increase monotonically) */
    n = TodoStore_Query(&f, buf, TODO_STORE_MAX_TASKS);
    for (int i = n - 1; i >= 0; i--) {
        if (strcmp(buf[i].title, utf8) == 0) {
            if (wasDone) TodoStore_SetDone(buf[i].id, TRUE);
            if (newIdOut && newIdCap)
                strcpy_s(newIdOut, newIdCap, buf[i].id);
            return TRUE;
        }
    }
    return FALSE;
}

#define STICKY_EDIT_ID 9001
#define STICKY_BAR_H 26

void TodoStickyEdit_Open(HWND parent, const char *taskId, HWND *editOut) {
    if (!parent || !taskId || !editOut) return;
    *editOut = NULL;
    TodoFilter f;
    TodoFilter_InitDefault(&f);
    f.showDone = TRUE;
    f.showSync = FALSE;
    TodoTask buf[TODO_STORE_MAX_TASKS];
    int n = TodoStore_Query(&f, buf, TODO_STORE_MAX_TASKS);
    const char *title = NULL;
    for (int i = 0; i < n; i++) {
        if (strcmp(buf[i].id, taskId) == 0) {
            title = buf[i].title;
            break;
        }
    }
    if (!title) return;
    RECT rc;
    GetClientRect(parent, &rc);
    rc.top += STICKY_BAR_H + 4;
    rc.left += 4; rc.right -= 4; rc.bottom -= 4;
    HWND edit = CreateWindowExW(0, L"EDIT", NULL,
                                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL,
                                rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                                parent, (HMENU)(INT_PTR)STICKY_EDIT_ID,
                                GetModuleHandleW(NULL), NULL);
    if (!edit) return;
    wchar_t wt[TODO_STORE_TITLE_LEN];
    if (MultiByteToWideChar(CP_UTF8, 0, title, -1, wt, _countof(wt)))
        SetWindowTextW(edit, wt);
    SetFocus(edit);
    *editOut = edit;
}
