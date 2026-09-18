/**
 * @file todo_stickies_lifecycle.c
 * @brief Sticky window lifecycle: hide/show-all/restore/forget/visible.
 *
 * Operates on the slot table in todo_stickies_window.c through
 * todo_stickies_slot.h accessors (no cross-TU static access).
 */
#include <string.h>

#include "todo_stickies.h"
#include "todo_store.h"
#include "todo_stickies_slot.h"

void TodoStickies_Hide(const char *taskId) {
    StickyWin *sw = TodoSticky_SlotById(taskId);
    if (!sw) return;
    ShowWindow(sw->hwnd, SW_HIDE);
}

void TodoStickies_HideAll(void) {
    for (int i = 0; i < TodoSticky_SlotCount(); i++) {
        StickyWin *sw = TodoSticky_SlotAt(i);
        if (sw && sw->used && sw->hwnd)
            ShowWindow(sw->hwnd, SW_HIDE);
    }
}

void TodoStickies_RestoreAll(void) {
    TodoFilter f;
    TodoTask buf[TODO_STORE_MAX_TASKS];
    TodoFilter_InitDefault(&f);
    int n = TodoStore_Query(&f, buf, TODO_STORE_MAX_TASKS);
    for (int i = 0; i < n; i++) {
        if (buf[i].done) continue;
        if (TodoSticky_IsPinned(buf[i].id))
            TodoStickies_Show(buf[i].id);
    }
}

void TodoStickies_Forget(const char *taskId) {
    StickyWin *sw = TodoSticky_SlotById(taskId);
    if (!sw) return;
    if (sw->hwnd) DestroyWindow(sw->hwnd);
}

BOOL TodoStickies_IsVisible(const char *taskId) {
    StickyWin *sw = TodoSticky_SlotById(taskId);
    if (!sw || !sw->hwnd) return FALSE;
    return IsWindowVisible(sw->hwnd);
}
