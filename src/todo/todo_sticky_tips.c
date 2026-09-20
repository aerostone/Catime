/**
 * @file todo_sticky_tips.c
 * @brief Sticky tooltip copy: honest pin glyphs + gesture discovery.
 *
 * Split target: keeps todo_stickies_window.c under the 300-line gate.
 * UpdateStickyTips attaches one tooltip control per sticky window with
 * three tools (pin zone / collapse zone / body zone).
 */
#include <string.h>

#include <windows.h>
#include <commctrl.h>

#include "todo_stickies.h"

#define STICKY_TIP_PIN 9301
#define STICKY_TIP_BAR 9302
#define STICKY_TIP_BODY 9303
#define STICKY_BAR_H 26

static HWND EnsureTip(HWND hwnd) {
    HWND tip = NULL;
    for (HWND c = GetWindow(hwnd, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        wchar_t cls[32] = L"";
        GetClassNameW(c, cls, _countof(cls));
        if (wcscmp(cls, TOOLTIPS_CLASSW) == 0) return c;
    }
    tip = CreateWindowExW(0, TOOLTIPS_CLASSW, NULL,
                          WS_POPUP | TTS_ALWAYSTIP,
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          hwnd, NULL, GetModuleHandleW(NULL), NULL);
    return tip;
}

void TodoSticky_UpdateTips(HWND hwnd, BOOL collapsed) {
    if (!hwnd) return;
    HWND tip = EnsureTip(hwnd);
    if (!tip) return;
    RECT rc;
    GetClientRect(hwnd, &rc);
    TOOLINFOW ti;
    memset(&ti, 0, sizeof(ti));
    ti.cbSize = sizeof(ti);
    ti.hwnd = hwnd;
    if (collapsed) {
        /* dot mode: one target, one hint */
        ti.uId = STICKY_TIP_BAR;
        ti.rect.left = 0;
        ti.rect.top = 0;
        ti.rect.right = rc.right;
        ti.rect.bottom = rc.bottom;
        ti.lpszText = L"\u70b9\u51fb\u5c55\u5f00\u4fbf\u7b7e";
        SendMessageW(tip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
        SendMessageW(tip, TTM_SETMAXTIPWIDTH, 0, 300);
        SendMessageW(tip, TTM_ACTIVATE, TRUE, 0);
        return;
    }
    /* pin zone: honest label, never "close" (D2) */
    ti.uId = STICKY_TIP_PIN;
    ti.rect.left = rc.right - 40;
    ti.rect.top = 0;
    ti.rect.right = rc.right;
    ti.rect.bottom = STICKY_BAR_H;
    ti.lpszText = L"\u53d6\u6d88\u7f6e\u9876\uff08\u4fdd\u7559\u4efb\u52a1\uff09";
    SendMessageW(tip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    /* collapse zone (D4 discovery) */
    ti.uId = STICKY_TIP_BAR;
    ti.rect.left = 0;
    ti.rect.top = 0;
    ti.rect.right = rc.right - 40;
    ti.rect.bottom = STICKY_BAR_H;
    ti.lpszText = collapsed ? L"\u53cc\u51fb\u5c55\u5f00" : L"\u53cc\u51fb\u6536\u8d77";
    SendMessageW(tip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    /* body zone */
    ti.uId = STICKY_TIP_BODY;
    ti.rect.left = 0;
    ti.rect.top = STICKY_BAR_H;
    ti.rect.right = rc.right;
    ti.rect.bottom = rc.bottom;
    ti.lpszText = L"\u53cc\u51fb\u53d6\u6d88\u4fbf\u7b7e \u00b7 \u53f3\u952e\u756a\u8304/\u5b8c\u6210/\u7f6e\u9876";
    SendMessageW(tip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    SendMessageW(tip, TTM_SETMAXTIPWIDTH, 0, 300);
    SendMessageW(tip, TTM_ACTIVATE, TRUE, 0);
}
