/**
 * @file todo_board_layout.c
 * @brief Row hit-test helpers shared by board paint and WndProc.
 *
 * Single source of truth for "which tasks become task rows, and where the
 * overflow band lives". The paint code and the WndProc both call
 * BoardLayout_ComputeRows, so a click on the overflow band ('...还有 N 项')
 * can never be confused with a click on the task row painted above it.
 */
#include "todo_board_layout.h"

int BoardLayout_RowAt(int y) {
    if (y < BOARD_BODY_TOP) return -1;
    return (y - BOARD_BODY_TOP) / BOARD_ROW_H;
}

int BoardLayout_RowTop(int row) {
    return BOARD_BODY_TOP + row * BOARD_ROW_H;
}

BoardLayoutRows BoardLayout_ComputeRows(int cardH, int count) {
    BoardLayoutRows r;
    r.painted = 0;
    r.more = 0;
    r.bandTop = 0;
    r.band = 0;
    if (cardH <= 0 || count <= 0) return r;

    /* Rows that fit fully inside the body. When the overflow band is drawn
     * it occupies the last BOARD_ROW_H strip of the card, so the task rows
     * have to stop above that strip. */
    int avail = cardH - BOARD_BODY_TOP;
    if (avail < BOARD_ROW_H) return r;
    int fit = avail / BOARD_ROW_H;
    if (fit > count) fit = count;

    if (fit < count) {
        int bandTop = cardH - BOARD_ROW_H;
        /* The band must never overlap a task row: shrink the painted rows
         * until the band sits at or below the last one. */
        while (fit > 1 && BoardLayout_RowTop(fit - 1) + BOARD_ROW_H > bandTop) {
            fit--;
        }
        /* At least one task row must remain visible; otherwise the card is
         * too small to show anything and the band is not drawn. */
        if (BoardLayout_RowTop(0) + BOARD_ROW_H <= bandTop && fit < count) {
            r.band = 1;
            r.bandTop = bandTop;
        }
    }
    r.painted = fit;
    r.more = count - fit;
    return r;
}

int BoardLayout_HitRow(int y, int cardH, int count, int *more) {
    if (more) *more = 0;
    if (y < BOARD_BODY_TOP) return -1;

    BoardLayoutRows lay = BoardLayout_ComputeRows(cardH, count);
    if (lay.band && y >= lay.bandTop) {
        if (more) *more = 1;
        return -1;
    }

    int row = (y - BOARD_BODY_TOP) / BOARD_ROW_H;
    /* Clicks below the last painted row belong to no task. */
    if (row < 0 || row >= lay.painted) return -1;
    return row;
}
