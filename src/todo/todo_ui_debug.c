/**
 * @file todo_ui_debug.c
 * @brief Three-layer UI debug switch + dialog metrics dump + overlay.
 *
 * Layers: CATIME_UI_DEBUG compile flag, CATIME_UI_DEBUG env var,
 * [Debug] UiDebug=1 ini. Dump path logs id/x/y/w/h (client coords)
 * plus class/font info per child. Overlay draws dashed red rects and
 * numeric id labels in WM_PAINT so overlaps show on screenshots.
 */
#include "todo_ui_debug.h"

#ifdef CATIME_UI_DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

static BOOL s_inited = FALSE;
static BOOL s_enabled = FALSE;
static char s_iniPath[MAX_PATH] = "";

#ifdef CATIME_UI_DEBUG
static const BOOL kCompileOn = TRUE;
#else
static const BOOL kCompileOn = FALSE;
#endif

static void ReadIniFlag(void) {
    if (!s_iniPath[0]) return;
    wchar_t w[MAX_PATH];
    if (!MultiByteToWideChar(CP_UTF8, 0, s_iniPath, -1, w, _countof(w)))
        return;
    s_enabled = GetPrivateProfileIntW(L"Debug", L"UiDebug",
                                      s_enabled ? 1 : 0, w) != 0;
}

void TodoUiDebug_Init(const char *iniDirA) {
    s_enabled = kCompileOn ? TRUE : FALSE;
    const char *env = getenv("CATIME_UI_DEBUG");
    if (env && env[0] && strcmp(env, "0") != 0) s_enabled = TRUE;
    if (iniDirA && iniDirA[0]) {
#if defined(_MSC_VER)
        _snprintf_s(s_iniPath, sizeof(s_iniPath), _TRUNCATE,
                    "%s\\config.ini", iniDirA);
#else
        snprintf(s_iniPath, sizeof(s_iniPath), "%s\\config.ini", iniDirA);
#endif
    } else {
        s_iniPath[0] = '\0';
    }
    ReadIniFlag();
    s_inited = TRUE;
}

BOOL TodoUiDebug_Enabled(void) {
    if (!s_inited) {
        const char *env = getenv("CATIME_UI_DEBUG");
        if ((env && env[0] && strcmp(env, "0") != 0) || kCompileOn)
            return TRUE;
        return FALSE;
    }
    return s_enabled;
}

void TodoUiDebug_SetEnabled(BOOL on) {
    s_enabled = on ? TRUE : FALSE;
    if (!s_iniPath[0]) return;
    wchar_t w[MAX_PATH];
    if (!MultiByteToWideChar(CP_UTF8, 0, s_iniPath, -1, w, _countof(w)))
        return;
    WritePrivateProfileStringW(L"Debug", L"UiDebug", on ? L"1" : L"0", w);
}

static void DumpOne(HWND hdlg, HWND child, const char *tag, HDC ref) {
    int id = GetDlgCtrlID(child);
    RECT rc = {0};
    GetWindowRect(child, &rc);
    POINT o0 = {rc.left, rc.top}, o1 = {rc.right, rc.bottom};
    ScreenToClient(hdlg, &o0);
    ScreenToClient(hdlg, &o1);
    wchar_t cls[64] = L"?";
    GetClassNameW(child, cls, _countof(cls));
    wchar_t txt[64] = L"";
    GetWindowTextW(child, txt, _countof(txt));
    char cu[64] = "", cc[32] = "";
    WideCharToMultiByte(CP_UTF8, 0, txt, -1, cu, sizeof(cu), NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, cls, -1, cc, sizeof(cc), NULL, NULL);
    HFONT f = (HFONT)SendMessageW(child, WM_GETFONT, 0, 0);
    LOGFONTW lf;
    char font[64] = "system";
    if (f && GetObjectW(f, sizeof(lf), &lf)) {
        WideCharToMultiByte(CP_UTF8, 0, lf.lfFaceName, -1,
                            font, sizeof(font), NULL, NULL);
    }
    (void)ref;
    LOG_WARNING("[uidebug] %s id=%d cls=%s xywh=%d,%d,%d,%d font=%s txt=%s",
                tag ? tag : "dlg", id, cc,
                o0.x, o0.y, o1.x - o0.x, o1.y - o0.y, font, cu);
}

void TodoUiDebug_DumpDialog(HWND hdlg, const char *tag) {
    if (!TodoUiDebug_Enabled() || !hdlg) return;
    RECT c0 = {0};
    GetClientRect(hdlg, &c0);
    LOG_WARNING("[uidebug] %s dialog client=%dx%d",
                tag ? tag : "dlg", c0.right, c0.bottom);
    for (HWND c = GetWindow(hdlg, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT))
        DumpOne(hdlg, c, tag, NULL);
}

void TodoUiDebug_PaintOverlay(HWND hdlg) {
    if (!TodoUiDebug_Enabled() || !hdlg) return;
    HDC hdc = GetDC(hdlg);
    if (!hdc) return;
    HPEN pen = CreatePen(PS_DOT, 1, RGB(220, 38, 38));
    HGDIOBJ oldPen = pen ? SelectObject(hdc, pen) : NULL;
    HGDIOBJ oldBr = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    int oldMode = SetBkMode(hdc, TRANSPARENT);
    COLORREF oldFg = SetTextColor(hdc, RGB(220, 38, 38));
    for (HWND c = GetWindow(hdlg, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        RECT rc = {0};
        GetWindowRect(c, &rc);
        POINT o0 = {rc.left, rc.top}, o1 = {rc.right, rc.bottom};
        ScreenToClient(hdlg, &o0);
        ScreenToClient(hdlg, &o1);
        Rectangle(hdc, o0.x, o0.y, o1.x, o1.y);
        wchar_t lab[16];
#if defined(_MSC_VER)
        _snwprintf_s(lab, _countof(lab), _TRUNCATE, L"%d", GetDlgCtrlID(c));
#else
        swprintf(lab, 16, L"%d", GetDlgCtrlID(c));
#endif
        RECT lr = {o0.x + 1, o0.y + 1, o0.x + 34, o0.y + 13};
        DrawTextW(hdc, lab, -1, &lr, DT_LEFT | DT_TOP | DT_SINGLELINE);
    }
    SetTextColor(hdc, oldFg);
    SetBkMode(hdc, oldMode);
    SelectObject(hdc, oldBr);
    if (oldPen) SelectObject(hdc, oldPen);
    if (pen) DeleteObject(pen);
    ReleaseDC(hdlg, hdc);
}

#endif /* CATIME_UI_DEBUG */
