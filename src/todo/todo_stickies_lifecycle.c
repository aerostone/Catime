/**
 * @file todo_stickies_lifecycle.c
 * @brief Sticky board lifecycle: slot table, show/hide, restore, topmost.
 *
 * One card per board. Visibility is persisted per board (todo.ini
 * [Board NAME] Visible=1) so boards come back on the next launch.
 */
#include <string.h>
#include <windows.h>

#include "todo_board.h"
#include "todo_stickies.h"

#include "todo_stickies_slot.h"

static StickyWin s_wins[STICKY_SLOT_MAX_WIN];

int TodoSticky_SlotCount(void) { return STICKY_SLOT_MAX_WIN; }

StickyWin *TodoSticky_SlotAt(int index) {
    if (index < 0 || index >= STICKY_SLOT_MAX_WIN) return NULL;
    return &s_wins[index];
}

StickyWin *TodoSticky_SlotByBoard(const char *board) {
    if (!board || !board[0]) return NULL;
    for (int i = 0; i < STICKY_SLOT_MAX_WIN; i++) {
        if (s_wins[i].used && strcmp(s_wins[i].board, board) == 0)
            return &s_wins[i];
    }
    return NULL;
}

StickyWin *TodoSticky_SlotByHwnd(HWND hwnd) {
    if (!hwnd) return NULL;
    for (int i = 0; i < STICKY_SLOT_MAX_WIN; i++) {
        if (s_wins[i].used && s_wins[i].hwnd == hwnd) return &s_wins[i];
    }
    return NULL;
}

StickyWin *TodoSticky_SlotAlloc(void) {
    for (int i = 0; i < STICKY_SLOT_MAX_WIN; i++) {
        if (!s_wins[i].used) return &s_wins[i];
    }
    return NULL;
}

BOOL TodoStickies_IsVisible(const char *board) {
    return TodoSticky_SlotByBoard(board) != NULL;
}

void TodoStickies_HideBoard(const char *board) {
    StickyWin *sw = TodoSticky_SlotByBoard(board);
    if (!sw) return;
    TodoBoard_SetVisible(board, FALSE);
    if (sw->hwnd) DestroyWindow(sw->hwnd); /* WM_DESTROY frees the slot */
}

void TodoStickies_HideAll(void) {
    for (int i = 0; i < STICKY_SLOT_MAX_WIN; i++) {
        StickyWin *sw = &s_wins[i];
        if (!sw->used) continue;
        TodoBoard_SetVisible(sw->board, FALSE);
        if (sw->hwnd) DestroyWindow(sw->hwnd);
        else sw->used = FALSE;
    }
}

void TodoStickies_ToggleBoard(const char *board) {
    if (!board || !board[0]) return;
    if (TodoStickies_IsVisible(board)) TodoStickies_HideBoard(board);
    else TodoStickies_ShowBoard(board);
}

/* Show a card for every board (tray "show all stickies"). */
void TodoStickies_ShowAll(void) {
    int n = TodoBoard_Count();
    for (int i = 0; i < n; i++) {
        const char *name = TodoBoard_NameAt(i);
        if (name[0]) TodoStickies_ShowBoard(name);
    }
}

void TodoStickies_RestoreAll(void) {
    int n = TodoBoard_Count();
    for (int i = 0; i < n; i++) {
        const char *name = TodoBoard_NameAt(i);
        if (!name[0]) continue;
        if (TodoBoard_Visible(name)) TodoStickies_ShowBoard(name);
    }
}

void TodoStickies_RefreshAll(void) {
    for (int i = 0; i < STICKY_SLOT_MAX_WIN; i++) {
        if (s_wins[i].used && s_wins[i].hwnd)
            InvalidateRect(s_wins[i].hwnd, NULL, FALSE);
    }
}

BOOL TodoSticky_TopmostGlobal(void) { return TodoBoard_TopmostGlobal(); }

void TodoSticky_SetTopmostGlobal(BOOL topmost) {
    TodoBoard_SetTopmostGlobal(topmost);
}

int TodoSticky_Opacity(void) { return TodoBoard_Opacity(); }

void TodoSticky_SetOpacity(int pct) { TodoBoard_SetOpacity(pct); }

void TodoSticky_ApplyOpacity(HWND hwnd) {
    if (!hwnd) return;
    int pct = TodoSticky_Opacity();
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE,
                      GetWindowLongPtrW(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hwnd, 0, (BYTE)(255 * pct / 100), LWA_ALPHA);
}

/* Re-applies z-order + opacity after a settings change. Cards with an
 * explicit per-board override keep their own choice. */
void TodoSticky_RetopAll(void) {
    for (int i = 0; i < STICKY_SLOT_MAX_WIN; i++) {
        if (!s_wins[i].used || !s_wins[i].hwnd) continue;
        if (TodoBoard_TopmostOverride(s_wins[i].board) != -1) continue;
        BOOL top = TodoBoard_TopmostFor(s_wins[i].board);
        SetWindowPos(s_wins[i].hwnd, top ? HWND_TOPMOST : HWND_NOTOPMOST,
                     0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        TodoSticky_ApplyOpacity(s_wins[i].hwnd);
    }
}
