/**
 * @file todo_overlay_draw.h
 * @brief Draw cached TODO lines onto the main window memDC (before present).
 */
#ifndef CATIME_TODO_OVERLAY_DRAW_H
#define CATIME_TODO_OVERLAY_DRAW_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Draws up to 8 cached todo lines at bottom-left of memDC (w x h).
 * No-op when sync disabled or cache empty. Uses small 13px YaHei. */
void TodoOverlay_DrawOnMemDC(HDC memDC, int w, int h);

#ifdef __cplusplus
}
#endif

#endif /* CATIME_TODO_OVERLAY_DRAW_H */
