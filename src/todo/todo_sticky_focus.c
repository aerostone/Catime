/**
 * @file todo_sticky_focus.c
 * @brief Selected-row focus rect for keyboard visibility (N5).
 *
 * Split from todo_stickies_window.c to respect the 300-line gate.
 */
#include <string.h>

#include "todo_board_layout.h"
#include "todo_store.h"

#include "todo_stickies_slot.h"

void TodoSticky_PaintFocus(HDC hdc, const RECT *rc, StickyWin *sw,
                           HWND self) {
    if (!hdc || !rc || !sw || sw->collapsed || !sw->selId[0]) return;
    if (GetFocus() != self) return;
    TodoTask tasks[TODO_STORE_MAX_TASKS];
    int n = TodoSticky_LoadBoardTasks(sw->board, tasks, TODO_STORE_MAX_TASKS);
    for (int i = 0; i < n; i++) {
        if (strcmp(tasks[i].id, sw->selId) == 0) {
            RECT fr = *rc;
            fr.top = BOARD_BODY_TOP + i * BOARD_ROW_H;
            fr.bottom = fr.top + BOARD_ROW_H;
            DrawFocusRect(hdc, &fr);
            break;
        }
    }
}
