/**
 * @file dialog_todo.h
 * @brief TODO list + settings dialog entry points.
 */
#ifndef CATIME_DIALOG_TODO_H
#define CATIME_DIALOG_TODO_H

#include <windows.h>

#include "todo/todo_store.h"

void ShowTodoListDialog(HWND hwndParent);
/* Open the list bound to one board (addNew focuses the title edit). */
void ShowTodoListDialogForBoard(HWND hwndParent, const char *board,
                                BOOL addNew);
void ShowTodoListDialogForNew(HWND hwndParent);
void TodoDlg_Preselect(const char *taskId);
void TodoDlg_ReadFilter(HWND hdlg, TodoFilter *f);
void ShowTodoSettingsDialog(HWND hwndParent);
void ShowTodoBooksDialog(HWND hwndParent); /* RD3: task-book manager */

#endif /* CATIME_DIALOG_TODO_H */
