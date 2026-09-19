/**
 * @file todo_stickies_window.c
 * @brief Sticky topmost tool windows: create/drag/edit/close.
 */
#include <string.h>
#include <windowsx.h>

#include "todo_stickies.h"
#include "todo_store.h"
#include "todo_sticky_pomo.h"
#include "todo_sticky_rows.h"

BOOL TodoStickyEdit_Commit(const char *oldId, const wchar_t *newTitle,
                           char *newIdOut, size_t newIdCap);

#define STICKY_CLASS L"CatimeStickyClass"
#define STICKY_BAR_H 26
#define STICKY_SLOT_MAX_WIN 16
#define STICKY_EDIT_ID 9001
#define STICKY_MENU_DONE 9101
#define STICKY_MENU_UNPIN 9102
#define STICKY_MENU_DELETE 9103
#define STICKY_MENU_EXPAND 9104
#define STICKY_MENU_TOPMOST 9105
#define STICKY_MENU_POMO 9106
#define STICKY_MENU_RENAME 9107
#define STICKY_TIMER_POMO 9201

void TodoSticky_LoadGeom(const char *taskId, int *x, int *y, int *w, int *h);
void TodoSticky_SaveGeom(const char *taskId, int x, int y, int w, int h);
COLORREF TodoSticky_BarColor(TodoImportance imp, BOOL done);

#include "todo_stickies_slot.h"

static StickyWin s_wins[STICKY_SLOT_MAX_WIN];
static BOOL s_classReg = FALSE;

int TodoSticky_SlotCount(void) {
    return STICKY_SLOT_MAX_WIN;
}

StickyWin *TodoSticky_SlotAt(int index) {
    if (index < 0 || index >= STICKY_SLOT_MAX_WIN) return NULL;
    return &s_wins[index];
}

StickyWin *TodoSticky_SlotById(const char *id) {
    if (!id) return NULL;
    for (int i = 0; i < STICKY_SLOT_MAX_WIN; i++) {
        if (s_wins[i].used && strcmp(s_wins[i].taskId, id) == 0)
            return &s_wins[i];
    }
    return NULL;
}

StickyWin *TodoSticky_SlotByHwnd(HWND h) {
    for (int i = 0; i < STICKY_SLOT_MAX_WIN; i++) {
        if (s_wins[i].used && s_wins[i].hwnd == h)
            return &s_wins[i];
    }
    return NULL;
}

static BOOL LoadTask(const char *id, TodoTask *out) {
    TodoFilter f;
    TodoFilter_InitDefault(&f);
    f.showDone = TRUE;
    f.showSync = FALSE;
    TodoTask buf[TODO_STORE_MAX_TASKS];
    int n = TodoStore_Query(&f, buf, TODO_STORE_MAX_TASKS);
    for (int i = 0; i < n; i++) {
        if (strcmp(buf[i].id, id) == 0) {
            *out = buf[i];
            return TRUE;
        }
    }
    return FALSE;
}

static void CloseEdit(StickyWin *sw, BOOL commit) {
    if (!sw->edit) return;
    if (commit) {
        wchar_t wt[TODO_STORE_TITLE_LEN];
        GetWindowTextW(sw->edit, wt, _countof(wt));
        char newId[TODO_STORE_ID_LEN] = "";
        if (TodoStickyEdit_Commit(sw->taskId, wt, newId, sizeof(newId))) {
            if (newId[0]) strcpy_s(sw->taskId, sizeof(sw->taskId), newId);
        }
    }
    DestroyWindow(sw->edit);
    sw->edit = NULL;
    InvalidateRect(sw->hwnd, NULL, TRUE);
}

void TodoStickyEdit_Open(HWND parent, const char *taskId, HWND *editOut);

static void OpenEdit(StickyWin *sw) {
    if (sw->edit) return;
    TodoStickyEdit_Open(sw->hwnd, sw->taskId, &sw->edit);
}

