/**
 * @file timeout_fullscreen.c
 * @brief Fullscreen dim + centered big-text timeout alert.
 *
 * Layout: fullscreen dim window (all monitors: union of monitor rects) at 70%
 * black opacity, layered; center card ~min(720px, 80%w) x auto, white rounded
 * look via plain rect + big fonts:
 *   title   44px bold Microsoft YaHei (wraps, centered)
 *   message 24px regular (wraps, centered)
 *   hint    15px gray "任意键 / 点击关闭" (centered)
 * Dismiss: WM_KEYDOWN anywhere, WM_LBUTTONDOWN anywhere, WM_TIMER timeout,
 * or TimeoutFullscreen_Dismiss(). ESC also works.
 */
#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include "todo/timeout_fullscreen.h"
#include "config.h"

#define TFS_CLASS_DIM L"CatimeTimeoutDimClass"
#define TFS_TIMER_ID 2101
/* Fallbacks when config not loaded (defaults mirror config_defaults.h) */
#define TFS_DIM_ALPHA_FB 178 /* ~70% */
#define TFS_TITLE_PX_FB 44
#define TFS_MSG_PX_FB 24
#define TFS_HINT_PX 15

static HWND g_dim = NULL;
static HWND g_card = NULL;
static HFONT g_fTitle = NULL;
static HFONT g_fMsg = NULL;
static HFONT g_fHint = NULL;
static wchar_t *g_title = NULL;
static wchar_t *g_msg = NULL;
static volatile LONG g_showing = 0;
static BOOL g_classReg = FALSE;
/* per-show style snapshot */
static wchar_t g_fsFont[64] = L"Microsoft YaHei";
static int g_fsTitlePx = TFS_TITLE_PX_FB;
static int g_fsMsgPx = TFS_MSG_PX_FB;
static COLORREF g_fsBg = RGB(0, 0, 0);
static BYTE g_fsAlpha = TFS_DIM_ALPHA_FB;

static wchar_t *DupStr(const wchar_t *s) {
    if (!s) s = L"";
    size_t n = wcslen(s) + 1;
    wchar_t *d = (wchar_t *)malloc(n * sizeof(wchar_t));
    if (d) wcscpy_s(d, n, s);
    return d;
}

static HFONT MakeFontEx(const wchar_t *name, int px, BOOL bold) {
    return CreateFontW(-px, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL,
                       FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                       (name && name[0]) ? name : L"Microsoft YaHei");
}

static void FreeFonts(void) {
    if (g_fTitle) { DeleteObject(g_fTitle); g_fTitle = NULL; }
    if (g_fMsg) { DeleteObject(g_fMsg); g_fMsg = NULL; }
    if (g_fHint) { DeleteObject(g_fHint); g_fHint = NULL; }
}

typedef struct { RECT rc; } MonitorUnion;
static BOOL CALLBACK UnionMonitors(HMONITOR h, HDC dc, LPRECT r, LPARAM p) {
    (void)h; (void)dc;
    MonitorUnion *u = (MonitorUnion *)p;
    if (u->rc.left == 0 && u->rc.top == 0 && u->rc.right == 0 && u->rc.bottom == 0) {
        u->rc = *r;
    } else {
        if (r->left < u->rc.left) u->rc.left = r->left;
        if (r->top < u->rc.top) u->rc.top = r->top;
        if (r->right > u->rc.right) u->rc.right = r->right;
        if (r->bottom > u->rc.bottom) u->rc.bottom = r->bottom;
    }
    return TRUE;
}

