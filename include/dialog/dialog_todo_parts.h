/**
 * @file dialog_todo_parts.h
 * @brief Cross-TU declarations for split TODO-list dialog parts.
 *
 * dialog_todo_list.c owns TodoDlg_State + Preselect buffer + proc;
 * dialog_todo_scope/combos/refresh implement the parts below.
 */
#ifndef CATIME_DIALOG_TODO_PARTS_H
#define CATIME_DIALOG_TODO_PARTS_H

#include <windows.h>

#include "todo/todo_store.h"
#include "dialog/dialog_todo_list_state.h"

void TodoDlg_ApplyScopeRange(TodoFilter *f);
void TodoDlg_InitCombos(HWND hdlg);
void TodoDlg_RefreshInto(HWND hdlg);
char *TodoDlg_PreselectBuf(void);
TodoDlgState *TodoDlg_State(void);

#endif /* CATIME_DIALOG_TODO_PARTS_H */
