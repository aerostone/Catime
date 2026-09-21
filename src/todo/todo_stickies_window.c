/**
 * @file todo_stickies_window.c
 * @brief Sticky card WndProc: paint, drag, select/hover/keys, menus.
 *
 * N1: click selects (+hover); dblclick/menu toggles done. N4: no
 * title-bar hide. N5: arrows/Space/Esc keyboard card.
 */
#include <string.h>
#include <windowsx.h>

#include "dialog/dialog_todo.h"
#include "todo_board.h"
#include "todo_board_layout.h"
#include "todo_board_render.h"
#include "todo_stickies.h"
#include "todo_store.h"
#include "todo_sticky_pomo.h"
#include "todo_sync.h"

#include "todo_stickies_slot.h"

BOOL g_stickyClassReg = FALSE;

void TodoSticky_Repaint(StickyWin *sw) {
    if (sw && sw->hwnd) InvalidateRect(sw->hwnd, NULL, FALSE);
}

void TodoSticky_SetCollapsedUI(HWND hwnd, StickyWin *sw, BOOL collapsed) {
    if (!sw || !hwnd) return;
    RECT wr;
    GetWindowRect(hwnd, &wr);
    int w = wr.right - wr.left, h = wr.bottom - wr.top;
    if (collapsed) {
        if (w > BOARD_BAR_COLLAPSED_W) sw->expandW = w;
        if (h > BOARD_BAR_COLLAPSED_H) sw->expandH = h;
        w = BOARD_BAR_COLLAPSED_W;
        h = BOARD_BAR_COLLAPSED_H;
    } else {
        w = sw->expandW > 200 ? sw->expandW : 320;
        h = sw->expandH > 120 ? sw->expandH : 260;
    }
    TodoBoard_SetCollapsed(sw->board, collapsed);
    sw->collapsed = collapsed;
    TodoSticky_ApplyOpacity(hwnd); /* RD6: dot vs card alpha */
    SetWindowPos(hwnd, NULL, wr.left, wr.top, w, h,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    TodoSticky_UpdateTips(hwnd, collapsed);
    TodoSticky_Repaint(sw);
}

static void ToggleScope(StickyWin *sw) {
    TodoDueScope sc = TodoBoard_Scope(sw->board);
    TodoBoard_SetScope(sw->board,
                       sc == TODO_DUE_SCOPE_ALL ? TODO_DUE_SCOPE_WEEK
                                                : TODO_DUE_SCOPE_ALL);
    TodoSticky_Repaint(sw);
}

static void OpenList(StickyWin *sw, BOOL addNew) {
    ShowTodoListDialogForBoard(TodoSync_MainHwnd(), sw->board, addNew);
}

static void HitTitleBar(StickyWin *sw, int x, int y) {
    RECT rc;
    GetClientRect(sw->hwnd, &rc);
    int fromRight = rc.right - x;
    /* N4: no one-click hide; the rest is drag. */
    if (fromRight <= BOARD_HIT_LIST) {
        OpenList(sw, FALSE);
        return;
    }
    if (fromRight <= BOARD_HIT_LIST + BOARD_HIT_FOLD) {
        TodoSticky_SetCollapsedUI(sw->hwnd, sw, !sw->collapsed);
        return;
    }
    sw->dragging = TRUE;
    sw->dragOff.x = x;
    sw->dragOff.y = y;
    SetCapture(sw->hwnd);
}

LRESULT CALLBACK StickyProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    StickyWin *sw = TodoSticky_SlotByHwnd(hwnd);
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        if (sw && !sw->collapsed) {
            TodoTask tasks[TODO_STORE_MAX_TASKS];
            int n = TodoSticky_LoadBoardTasks(sw->board, tasks, TODO_STORE_MAX_TASKS);
            TodoBoard_PaintTitle(hdc, &rc, sw->board, n, FALSE,
                                 TodoSticky_PomoRemainingFor(sw->board), sw->fTitle);
            TodoBoard_PaintFilter(hdc, &rc, TodoBoard_Scope(sw->board), "",
                                  sw->fBody);
            TodoBoard_PaintRowsEx(hdc, &rc, tasks, n, sw->fBody,
                                      sw->selId,
                                      sw->hoverRow < n ? sw->hoverRow : -1);
        } else if (sw) {
            /* RD6: minimized = translucent dot, no title bar. */
            TodoBoard_PaintDot(hdc, &rc,
                               TodoSticky_PeakImportance(sw->board),
                               TodoBoard_OpenCount(sw->board));
        }
        TodoSticky_PaintFocus(hdc, &rc, sw, hwnd);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_SIZE:
        if (sw) {
            if (!sw->collapsed) {
                TodoSticky_LayoutChildren(sw);
                RECT wr;
                GetWindowRect(hwnd, &wr);
                TodoBoard_SaveGeom(sw->board, wr.left, wr.top,
                                   wr.right - wr.left, wr.bottom - wr.top);
            }
        }
        return 0;
    case WM_MOVE:
        if (sw && !sw->collapsed) {
            RECT wr;
            GetWindowRect(hwnd, &wr);
            TodoBoard_SaveGeom(sw->board, wr.left, wr.top,
                               wr.right - wr.left, wr.bottom - wr.top);
        }
        return 0;
    case WM_LBUTTONDOWN: {
        if (!sw) break;
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        if (sw->collapsed) {
            TodoSticky_SetCollapsedUI(hwnd, sw, FALSE);
            return 0;
        }
        if (y <= BOARD_BAR_H) {
            HitTitleBar(sw, x, y);
            return 0;
        }
        if (y <= BOARD_BAR_H + BOARD_FILTER_H) {
            /* RD2/RD5: filter row is scope-toggle only; new tasks are
             * added in the task manager dialog. */
            if (x < 4 + BOARD_HIT_SCOPE_W) ToggleScope(sw);
            return 0;
        }
        /* N1: click selects; completion needs dblclick or the menu. */
        int row = BoardLayout_RowAt(y);
        if (row >= 0) {
            char id[TODO_STORE_ID_LEN] = "";
            if (TodoSticky_RowIdAt(sw->board, row, id, sizeof(id))) {
                if (strcmp(sw->selId, id) != 0) {
                    strcpy_s(sw->selId, sizeof(sw->selId), id);
                    TodoSticky_Repaint(sw);
                }
                SetFocus(hwnd);
            }
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (sw && sw->dragging) {
            POINT pt;
            GetCursorPos(&pt);
            SetWindowPos(hwnd, NULL, pt.x - sw->dragOff.x,
                         pt.y - sw->dragOff.y, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER);
            return 0;
        }
        /* N1 hover affordance: repaint only when the row changes. */
        if (sw && !sw->collapsed) {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            (void)x;
            int row = (y > BOARD_BAR_H + BOARD_FILTER_H)
                          ? BoardLayout_RowAt(y)
                          : -1;
            if (row != sw->hoverRow) {
                sw->hoverRow = row;
                TodoSticky_Repaint(sw);
            }
            if (row >= 0) {
                TRACKMOUSEEVENT tme;
                memset(&tme, 0, sizeof(tme));
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);
            }
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        if (sw && sw->hoverRow != -1) {
            sw->hoverRow = -1;
            TodoSticky_Repaint(sw);
        }
        return 0;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        TodoSticky_Repaint(TodoSticky_SlotByHwnd(hwnd));
        return 0;
    case WM_GETDLGCODE:
        return DLGC_WANTARROWS | DLGC_WANTCHARS;
    case WM_KEYDOWN: {
        /* N5: minimal keyboard card. */
        StickyWin *ksw = TodoSticky_SlotByHwnd(hwnd);
        if (ksw && !ksw->collapsed) {
            if (wp == VK_ESCAPE) {
                TodoStickies_HideBoard(ksw->board);
                return 0;
            }
            if (wp == VK_UP || wp == VK_DOWN) {
                TodoSticky_MoveSelection(ksw,
                                         wp == VK_DOWN ? 1 : -1);
                return 0;
            }
            if (wp == VK_SPACE || wp == VK_RETURN) {
                TodoSticky_ToggleSelected(ksw);
                return 0;
            }
        }
        break;
    }
    case WM_LBUTTONUP:
        if (sw && sw->dragging) {
            sw->dragging = FALSE;
            ReleaseCapture();
        }
        return 0;
    case WM_LBUTTONDBLCLK: {
        /* RD6: double-click no longer folds; rows still dblclick-toggle
         * done, everywhere else dblclick is a no-op (fold button only). */
        if (!sw) break;
        int y = GET_Y_LPARAM(lp);
        int row = (y > BOARD_BAR_H + BOARD_FILTER_H)
                      ? BoardLayout_RowAt(y)
                      : -1;
        if (row >= 0) TodoSticky_ToggleSelected(sw);
        return 0;
    }
    case WM_RBUTTONUP: {
        if (!sw) break;
        if (sw->collapsed) {
            TodoSticky_ShowCardMenu(hwnd, sw);
            return 0;
        }
        int y = GET_Y_LPARAM(lp);
        if (y <= BOARD_BAR_H + BOARD_FILTER_H)
            TodoSticky_ShowCardMenu(hwnd, sw);
        else
            TodoSticky_ShowRowMenu(hwnd, sw, BoardLayout_RowAt(y));
        return 0;
    }
    case WM_COMMAND:
        return 0;
    case WM_TIMER:
        if (wp == STICKY_TIMER_POMO) {
            if (!TodoStickyPomo_Validate() || TodoStickyPomo_Remaining() <= 0)
                KillTimer(hwnd, STICKY_TIMER_POMO);
            TodoSticky_Repaint(sw);
        }
        return 0;
    case WM_DESTROY:
        if (sw) {
            KillTimer(hwnd, STICKY_TIMER_POMO);
            if (sw->fTitle) DeleteObject(sw->fTitle);
            if (sw->fBody) DeleteObject(sw->fBody);
            sw->used = FALSE;
            sw->hwnd = NULL;
            sw->edit = NULL;
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

