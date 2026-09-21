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
#include <commctrl.h> /* WC_EDITW for the keyword filter child edit */

#include "todo/todo_types.h"

#define STICKY_SLOT_MAX_WIN 16
#define STICKY_EDIT_ID 9001 /* keyword filter edit */
#define STICKY_TIMER_POMO 9201 /* 1s tick while a pomodoro is bound */
#define STICKY_TIMER_KW 9202 /* keyword persistence debounce (ms 300) */

/* Context-menu command ids (card-local, not dialog ids). */
#define STICKY_CMD_NEW_TASK 9101
#define STICKY_CMD_OPEN_LIST 9102
#define STICKY_CMD_SCOPE 9103
#define STICKY_CMD_FOLD 9104
#define STICKY_CMD_TOPMOST 9105
#define STICKY_CMD_HIDE 9106
#define STICKY_CMD_ROW_DONE 9111
#define STICKY_CMD_ROW_POMO 9112
#define STICKY_CMD_ROW_EDIT 9113 /* N3: honest jump to the list editor */
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
    char selId[TODO_STORE_ID_LEN]; /* selected row (single-select, N1/N5) */
    int hoverRow; /* hovered row for click affordance, -1 = none (N1) */
} StickyWin;

int TodoSticky_SlotCount(void);
StickyWin *TodoSticky_SlotAt(int index);
StickyWin *TodoSticky_SlotByBoard(const char *board);
StickyWin *TodoSticky_SlotByHwnd(HWND hwnd);
StickyWin *TodoSticky_SlotAlloc(void);

/* todo_stickies_window.c: class name + WndProc + runtime flag. */
#define STICKY_CLASS L"CatimeStickyClass"
extern BOOL g_stickyClassReg;
LRESULT CALLBACK StickyProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
void TodoSticky_SetCollapsedUI(HWND hwnd, StickyWin *sw, BOOL collapsed);
void TodoSticky_Repaint(StickyWin *sw);
void TodoSticky_LayoutChildren(StickyWin *sw);
void TodoSticky_SyncEditFromBoard(StickyWin *sw);
/* todo_sticky_select.c: selection model (N1/N5). */
void TodoSticky_PaintFocus(HDC hdc, const RECT *rc, StickyWin *sw,
                           HWND self);
void TodoSticky_MoveSelection(StickyWin *sw, int dir);
void TodoSticky_ToggleSelected(StickyWin *sw);
/* todo_sticky_rows.c: board queries shared by window + create TUs. */
int TodoSticky_LoadBoardTasks(const char *board, TodoTask *out, int cap);
int TodoSticky_PomoRemainingFor(const char *board);
/* task id of a visible row ("" when empty). */
int TodoSticky_RowIdAt(const char *board, int row, char *out, size_t cap);
TodoImportance TodoSticky_PeakImportance(const char *board);
int TodoSticky_RowCount(const char *board);
/* todo_sticky_rowmenu.c */
void TodoSticky_ShowRowMenu(HWND hwnd, StickyWin *sw, int row);
void TodoSticky_ShowCardMenu(HWND hwnd, StickyWin *sw);
void TodoSticky_RowMenuCommand(HWND hwnd, StickyWin *sw, UINT cmd);

#endif /* CATIME_TODO_STICKIES_SLOT_H */
