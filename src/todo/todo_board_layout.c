/**
 * @file todo_board_layout.c
 * @brief Row hit-test helpers shared by board paint and WndProc.
 */
#include "todo_board_layout.h"

int BoardLayout_RowAt(int y) {
    if (y < BOARD_BODY_TOP) return -1;
    return (y - BOARD_BODY_TOP) / BOARD_ROW_H;
}

int BoardLayout_RowTop(int row) {
    return BOARD_BODY_TOP + row * BOARD_ROW_H;
}
