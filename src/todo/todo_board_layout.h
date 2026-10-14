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

/* Task row index under a client-space y, or -1 outside the body.
 * This is the raw band index and knows nothing about how many rows were
 * actually painted; use BoardLayout_HitRow for hit testing. */
int BoardLayout_RowAt(int y);
int BoardLayout_RowTop(int row);

/* Layout decision for a card of client height cardH holding count tasks.
 * Paint and hit testing both derive their geometry from this, so the
 * painted '…还有 N 项' overflow band can never be mistaken for a task row. */
typedef struct {
    int painted;   /* leading tasks drawn as task rows */
    int more;      /* tasks announced by the overflow band (0 = none) */
    int bandTop;   /* overflow band top, valid only when band is TRUE */
    int band;      /* nonzero when the overflow band is drawn at all */
} BoardLayoutRows;

BoardLayoutRows BoardLayout_ComputeRows(int cardH, int count);

/* Hit test a client-space y in a card of client height cardH holding
 * count tasks. more is set when the y landed on the overflow band, in
 * which case row is -1.
 * Returns the task row index, or -1 when the y is not a task row. */
int BoardLayout_HitRow(int y, int cardH, int count, int *more);

#endif /* CATIME_TODO_BOARD_LAYOUT_H */
