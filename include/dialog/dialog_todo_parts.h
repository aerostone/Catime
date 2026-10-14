/**
 * @file dialog_todo_parts.h
 * @brief Cross-TU declarations for split TODO-list dialog parts.
 *
 * dialog_todo_list.c owns TodoDlg_State + Preselect buffer + proc;
 * dialog_todo_scope/combos/refresh/boards implement the parts below.
 */
#ifndef CATIME_DIALOG_TODO_PARTS_H
#define CATIME_DIALOG_TODO_PARTS_H

#include <windows.h>

#include "todo/todo_store.h"
#include "dialog/dialog_todo_list_state.h"

/* EM_SETCUEBANNER with a Unicode string (Win7/commctrl.h spelling).
 * Shared here because both dialog_todo_combos.c and dialog_todo_boards.c
 * set placeholder text; putting it in one header avoids divergence. */
#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER (WM_USER + 1)
#endif
#ifndef EM_SETCUEBANNER_W
#define EM_SETCUEBANNER_W EM_SETCUEBANNER
#endif

void TodoDlg_ApplyScopeRange(TodoFilter *f);
void TodoDlg_InitCombos(HWND hdlg);
void TodoDlg_RefreshInto(HWND hdlg);
void TodoDlg_RefreshList(HWND hdlg);
char *TodoDlg_PreselectBuf(void);
TodoDlgState *TodoDlg_State(void);

/* dialog_todo_boards.c */
void TodoDlg_InitBoardCombo(HWND hdlg);
void TodoDlg_GetBoard(HWND hdlg, char *out, size_t cap);
void TodoDlg_SyncStickyButton(HWND hdlg);
/* Keep the Add row enabled on every book; the tweek book is bidirectional
 * and the hint is a cue banner, never edit content (see SyncAddRowGate). */
void TodoDlg_RefreshAddGate(HWND hdlg);
void TodoDlg_ApplyBoardFilter(HWND hdlg, TodoFilter *f);
BOOL TodoDlg_BoardCommand(HWND hdlg, WORD id, WORD code);
/* Keep the list scope combo aligned with the selected board's scope and
 * persist in-dialog scope changes back to the board. */
void TodoDlg_SyncScopeCombo(HWND hdlg);
void TodoDlg_PersistScope(HWND hdlg, TodoDueScope scope);

/* dialog_todo_list_cmd.c: task actions on the current selection. */
void TodoDlg_OnAdd(HWND hdlg);
void TodoDlg_OnToggleDone(HWND hdlg);
void TodoDlg_OnDelete(HWND hdlg);
void TodoDlg_OnSaveEdit(HWND hdlg); /* RD9 */
/* dialog_todo_duepick.c: DTS_SHOWNONE picker (unchecked = no date). */
void TodoDlg_GetDue(HWND hdlg, char *out, size_t cap);
void TodoDlg_SetDue(HWND hdlg, const char *dueDate);
void TodoDlg_OnSelect(HWND hdlg, int listIndex);

#endif /* CATIME_DIALOG_TODO_PARTS_H */