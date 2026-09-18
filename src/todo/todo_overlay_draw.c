/**
 * @file todo_overlay_draw.c
 * @brief Bottom-left TODO overlay on the layered main-window memDC.
 */
#include <windows.h>
#include <stdio.h>

#include "todo/todo_overlay_draw.h"
#include "todo/todo_store.h"
#include "todo/todo_sync.h"

void TodoOverlay_DrawOnMemDC(HDC memDC, int w, int h) {
    if (!memDC || w <= 0 || h <= 0) return;
    /* merged local + sync view: open tasks only */
    TodoFilter f;
    TodoFilter_InitDefault(&f);
    f.showDone = FALSE;
    TodoTask tasks[16];
    int n = TodoStore_Query(&f, tasks, 16);
    if (n <= 0) return;
    char lines[8][256];
    int m = n < 8 ? n : 8;
    for (int i = 0; i < m; i++) {
        const char *mark = tasks[i].done ? "[x]" :
            (tasks[i].source == TODO_SOURCE_SYNC && tasks[i].importance == TODO_IMPORTANCE_HIGH) ? "!" : "[ ]";
        _snprintf_s(lines[i], sizeof(lines[i]), _TRUNCATE, " %s %s", mark, tasks[i].title);
    }
    int open = TodoStore_OpenCount();
    int pomo = 0;
    {
        int t = 0, o = 0, s = 0;
        TodoSync_GetCounts(&t, &o, &s, &pomo);
        (void)t; (void)o; (void)s;
    }

    HFONT font = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
    if (!font) return;
    HGDIOBJ oldFont = SelectObject(memDC, font);
    if (oldFont == HGDI_ERROR) { DeleteObject(font); return; }
    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, RGB(255, 255, 255));

    /* header: 待办 open · 番茄 pomo min */
    wchar_t header[128];
    _snwprintf_s(header, _countof(header), _TRUNCATE,
                 L"待办 %d · 番茄 %dmin", open, pomo);
    RECT hr = {8, h - (m + 1) * 17 - 20, w - 8, h - m * 17 - 4};
    DrawTextW(memDC, header, -1, &hr, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

    SetTextColor(memDC, RGB(230, 230, 230));
    for (int i = 0; i < m; i++) {
        wchar_t wl[256];
        if (!MultiByteToWideChar(CP_UTF8, 0, lines[i], -1, wl, _countof(wl))) continue;
        RECT r = {8, h - (m - i) * 17 - 4, w - 8, h - (m - i - 1) * 17 - 4};
        if (r.right > r.left && r.bottom > r.top)
            DrawTextW(memDC, wl, -1, &r, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    if (oldFont) SelectObject(memDC, oldFont);
    if (font) DeleteObject(font);
}
