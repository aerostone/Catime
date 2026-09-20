/**
 * @file todo_stickies_slot.h
 * @brief Internal sticky window slot table shared by window + lifecycle TUs.
 *
 * Internal header (not installed). The slot array lives in
 * todo_stickies_window.c; todo_stickies_lifecycle.c operates on it only
 * through these accessors so no TU touches another TU's statics.
 */
#ifndef CATIME_TODO_STICKIES_SLOT_H
#define CATIME_TODO_STICKIES_SLOT_H

#include <windows.h>

#include "todo/todo_types.h"

#define STICKY_SLOT_MAX_WIN 16
/* Collapsed dot mode: window shrinks to this square (px). */
#define STICKY_DOT_SIZE 32

typedef struct {
    BOOL used;
    HWND hwnd;
    HWND edit; /* retired: kept for struct layout compat, always NULL */
    char taskId[TODO_STORE_ID_LEN];
    BOOL dragging;
    POINT dragOff;
    HFONT fTitle;
    HFONT fBody;
    BOOL collapsed;       /* dot mode */
    int expandH;          /* remembered height for expand */
    int expandW;          /* remembered width for expand */
    int topmostOverride;  /* -1 inherit global, 0 normal, 1 topmost */
    char pomoTaskId[TODO_STORE_ID_LEN]; /* bound pomo row id ("" = none) */
} StickyWin;

int TodoSticky_SlotCount(void);
StickyWin *TodoSticky_SlotAt(int index);
StickyWin *TodoSticky_SlotById(const char *id);
StickyWin *TodoSticky_SlotByHwnd(HWND h);
void TodoStickies_Show(const char *taskId);
void TodoSticky_RunMenuById(HWND hwnd, StickyWin *sw, unsigned cmd);
void TodoSticky_ShowRowMenu(HWND hwnd, StickyWin *sw);
void TodoSticky_SetCollapsedUI(HWND hwnd, StickyWin *sw, BOOL collapsed);
void TodoSticky_ApplyTopmost(HWND hwnd, StickyWin *sw);
HWND FindCatimeMainWindow(void);

#endif /* CATIME_TODO_STICKIES_SLOT_H */
