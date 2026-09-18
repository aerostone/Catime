/**
 * @file todo_stickies_render.c
 * @brief Sticky window painting: title bar, importance dot, body text.
 *
 * Shared by the window module. Colors: pale yellow default,
 * importance tints the title bar (none gray, low green, med orange,
 * high red). Done tasks render struck title via gray text.
 */
#include <windows.h>
#include <stdio.h>

#include "todo_types.h"

COLORREF TodoSticky_BarColor(TodoImportance imp, BOOL done) {
    if (done) return RGB(160, 160, 160);
    switch (imp) {
    case TODO_IMPORTANCE_LOW: return RGB(120, 190, 120);
    case TODO_IMPORTANCE_MEDIUM: return RGB(230, 170, 80);
    case TODO_IMPORTANCE_HIGH: return RGB(220, 110, 110);
    default: return RGB(150, 150, 150);
    }
}

COLORREF TodoSticky_BodyColor(void) {
    return RGB(255, 250, 205); /* lemon chiffon */
}

void TodoSticky_Paint(HDC hdc, const RECT *rc, const TodoTask *t,
                      HFONT fTitle, HFONT fBody) {
    if (!hdc || !rc || !t) return;
    int barH = 26;
    HBRUSH body = CreateSolidBrush(TodoSticky_BodyColor());
    if (body) {
        FillRect(hdc, rc, body);
        DeleteObject(body);
    }
    RECT bar = *rc;
    bar.bottom = bar.top + barH;
    HBRUSH bb = CreateSolidBrush(TodoSticky_BarColor(t->importance, t->done));
    if (bb) {
        FillRect(hdc, &bar, bb);
        DeleteObject(bb);
    }
    SetBkMode(hdc, TRANSPARENT);
    if (fTitle) SelectObject(hdc, fTitle);
    SetTextColor(hdc, RGB(40, 40, 40));
    RECT tr = bar;
    tr.left += 8; tr.right -= 30;
    wchar_t wt[TODO_STORE_TITLE_LEN];
    if (MultiByteToWideChar(CP_UTF8, 0, t->title, -1, wt, _countof(wt))) {
        DrawTextW(hdc, wt, -1, &tr,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    /* close "x" hint */
    if (fBody) SelectObject(hdc, fBody);
    SetTextColor(hdc, RGB(90, 90, 90));
    RECT xr = bar;
    xr.left = xr.right - 26;
    DrawTextW(hdc, L"x", -1, &xr, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    /* body: due date line + done state */
    RECT br = *rc;
    br.top += barH + 6; br.left += 8; br.right -= 8; br.bottom -= 6;
    SetTextColor(hdc, t->done ? RGB(140, 140, 140) : RGB(60, 60, 60));
    wchar_t info[64] = L"";
    if (t->dueDate[0]) {
        wchar_t wd[TODO_STORE_DATE_LEN];
        if (MultiByteToWideChar(CP_UTF8, 0, t->dueDate, -1, wd, _countof(wd)))
            _snwprintf_s(info, _countof(info), _TRUNCATE, L"截止 %s%s",
                         wd, t->done ? L" · 已完成" : L"");
    } else if (t->done) {
        wcscpy_s(info, _countof(info), L"已完成");
    } else {
        wcscpy_s(info, _countof(info), L"双击编辑内容");
    }
    DrawTextW(hdc, info, -1, &br, DT_LEFT | DT_TOP | DT_NOPREFIX);
}
