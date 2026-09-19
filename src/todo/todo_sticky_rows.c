/**
 * @file todo_sticky_rows.c
 * @brief Sticky expanded rows: row-index -> task-id map for id-keyed menu.
 *
 * Single-task sticky: row 0 = the bound task.
 * Rows are rebuilt on every paint (order = store snapshot order).
 * Hit test: body line height 20px starting at BAR_H+30.
 */
#include <string.h>

#include "todo_store.h"

#include "todo_sticky_rows.h"

#define STICKY_ROW_H 20
#define STICKY_BODY_TOP 56

int TodoStickyRow_Build(const char *taskId, char ids[][TODO_STORE_ID_LEN],
                        int cap) {
    if (!ids || cap <= 0) return 0;
    if (!taskId || !taskId[0]) return 0;
    /* single-task sticky today: exactly one row */
    strcpy_s(ids[0], TODO_STORE_ID_LEN, taskId);
    (void)cap;
    return 1;
}

int TodoStickyRow_Hit(int y) {
    int r = (y - STICKY_BODY_TOP) / STICKY_ROW_H;
    return r < 0 ? -1 : r;
}

int TodoStickyRow_Top(int row) {
    return STICKY_BODY_TOP + row * STICKY_ROW_H;
}
