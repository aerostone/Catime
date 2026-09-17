/**
 * @file todo_overlay_draw.c
 * @brief Bottom-left TODO overlay on the layered main-window memDC.
 */
#include <windows.h>
#include <stdio.h>

#include "todo/todo_overlay_draw.h"
#include "todo/todo_sync.h"

void TodoOverlay_DrawOnMemDC(HDC memDC, int w, int h) {
    if (!memDC || w <= 0 || h <= 0) return;
    char lines[8][256];
    int n = TodoSync_GetLines(lines, 8);
    if (n <= 0) return;
    int today = 0, overdue = 0, someday = 0, pomo = 0;
    TodoSync_GetCounts(&today, &overdue, &someday, &pomo);

    HFONT font = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
    HGDIOBJ oldFont = font ? SelectObject(memDC, font) : NULL;
    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, RGB(255, 255, 255));

    /* header: 今日 T · 逾期 O · 池 S · 番茄 Pmin */
    wchar_t header[128];
    _snwprintf_s(header, _countof(header), _TRUNCATE,
                 L"今日 %d · 逾期 %d · 池 %d · 番茄 %dmin", today, overdue, someday, pomo);
    RECT hr = {8, h - (n + 1) * 17 - 20, w - 8, h - n * 17 - 4};
    DrawTextW(memDC, header, -1, &hr, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

    SetTextColor(memDC, RGB(230, 230, 230));
    for (int i = 0; i < n; i++) {
        wchar_t wl[256];
        MultiByteToWideChar(CP_UTF8, 0, lines[i], -1, wl, _countof(wl));
        RECT r = {8, h - (n - i) * 17 - 4, w - 8, h - (n - i - 1) * 17 - 4};
        DrawTextW(memDC, wl, -1, &r, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    if (oldFont) SelectObject(memDC, oldFont);
    if (font) DeleteObject(font);
}