static LRESULT CALLBACK StickyProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    StickyWin *sw = TodoSticky_SlotByHwnd(hwnd);
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        if (sw) {
            TodoTask t;
            if (LoadTask(sw->taskId, &t)) {
                int rem = 0;
                const char *pid = TodoStickyPomo_TaskId();
                if (pid[0] && strcmp(pid, sw->taskId) == 0)
                    rem = TodoStickyPomo_Remaining();
                TodoSticky_PaintEx(hdc, &rc, &t, sw->fTitle, sw->fBody,
                                   sw->collapsed, rem);
            }
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        if (!sw) break;
        int y = GET_Y_LPARAM(lp);
        int x = GET_X_LPARAM(lp);
        RECT rc;
        GetClientRect(hwnd, &rc);
        /* D2: rightmost 18px hides (= unpin); pin glyph 40..18 toggles.
         * Neither claims to be "close": symbols are ◉/○/–. */
        if (x >= rc.right - 18 && y <= STICKY_BAR_H) {
            TodoSticky_SetPinned(sw->taskId, FALSE);
            return 0;
        }
        if (y <= STICKY_BAR_H) {
            sw->dragging = TRUE;
            sw->dragOff.x = x;
            sw->dragOff.y = y;
            SetCapture(hwnd);
        }
        return 0;
    }
    case WM_MOUSEMOVE:
        if (sw && sw->dragging) {
            POINT pt;
            GetCursorPos(&pt);
            RECT wr;
            GetWindowRect(hwnd, &wr);
            int w = wr.right - wr.left, h = wr.bottom - wr.top;
            int nx = pt.x - sw->dragOff.x, ny = pt.y - sw->dragOff.y;
            SetWindowPos(hwnd, NULL, nx, ny, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            TodoSticky_SaveGeom(sw->taskId, nx, ny, w, h);
        }
        return 0;
    case WM_LBUTTONUP:
        if (sw && sw->dragging) {
            sw->dragging = FALSE;
            ReleaseCapture();
        }
        return 0;
    case WM_LBUTTONDBLCLK: {
        if (!sw) break;
        int y = GET_Y_LPARAM(lp);
        if (y <= STICKY_BAR_H)
            TodoSticky_SetCollapsedUI(hwnd, sw, !sw->collapsed);
        else if (!sw->collapsed)
            OpenEdit(sw);
        return 0;
    }
    case WM_RBUTTONUP: {
        if (!sw) break;
        TodoSticky_ShowRowMenu(hwnd, sw);
        return 0;
    }
    case WM_COMMAND:
        if (sw && LOWORD(wp) == STICKY_EDIT_ID && HIWORD(wp) == EN_KILLFOCUS)
            CloseEdit(sw, TRUE);
        return 0;
    case WM_KEYDOWN:
        if (sw && sw->edit && (wp == VK_RETURN || wp == VK_ESCAPE)) {
            CloseEdit(sw, wp == VK_RETURN);
            return 0;
        }
        return 0;
    case WM_SIZE:
        if (sw) {
            RECT wr;
            GetWindowRect(hwnd, &wr);
            TodoSticky_SaveGeom(sw->taskId, wr.left, wr.top,
                                wr.right - wr.left, wr.bottom - wr.top);
        }
        return 0;
    case WM_TIMER:
        if (sw && wp == STICKY_TIMER_POMO) {
            if (!TodoStickyPomo_Validate() ||
                TodoStickyPomo_Remaining() <= 0) {
                KillTimer(hwnd, STICKY_TIMER_POMO);
            }
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_DESTROY:
        if (sw) {
            CloseEdit(sw, FALSE);
            if (sw->fTitle) DeleteObject(sw->fTitle);
            if (sw->fBody) DeleteObject(sw->fBody);
            sw->used = FALSE;
            sw->hwnd = NULL;
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static BOOL EnsureClass(void) {
    if (s_classReg) return TRUE;
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = STICKY_CLASS;
    wc.lpfnWndProc = StickyProc;
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.style = CS_DBLCLKS;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return FALSE;
    s_classReg = TRUE;
    return TRUE;
}

void TodoStickies_Show(const char *taskId) {
    if (!taskId || !taskId[0] || !EnsureClass()) return;
    if (TodoSticky_SlotById(taskId)) {
        StickyWin *e = TodoSticky_SlotById(taskId);
        ShowWindow(e->hwnd, SW_SHOW);
        return;
    }
    StickyWin *sw = NULL;
    for (int i = 0; i < STICKY_SLOT_MAX_WIN; i++) {
        if (!s_wins[i].used) {
            sw = &s_wins[i];
            break;
        }
    }
    if (!sw) return;
    TodoTask t;
    if (!LoadTask(taskId, &t)) return;
    int x = 120, y = 120, w = 240, h = 180;
    TodoSticky_LoadGeom(taskId, &x, &y, &w, &h);
    memset(sw, 0, sizeof(*sw));
    sw->used = TRUE;
    strcpy_s(sw->taskId, sizeof(sw->taskId), taskId);
    sw->fTitle = CreateFontW(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
    sw->fBody = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
    DWORD ex = WS_EX_TOOLWINDOW | (TodoSticky_TopmostFor(taskId) ? WS_EX_TOPMOST : 0);
    sw->collapsed = TodoSticky_IsCollapsed(taskId);
    sw->topmostOverride = TodoSticky_TopmostOverride(taskId);
    int fullH = h;
    sw->expandH = h;
    if (sw->collapsed) h = STICKY_BAR_H + 2;
    sw->hwnd = CreateWindowExW(ex, STICKY_CLASS,
                               L"Catime 便签", WS_POPUP | WS_VISIBLE | WS_THICKFRAME,
                               x, y, w, h, NULL, NULL,
                               GetModuleHandleW(NULL), NULL);
    (void)fullH;
    if (!sw->hwnd) {
        if (sw->fTitle) DeleteObject(sw->fTitle);
        if (sw->fBody) DeleteObject(sw->fBody);
        sw->used = FALSE;
        return;
    }
    ShowWindow(sw->hwnd, SW_SHOW);
    TodoSticky_UpdateTips(sw->hwnd, sw->collapsed);
}
