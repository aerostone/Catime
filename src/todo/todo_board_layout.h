/**
 * @file todo_board_layout.h
 * @brief Shared geometry for board card painting and hit testing.
 *
 * Kept in one place so the paint code (todo_board_render.c) and the
 * WndProc (todo_stickies_window.c) can never disagree about where a
 * row lives.
 */
#ifndef CATIME_TODO_BOARD_LAYOUT_H
#define CATIME_TODO_BOARD_LAYOUT_H

#define BOARD_BAR_H 30      /* title bar: drag / collapse / list / close */
#define BOARD_FILTER_H 26   /* scope toggle + keyword edit row */
#define BOARD_ROW_H 22      /* one task row */
#define BOARD_BODY_TOP (BOARD_BAR_H + BOARD_FILTER_H)
#define BOARD_BAR_COLLAPSED_W 150 /* collapsed card width */
#define BOARD_BAR_COLLAPSED_H (BOARD_BAR_H + 2) /* never clip the bar */

/* Title bar hit zones (x measured from the right edge). */
#define BOARD_HIT_CLOSE 18  /* rightmost 18px: hide sticky */
#define BOARD_HIT_LIST 20   /* next 20px: open task list for this board */
#define BOARD_HIT_FOLD 20   /* next 20px: toggle collapse */
#define BOARD_HIT_ADD_W 22  /* filter row: rightmost 22px = add task */

/* Filter row hit zones. */
#define BOARD_HIT_SCOPE_W 46 /* leftmost 46px: week/all toggle */

/* Task row index under a client-space y, or -1 outside the body. */
int BoardLayout_RowAt(int y);
int BoardLayout_RowTop(int row);

#endif /* CATIME_TODO_BOARD_LAYOUT_H */
