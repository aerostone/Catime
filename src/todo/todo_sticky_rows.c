/**
 * @file todo_sticky_rows.c
 * @brief Row map of a board card: visible row index -> task id.
 *
 * Rows are rebuilt from TodoBoard_Tasks on every query, so the mapping
 * can never drift from what was painted (same filter, same order).
 */
#include <string.h>

#include "todo_board.h"
#include "todo_store.h"

#include "todo_sticky_pomo.h"

#include "todo_stickies_slot.h"

int TodoSticky_LoadBoardTasks(const char *board, TodoTask *out, int cap) {
    return TodoBoard_Tasks(board, out, cap);
}

/* Remaining pomodoro seconds when the running pomo is bound to a task on
 * this board (0 otherwise, so cards without the bound row stay clean). */
int TodoSticky_PomoRemainingFor(const char *board) {
    const char *pid = TodoStickyPomo_TaskId();
    if (!pid || !pid[0]) return 0;
    TodoTask tasks[TODO_STORE_MAX_TASKS];
    int n = TodoSticky_LoadBoardTasks(board, tasks, TODO_STORE_MAX_TASKS);
    for (int i = 0; i < n; i++) {
        if (strcmp(tasks[i].id, pid) == 0) return TodoStickyPomo_Remaining();
    }
    return 0;
}

int TodoSticky_RowCount(const char *board) {
    return TodoBoard_OpenCount(board);
}

int TodoSticky_RowIdAt(const char *board, int row, char *out, size_t cap) {
    if (out && cap) out[0] = '\0';
    if (!board || !board[0] || row < 0 || !out || cap == 0) return 0;
    TodoTask tasks[TODO_STORE_MAX_TASKS];
    int n = TodoBoard_Tasks(board, tasks, TODO_STORE_MAX_TASKS);
    if (row >= n) return 0;
    strcpy_s(out, cap, tasks[row].id);
    return 1;
}
