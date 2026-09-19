/**
 * @file dialog_todo.h
 * @brief TODO list + settings dialog entry points.
 */
#ifndef CATIME_DIALOG_TODO_H
#define CATIME_DIALOG_TODO_H

#include <windows.h>

#include "todo/todo_store.h"

void ShowTodoListDialog(HWND hwndParent);
void ShowTodoListDialogForNew(HWND hwndParent);
void TodoDlg_Preselect(const char *taskId);
void TodoDlg_ReadFilter(HWND hdlg, TodoFilter *f);
void ShowTodoSettingsDialog(HWND hwndParent);

#endif /* CATIME_DIALOG_TODO_H */
