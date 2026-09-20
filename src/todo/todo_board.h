/**
 * @file todo_board.h
 * @brief Sticky boards: named, persistent task groupings shown as cards.
 *
 * A board is a named group of tasks (`board:NAME` in todo.txt) plus a
 * saved view filter (scope + keyword). Exactly one sync board exists
 * (TODO_BOARD_SYNC) whose members arrive by pull; local boards are
 * created/renamed/removed in the task list dialog.
 *
 * Pure Win32, no global hooks (CatimeInteractionSafety compliant).
 */
#ifndef CATIME_TODO_BOARD_H
#define CATIME_TODO_BOARD_H

#include "todo/todo_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TODO_BOARD_MAX 8

/* Lifecycle: load board list from todo.ini, create the defaults. */
void TodoBoard_Init(void);
void TodoBoard_Shutdown(void);

/* Board list (ordered: local boards first, sync board last). */
int TodoBoard_Count(void);
const char *TodoBoard_NameAt(int index);
int TodoBoard_IndexByName(const char *name);

/* Create/rename/remove. Returns FALSE on duplicate/limit/reserved. */
BOOL TodoBoard_Add(const char *name);
BOOL TodoBoard_Rename(int index, const char *newName);
BOOL TodoBoard_Remove(int index);
BOOL TodoBoard_IsSync(int index);

/* Saved view filter (scope week/all + keyword), persisted per board. */
TodoDueScope TodoBoard_Scope(const char *name);
void TodoBoard_SetScope(const char *name, TodoDueScope scope);
void TodoBoard_Keyword(const char *name, char *out, size_t cap);
void TodoBoard_SetKeyword(const char *name, const char *keyword);

/* Visibility (drives the sticky window). */
BOOL TodoBoard_Visible(const char *name);
void TodoBoard_SetVisible(const char *name, BOOL visible);

/* Sticky geometry per board. */
void TodoBoard_LoadGeom(const char *name, int *x, int *y, int *w, int *h);
void TodoBoard_SaveGeom(const char *name, int x, int y, int w, int h);
BOOL TodoBoard_IsCollapsed(const char *name);
void TodoBoard_SetCollapsed(const char *name, BOOL collapsed);

/* Global sticky settings ([Sticky] section of the board ini). */
BOOL TodoBoard_TopmostGlobal(void);
void TodoBoard_SetTopmostGlobal(BOOL topmost);
/* Opacity 30..100 percent. */
int TodoBoard_Opacity(void);
void TodoBoard_SetOpacity(int pct);

/* Per-board topmost override: -1 = inherit global, 0 = normal, 1 = top. */
int TodoBoard_TopmostOverride(const char *name);
void TodoBoard_SetTopmostOverride(const char *name, int mode);
BOOL TodoBoard_TopmostFor(const char *name);

/* Tasks belonging to a board (store order, filtered). Returns count. */
int TodoBoard_Tasks(const char *name, TodoTask *out, int cap);

/* Number of open tasks on a board (badge/tray use). */
int TodoBoard_OpenCount(const char *name);

/* Move a task onto a board, stamping updatedAt. */
BOOL TodoBoard_AssignTask(const char *taskId, const char *boardName);

#ifdef __cplusplus
}
#endif

#endif /* CATIME_TODO_BOARD_H */