static LRESULT CALLBACK DimProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wp;
        RECT rc;
        if (!hdc || !GetClientRect(hwnd, &rc)) break;
        HBRUSH b = CreateSolidBrush(g_fsBg);
        if (b) {
            FillRect(hdc, &rc, b);
            DeleteObject(b);
        }
        return 1;
    }
    case WM_KEYDOWN:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        TimeoutFullscreen_Dismiss();
        return 0;
    case WM_TIMER:
        if (wp == TFS_TIMER_ID) TimeoutFullscreen_Dismiss();
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, TFS_TIMER_ID);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK CardProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_KEYDOWN:
    case WM_LBUTTONDOWN:
        TimeoutFullscreen_Dismiss();
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        if (!GetClientRect(hwnd, &rc)) { EndPaint(hwnd, &ps); return 0; }
        /* card bg */
        HBRUSH bg = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);
        SetBkMode(hdc, TRANSPARENT);
        int y = 36;
        /* title */
        if (g_fTitle) SelectObject(hdc, g_fTitle);
        SetTextColor(hdc, RGB(20, 20, 20));
        RECT tr = {40, y, rc.right - 40, rc.bottom};
        int th = DrawTextW(hdc, g_title ? g_title : L"", -1, &tr,
                           DT_CENTER | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
        tr.bottom = tr.top + th;
        DrawTextW(hdc, g_title ? g_title : L"", -1, &tr,
                  DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
        y = tr.bottom + 20;
        /* message */
        if (g_fMsg) SelectObject(hdc, g_fMsg);
        SetTextColor(hdc, RGB(60, 60, 60));
        RECT mr = {40, y, rc.right - 40, rc.bottom - 60};
        int mh = DrawTextW(hdc, g_msg ? g_msg : L"", -1, &mr,
                           DT_CENTER | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
        mr.bottom = mr.top + mh;
        DrawTextW(hdc, g_msg ? g_msg : L"", -1, &mr,
                  DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
        /* hint */
        if (g_fHint) SelectObject(hdc, g_fHint);
        SetTextColor(hdc, RGB(150, 150, 150));
        RECT hr = {40, rc.bottom - 44, rc.right - 40, rc.bottom - 12};
        const wchar_t *hint = L"任意键 / 点击关闭";
        DrawTextW(hdc, hint, -1, &hr, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_TIMER:
        if (wp == TFS_TIMER_ID) TimeoutFullscreen_Dismiss();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* measure wrapped height for width w with font f */
static int MeasureH(HWND ref, HFONT f, const wchar_t *s, int w) {
    HDC hdc = GetDC(ref);
    if (!hdc) return 60;
    HGDIOBJ old = NULL;
    if (f) {
        old = SelectObject(hdc, f);
        if (old == HGDI_ERROR) old = NULL;
    }
    RECT r = {0, 0, w, 0};
    DrawTextW(hdc, s ? s : L"", -1, &r, DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
    if (old) SelectObject(hdc, old);
    ReleaseDC(ref, hdc);
    return r.bottom > 0 ? r.bottom : 20;
}

void TimeoutFullscreen_Show(HWND hwndOwner, const wchar_t *title, const wchar_t *message, int timeoutMs) {
    if (InterlockedCompareExchange(&g_showing, 1, 0) != 0) return; /* already showing */
    HINSTANCE hInst = GetModuleHandleW(NULL);
    if (!g_classReg) {
        WNDCLASSEXW wc = {0};
        wc.cbSize = sizeof(wc);
        wc.hInstance = hInst;
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.lpszClassName = TFS_CLASS_DIM;
        wc.lpfnWndProc = DimProc;
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        if (RegisterClassExW(&wc) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            InterlockedExchange(&g_showing, 0);
            return;
        }
        WNDCLASSEXW wc2 = {0};
        wc2.cbSize = sizeof(wc2);
        wc2.hInstance = hInst;
        wc2.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc2.lpszClassName = L"CatimeTimeoutCardClass";
        wc2.lpfnWndProc = CardProc;
        wc2.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
        if (RegisterClassExW(&wc2) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            InterlockedExchange(&g_showing, 0);
            return;
        }
        g_classReg = TRUE;
    }
    free(g_title); free(g_msg);
    g_title = DupStr(title);
    g_msg = DupStr(message);
    if (!g_title || !g_msg) {
        free(g_title); g_title = NULL;
        free(g_msg); g_msg = NULL;
        InterlockedExchange(&g_showing, 0);
        return;
    }
    FreeFonts();
    /* styled from config (clamped at apply time) */
    g_fsTitlePx = g_AppConfig.notification.display.fullscreen_title_px;
    if (g_fsTitlePx <= 0) g_fsTitlePx = TFS_TITLE_PX_FB;
    g_fsMsgPx = g_AppConfig.notification.display.fullscreen_msg_px;
    if (g_fsMsgPx <= 0) g_fsMsgPx = TFS_MSG_PX_FB;
    if (g_AppConfig.notification.display.fullscreen_fontname[0])
        wcsncpy_s(g_fsFont, _countof(g_fsFont),
                  g_AppConfig.notification.display.fullscreen_fontname, _TRUNCATE);
    else
        wcsncpy_s(g_fsFont, _countof(g_fsFont), L"Microsoft YaHei", _TRUNCATE);
    g_fsBg = g_AppConfig.notification.display.fullscreen_bgcolor;
    g_fsAlpha = (BYTE)((g_AppConfig.notification.display.fullscreen_opacity > 0
                        ? g_AppConfig.notification.display.fullscreen_opacity : 70) * 255 / 100);
    g_fTitle = MakeFontEx(g_fsFont, g_fsTitlePx, TRUE);
    g_fMsg = MakeFontEx(g_fsFont, g_fsMsgPx, FALSE);
    g_fHint = MakeFontEx(g_fsFont, TFS_HINT_PX, FALSE);
    if (!g_fTitle || !g_fMsg || !g_fHint) {
        FreeFonts();
        InterlockedExchange(&g_showing, 0);
        return;
    }

    MonitorUnion u;
    memset(&u, 0, sizeof(u));
    EnumDisplayMonitors(NULL, NULL, UnionMonitors, (LPARAM)&u);
    int fw = u.rc.right - u.rc.left;
    int fh = u.rc.bottom - u.rc.top;
    if (fw <= 0 || fh <= 0) { fw = GetSystemMetrics(SM_CXSCREEN); fh = GetSystemMetrics(SM_CYSCREEN); u.rc.left = 0; u.rc.top = 0; }

    g_dim = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
                            TFS_CLASS_DIM, L"Catime Timeout",
                            WS_POPUP | WS_VISIBLE,
                            u.rc.left, u.rc.top, fw, fh,
                            hwndOwner, NULL, hInst, NULL);
    if (!g_dim) { InterlockedExchange(&g_showing, 0); return; }
    SetLayeredWindowAttributes(g_dim, 0, g_fsAlpha, LWA_ALPHA);

    /* card size: width min(720, 80% screen), height from wrapped text */
    int cw = fw * 8 / 10;
    if (cw > 720) cw = 720;
    if (cw < 320) cw = 320;
    int inner = cw - 80;
    int th = MeasureH(g_dim, g_fTitle, g_title, inner);
    int mh = MeasureH(g_dim, g_fMsg, g_msg, inner);
    int chh = 36 + th + 20 + mh + 60;
    if (chh < 220) chh = 220;
    if (chh > fh - 80) chh = fh - 80;
    int cx = u.rc.left + (fw - cw) / 2;
    int cy = u.rc.top + (fh - chh) / 2;
    g_card = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                             L"CatimeTimeoutCardClass", L"",
                             WS_POPUP | WS_VISIBLE,
                             cx, cy, cw, chh,
                             g_dim, NULL, hInst, NULL);
    if (!g_card) { DestroyWindow(g_dim); g_dim = NULL; InterlockedExchange(&g_showing, 0); return; }
    SetForegroundWindow(g_card);
    SetFocus(g_card);
    if (timeoutMs <= 0) timeoutMs = 8000;
    SetTimer(g_card, TFS_TIMER_ID, (UINT)timeoutMs, NULL);
    SetTimer(g_dim, TFS_TIMER_ID, (UINT)timeoutMs, NULL);
}

void TimeoutFullscreen_Dismiss(void) {
    if (InterlockedCompareExchange(&g_showing, 0, 1) != 1) return;
    HWND card = g_card, dim = g_dim;
    g_card = NULL; g_dim = NULL;
    if (card) DestroyWindow(card);
    if (dim) DestroyWindow(dim);
    free(g_title); g_title = NULL;
    free(g_msg); g_msg = NULL;
    FreeFonts();
}

BOOL TimeoutFullscreen_IsShowing(void) {
    return InterlockedCompareExchange(&g_showing, 1, 1) == 1;
}
