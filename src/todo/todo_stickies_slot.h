/**
 * @file todo_stickies_slot.h
 * @brief Sticky board window slot table (internal header).
 *
 * One slot per visible board card. Slot storage lives in
 * todo_stickies_lifecycle.c so every other sticky TU shares it.
 */
#ifndef CATIME_TODO_STICKIES_SLOT_H
#define CATIME_TODO_STICKIES_SLOT_H

#include <windows.h>

#include "todo/todo_types.h"

#define STICKY_SLOT_MAX_WIN 16
#define STICKY_EDIT_ID 9001 /* keyword filter edit */

/* Context-menu command ids (card-local, not dialog ids). */
#define STICKY_CMD_NEW_TASK 9101
#define STICKY_CMD_OPEN_LIST 9102
#define STICKY_CMD_SCOPE 9103
#define STICKY_CMD_FOLD 9104
#define STICKY_CMD_TOPMOST 9105
#define STICKY_CMD_HIDE 9106
#define STICKY_CMD_ROW_DONE 9111
#define STICKY_CMD_ROW_POMO 9112
#define STICKY_CMD_ROW_DUE 9113
#define STICKY_CMD_ROW_IMP 9114
#define STICKY_CMD_ROW_MOVE 9120 /* + board index */
#define STICKY_CMD_ROW_DELETE 9121

typedef struct {
    BOOL used;
    HWND hwnd;
    HWND edit; /* keyword filter (child control) */
    char board[TODO_STORE_BOARD_LEN];
    HFONT fTitle;
    HFONT fBody;
    BOOL collapsed;
    BOOL dragging;
    POINT dragOff;
    int expandW, expandH; /* geometry to restore when expanding */
    char menuTaskId[TODO_STORE_ID_LEN]; /* row under the last right-click */
} StickyWin;

int TodoSticky_SlotCount(void);
StickyWin *TodoSticky_SlotAt(int index);
StickyWin *TodoSticky_SlotByBoard(const char *board);
StickyWin *TodoSticky_SlotByHwnd(HWND hwnd);
StickyWin *TodoSticky_SlotAlloc(void);

/* todo_stickies_window.c */
void TodoSticky_SetCollapsedUI(HWND hwnd, StickyWin *sw, BOOL collapsed);
void TodoSticky_Repaint(StickyWin *sw);
void TodoSticky_LayoutChildren(StickyWin *sw);
void TodoSticky_SyncEditFromBoard(StickyWin *sw);
/* todo_sticky_rows.c: board queries shared by window + create TUs. */
int TodoSticky_LoadBoardTasks(const char *board, TodoTask *out, int cap);
int TodoSticky_PomoRemainingFor(const char *board);
/* task id of a visible row ("" when empty). */
int TodoSticky_RowIdAt(const char *board, int row, char *out, size_t cap);
int TodoSticky_RowCount(const char *board);
/* todo_sticky_rowmenu.c */
void TodoSticky_ShowRowMenu(HWND hwnd, StickyWin *sw, int row);
void TodoSticky_ShowCardMenu(HWND hwnd, StickyWin *sw);
void TodoSticky_RowMenuCommand(HWND hwnd, StickyWin *sw, UINT cmd);

#endif /* CATIME_TODO_STICKIES_SLOT_H */
