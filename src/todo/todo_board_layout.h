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

/* RD2: geometry is +30% over the old 30/26/22 baseline so the card
 * and its fonts read larger; DOT_* is the minimized translucent dot. */
#define BOARD_BAR_H 39      /* title bar: drag / fold / list */
#define BOARD_FILTER_H 34   /* scope toggle row (search removed, RD5) */
#define BOARD_ROW_H 29      /* one task row */
#define BOARD_BODY_TOP (BOARD_BAR_H + BOARD_FILTER_H)
#define BOARD_BAR_COLLAPSED_W 52 /* minimized dot width */
#define BOARD_BAR_COLLAPSED_H 52 /* minimized dot height */
#define BOARD_DOT_ALPHA 140 /* minimized dot translucency (0-255) */

/* Title bar hit zones (x measured from the right edge). RD6: an
 * explicit fold button toggles collapse; double-click is gone. */
#define BOARD_HIT_LIST 26   /* rightmost 26px: open task manager */
#define BOARD_HIT_FOLD 26   /* next 26px: fold/unfold button */

/* Filter row hit zones. */
#define BOARD_HIT_SCOPE_W 60 /* leftmost 60px: week/all toggle */

/* Task row index under a client-space y, or -1 outside the body. */
int BoardLayout_RowAt(int y);
int BoardLayout_RowTop(int row);

#endif /* CATIME_TODO_BOARD_LAYOUT_H */
