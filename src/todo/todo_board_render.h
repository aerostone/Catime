/**
 * @file todo_board_render.h
 * @brief Board card painting entry points (paint side of the layout).
 */
#ifndef CATIME_TODO_BOARD_RENDER_H
#define CATIME_TODO_BOARD_RENDER_H

#include <windows.h>

#include "todo/todo_types.h"

void TodoBoard_PaintTitle(HDC hdc, const RECT *rc, const char *board,
                          int openCount, BOOL collapsed, int pomoRemSec,
                          HFONT fTitle);
void TodoBoard_PaintFilter(HDC hdc, const RECT *rc, TodoDueScope scope,
                           const char *keyword, HFONT fBody);
void TodoBoard_PaintRows(HDC hdc, const RECT *rc, const TodoTask *tasks,
                         int count, HFONT fBody);
void TodoBoard_BarColor(TodoImportance imp, BOOL done, COLORREF *out);

#endif /* CATIME_TODO_BOARD_RENDER_H */
