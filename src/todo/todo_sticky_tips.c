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

#include "language.h"
#include "todo/todo_board_layout.h"
#include "todo_stickies.h"

#define STICKY_TIP_PIN 9301
#define STICKY_TIP_BAR 9302
#define STICKY_TIP_BODY 9303
/* bar height mirrors todo_board_layout.h BOARD_BAR_H */
#define STICKY_BAR_H BOARD_BAR_H

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
        ti.lpszText = (LPWSTR)GetLocalizedString(L"\u70b9\u51fb\u5c55\u5f00\u4fbf\u7b7e", L"Click to expand");
        SendMessageW(tip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
        SendMessageW(tip, TTM_SETMAXTIPWIDTH, 0, 300);
        SendMessageW(tip, TTM_ACTIVATE, TRUE, 0);
        return;
    }
    /* N4: bar zones mirror the paint/hit layout (list + fold only). */
    ti.uId = STICKY_TIP_PIN;
    ti.rect.left = rc.right - BOARD_HIT_LIST;
    ti.rect.top = 0;
    ti.rect.right = rc.right;
    ti.rect.bottom = STICKY_BAR_H;
    ti.lpszText = (LPWSTR)GetLocalizedString(L"\u6253\u5f00\u4efb\u52a1\u5217\u8868", L"Open task list");
    SendMessageW(tip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    ti.uId = STICKY_TIP_BAR;
    ti.rect.left = 0;
    ti.rect.top = 0;
    ti.rect.right = rc.right - BOARD_HIT_LIST;
    ti.rect.bottom = STICKY_BAR_H;
    ti.lpszText = collapsed ? GetLocalizedString(L"\u53cc\u51fb\u5c55\u5f00", L"Double-click to expand") : GetLocalizedString(L"\u53cc\u51fb\u6536\u8d77", L"Double-click to collapse");
    SendMessageW(tip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    /* body zone */
    ti.uId = STICKY_TIP_BODY;
    ti.rect.left = 0;
    ti.rect.top = STICKY_BAR_H;
    ti.rect.right = rc.right;
    ti.rect.bottom = rc.bottom;
    ti.lpszText = (LPWSTR)GetLocalizedString(L"\u53cc\u51fb\u5207\u6362\u5b8c\u6210 \u00b7 \u53f3\u952e\u66f4\u591a\u64cd\u4f5c", L"Double-click toggles done; right-click for more");
    SendMessageW(tip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    SendMessageW(tip, TTM_SETMAXTIPWIDTH, 0, 300);
    SendMessageW(tip, TTM_ACTIVATE, TRUE, 0);
}
