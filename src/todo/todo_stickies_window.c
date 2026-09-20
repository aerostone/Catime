/**
 * @file todo_stickies_window.c
 * @brief Sticky board card window: creation, painting, drag, hit-test.
 *
 * Layout: title bar (drag / fold / open list / hide), filter row
 * (week-all toggle + keyword edit + add button), task rows
 * (click = toggle done, right-click = row menu).
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

#define STICKY_TIMER_POMO 9201
#define STICKY_TIMER_KW 9202 /* keyword persistence debounce */

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
    if (sw->edit) ShowWindow(sw->edit, collapsed ? SW_HIDE : SW_SHOW);
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
    if (fromRight <= BOARD_HIT_CLOSE) {
        TodoStickies_HideBoard(sw->board);
        return;
    }
    if (fromRight <= BOARD_HIT_CLOSE + BOARD_HIT_LIST) {
        OpenList(sw, FALSE);
        return;
    }
    if (fromRight <= BOARD_HIT_CLOSE + BOARD_HIT_LIST + BOARD_HIT_FOLD) {
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
            TodoBoard_PaintRows(hdc, &rc, tasks, n, sw->fBody);
        } else if (sw) {
            TodoBoard_PaintTitle(hdc, &rc, sw->board,
                                 TodoBoard_OpenCount(sw->board), TRUE, 0,
                                 sw->fTitle);
        }
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
            if (x < 4 + BOARD_HIT_SCOPE_W) ToggleScope(sw);
            else if (x >= 4 + BOARD_HIT_SCOPE_W + 4) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                if (x >= rc.right - 4 - BOARD_HIT_ADD_W) OpenList(sw, TRUE);
            }
            return 0;
        }
        int row = BoardLayout_RowAt(y);
        if (row >= 0) {
            char id[TODO_STORE_ID_LEN] = "";
            if (TodoSticky_RowIdAt(sw->board, row, id, sizeof(id))) {
                TodoTask t;
                memset(&t, 0, sizeof(t));
                if (TodoStore_FindById(id, &t)) {
                    TodoStore_SetDone(t.id, !t.done);
                    TodoSticky_Repaint(sw);
                }
            }
        }
        return 0;
    }
    case WM_MOUSEMOVE:
        if (sw && sw->dragging) {
            POINT pt;
            GetCursorPos(&pt);
            SetWindowPos(hwnd, NULL, pt.x - sw->dragOff.x,
                         pt.y - sw->dragOff.y, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER);
        }
        return 0;
    case WM_LBUTTONUP:
        if (sw && sw->dragging) {
            sw->dragging = FALSE;
            ReleaseCapture();
        }
        return 0;
    case WM_LBUTTONDBLCLK:
        if (sw) TodoSticky_SetCollapsedUI(hwnd, sw, !sw->collapsed);
        return 0;
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
    case WM_COMMAND: {
        WORD id = LOWORD(wp), code = HIWORD(wp);
        if (sw && id == STICKY_EDIT_ID && code == EN_CHANGE) {
            /* debounce: persist the keyword once typing pauses */
            KillTimer(hwnd, STICKY_TIMER_KW);
            SetTimer(hwnd, STICKY_TIMER_KW, 300, NULL);
        }
        return 0;
    }
    case WM_TIMER:
        if (wp == STICKY_TIMER_POMO) {
            if (!TodoStickyPomo_Validate() || TodoStickyPomo_Remaining() <= 0)
                KillTimer(hwnd, STICKY_TIMER_POMO);
            TodoSticky_Repaint(sw);
        } else if (wp == STICKY_TIMER_KW && sw && sw->edit) {
            KillTimer(hwnd, STICKY_TIMER_KW);
            wchar_t wkw[TODO_STORE_TITLE_LEN];
            GetWindowTextW(sw->edit, wkw, TODO_STORE_TITLE_LEN);
            char kw[TODO_STORE_TITLE_LEN * 2];
            WideCharToMultiByte(CP_UTF8, 0, wkw, -1, kw, sizeof(kw), NULL,
                                NULL);
            TodoBoard_SetKeyword(sw->board, kw);
            TodoSticky_Repaint(sw);
        }
        return 0;
    case WM_DESTROY:
        if (sw) {
            KillTimer(hwnd, STICKY_TIMER_POMO);
            KillTimer(hwnd, STICKY_TIMER_KW);
            if (sw->edit) DestroyWindow(sw->edit);
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

