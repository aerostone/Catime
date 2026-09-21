/**
 * @file todo_sticky_select.c
 * @brief Sticky card selection model (N1/N5): select/hover/keyboard-toggle.
 *
 * Split from todo_stickies_window.c to respect the 300-line gate.
 * Selection is a transient UI state (task id); done-toggles always go
 * through TodoStore_SetDone + repaint so paint and sync stay in sync.
 */
#include <string.h>

#include "todo_store.h"

#include "todo_stickies_slot.h"

void TodoSticky_MoveSelection(StickyWin *sw, int dir) {
    if (!sw) return;
    TodoTask tasks[TODO_STORE_MAX_TASKS];
    int n = TodoSticky_LoadBoardTasks(sw->board, tasks, TODO_STORE_MAX_TASKS);
    if (n <= 0) return;
    int cur = -1;
    for (int i = 0; i < n; i++) {
        if (strcmp(tasks[i].id, sw->selId) == 0) {
            cur = i;
            break;
        }
    }
    int next = (cur < 0) ? (dir > 0 ? 0 : n - 1) : (cur + dir + n) % n;
    strcpy_s(sw->selId, sizeof(sw->selId), tasks[next].id);
    TodoSticky_Repaint(sw);
}

void TodoSticky_ToggleSelected(StickyWin *sw) {
    if (!sw || !sw->selId[0]) return;
    TodoTask t;
    memset(&t, 0, sizeof(t));
    if (!TodoStore_FindById(sw->selId, &t)) {
        sw->selId[0] = '\0';
        TodoSticky_Repaint(sw);
        return;
    }
    TodoStore_SetDone(t.id, !t.done);
    TodoSticky_Repaint(sw);
}
