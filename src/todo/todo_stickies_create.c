/**
 * @file todo_stickies_create.c
 * @brief Board card creation: window class, child edit, show/raise.
 *
 * Split from todo_stickies_window.c to respect the 300-line gate.
 * The WndProc itself lives in todo_stickies_window.c.
 */
#include <string.h>

#include "todo_board.h"
#include "todo_board_layout.h"
#include "todo_stickies.h"
#include "todo_sticky_pomo.h"
#include "todo_store.h"

#include "todo_stickies_slot.h"

/* RD5: the card has no search box; keyword filtering lives in the
 * task manager dialog only. These stay as no-ops for the layout call
 * sites in the WndProc. */
void TodoSticky_SyncEditFromBoard(StickyWin *sw) {
    (void)sw;
}

void TodoSticky_LayoutChildren(StickyWin *sw) {
    (void)sw;
}

static BOOL EnsureClass(void) {
    if (g_stickyClassReg) return TRUE;
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = STICKY_CLASS;
    wc.lpfnWndProc = StickyProc;
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.style = CS_DBLCLKS;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return FALSE;
    g_stickyClassReg = TRUE;
    return TRUE;
}

void TodoStickies_ShowBoard(const char *board) {
    if (!board || !board[0] || !EnsureClass()) return;
    if (TodoBoard_IndexByName(board) < 0) return;
    StickyWin *exist = TodoSticky_SlotByBoard(board);
    if (exist && exist->hwnd) {
        ShowWindow(exist->hwnd, SW_SHOW);
        SetWindowPos(exist->hwnd, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        TodoSticky_Repaint(exist);
        return;
    }
    StickyWin *sw = exist ? exist : TodoSticky_SlotAlloc();
    if (!sw) return;
    int x = 140, y = 140, w = 416, h = 338; /* RD2: +30% */
    TodoBoard_LoadGeom(board, &x, &y, &w, &h);
    memset(sw, 0, sizeof(*sw));
    sw->used = TRUE;
    strcpy_s(sw->board, sizeof(sw->board), board);
    sw->fTitle = CreateFontW(-21, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, /* RD2 +30% */
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
    sw->fBody = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, /* RD2 +30% */
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
    DWORD ex = WS_EX_TOOLWINDOW | WS_EX_LAYERED;
    if (TodoBoard_TopmostFor(board)) ex |= WS_EX_TOPMOST;
    sw->collapsed = TodoBoard_IsCollapsed(board);
    sw->expandW = w;
    sw->expandH = h;
    if (sw->collapsed) {
        w = BOARD_BAR_COLLAPSED_W;
        h = BOARD_BAR_COLLAPSED_H;
    }
    sw->hwnd = CreateWindowExW(ex, STICKY_CLASS, L"Catime TODO", /* RD1 */
                               WS_POPUP | WS_VISIBLE | WS_THICKFRAME |
                                   WS_CLIPCHILDREN,
                               x, y, w, h, NULL, NULL,
                               GetModuleHandleW(NULL), NULL);
    if (!sw->hwnd) {
        if (sw->fTitle) DeleteObject(sw->fTitle);
        if (sw->fBody) DeleteObject(sw->fBody);
        sw->used = FALSE;
        return;
    }
    sw->edit = NULL; /* RD5: no search box on the card */
    TodoSticky_LayoutChildren(sw);
    TodoBoard_SetVisible(board, TRUE);
    if (TodoStickyPomo_Active()) SetTimer(sw->hwnd, STICKY_TIMER_POMO, 1000, NULL);
    ShowWindow(sw->hwnd, SW_SHOW);
    TodoSticky_ApplyOpacity(sw->hwnd);
    TodoSticky_UpdateTips(sw->hwnd, sw->collapsed);
    TodoSticky_Repaint(sw);
}
