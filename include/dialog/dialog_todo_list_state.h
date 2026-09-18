/**
 * @file dialog_todo_list_state.h
 * @brief Shared snapshot type for the TODO list dialog + commands.
 */
#ifndef CATIME_DIALOG_TODO_LIST_STATE_H
#define CATIME_DIALOG_TODO_LIST_STATE_H

#include <windows.h>

#include "todo/todo_store.h"

#define TODO_DLG_MAX_ROWS 200

typedef struct {
    TodoFilter filter;
    TodoTask rows[TODO_DLG_MAX_ROWS];
    int rowCount;
    int selected; /* index into rows, -1 none */
} TodoDlgState;

#endif /* CATIME_DIALOG_TODO_LIST_STATE_H */
