/**
 * @file todo_sticky_rows.h
 * @brief Sticky row map (internal header).
 */
#ifndef CATIME_TODO_STICKY_ROWS_H
#define CATIME_TODO_STICKY_ROWS_H

#include "todo/todo_types.h"

int TodoStickyRow_Build(const char *taskId, char ids[][TODO_STORE_ID_LEN],
                        int cap);
int TodoStickyRow_Hit(int y);
int TodoStickyRow_Top(int row);

#endif /* CATIME_TODO_STICKY_ROWS_H */
