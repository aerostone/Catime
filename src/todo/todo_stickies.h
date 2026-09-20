/**
 * @file todo_stickies.h
 * @brief Sticky boards: one desktop card per board, always-on-top.
 *
 * A card shows a board's open tasks (see todo_board.h) with its own
 * 本周/全部 scope toggle and keyword filter. Position/size, visibility
 * and filter persist in todo.ini per board.
 * Pure Win32, no global hooks (CatimeInteractionSafety compliant).
 */
#ifndef CATIME_TODO_STICKIES_H
#define CATIME_TODO_STICKIES_H

#include <windows.h>

#include "todo/todo_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Show (or raise) the card of one board; creates the window if needed. */
void TodoStickies_ShowBoard(const char *board);
void TodoStickies_HideBoard(const char *board);
void TodoStickies_ToggleBoard(const char *board);
void TodoStickies_HideAll(void);
/* Show a card for every board (tray "show all stickies"). */
void TodoStickies_ShowAll(void);
/* Open cards for every board marked Visible=1 (called at startup). */
void TodoStickies_RestoreAll(void);
/* Repaint every open card (after task mutations). */
void TodoStickies_RefreshAll(void);
BOOL TodoStickies_IsVisible(const char *board);

/* Global sticky settings (per-board override wins in TodoBoard_TopmostFor). */
BOOL TodoSticky_TopmostGlobal(void);
void TodoSticky_SetTopmostGlobal(BOOL topmost);
int TodoSticky_Opacity(void);
void TodoSticky_SetOpacity(int pct);
void TodoSticky_ApplyOpacity(HWND hwnd);
void TodoSticky_RetopAll(void);
void TodoSticky_UpdateTips(HWND hwnd, BOOL collapsed);

#ifdef __cplusplus
}
#endif

#endif /* CATIME_TODO_STICKIES_H */
