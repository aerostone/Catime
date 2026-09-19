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
    TodoSticky_PaintEx(hdc, rc, t, fTitle, fBody, FALSE, 0);
}

void TodoSticky_PaintEx(HDC hdc, const RECT *rc, const TodoTask *t,
                        HFONT fTitle, HFONT fBody, BOOL collapsed,
                        int pomoRemSec) {
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
    tr.left += 8; tr.right = bar.right - 74; /* D1: reserve 70px glyph zone */
    wchar_t mark[8] = L"";
    switch (t->importance) {
    case TODO_IMPORTANCE_HIGH: wcscpy_s(mark, _countof(mark), L"[!] "); break;
    case TODO_IMPORTANCE_MEDIUM: wcscpy_s(mark, _countof(mark), L"[B] "); break;
    case TODO_IMPORTANCE_LOW: wcscpy_s(mark, _countof(mark), L"[C] "); break;
    default: break;
    }
    if (t->done) wcscpy_s(mark, _countof(mark), L"[x] ");
    wchar_t wt[TODO_STORE_TITLE_LEN];
    if (MultiByteToWideChar(CP_UTF8, 0, t->title, -1, wt, _countof(wt))) {
        wchar_t full[TODO_STORE_TITLE_LEN + 8];
        _snwprintf_s(full, _countof(full), _TRUNCATE, L"%s%s", mark, wt);
        DrawTextW(hdc, full, -1, &tr,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    /* collapse glyph + close "x" hint */
    if (fBody) SelectObject(hdc, fBody);
    SetTextColor(hdc, RGB(90, 90, 90));
    RECT xr = bar;
    xr.left = xr.right - 70;
    xr.right = xr.right - 44;
    DrawTextW(hdc, collapsed ? L"[+]" : L"[-]", -1, &xr,
              DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    /* D2: pin-state glyph (◉ pinned / ○ unpinned), not a close X. */
    RECT cr = bar;
    cr.left = cr.right - 40;
    cr.right = cr.right - 18;
    DrawTextW(hdc, t->pinned ? L"\u25c9" : L"\u25cb", -1, &cr,
              DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    RECT hr = bar;
    hr.left = hr.right - 18;
    DrawTextW(hdc, L"\u2013", -1, &hr,
              DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    if (pomoRemSec > 0) {
        /* D1 fix: countdown lives in the body, never over the title. */
        wchar_t pomo[32];
        _snwprintf_s(pomo, _countof(pomo), _TRUNCATE, L"\u5515 %d:%02d",
                     pomoRemSec / 60, pomoRemSec % 60);
        RECT br0 = *rc;
        br0.top += barH + 6; br0.left += 8; br0.right -= 8;
        br0.bottom = br0.top + 18;
        SetTextColor(hdc, RGB(185, 28, 28));
        DrawTextW(hdc, pomo, -1, &br0,
                  DT_LEFT | DT_TOP | DT_NOPREFIX | DT_SINGLELINE);
    }
    /* body: due date line + done state (D4: gesture hint rotates) */
    static volatile LONG s_hintFlip = 0;
    RECT br = *rc;
    br.top += barH + 6 + (pomoRemSec > 0 ? 20 : 0); br.left += 8; br.right -= 8; br.bottom -= 6;
    SetTextColor(hdc, t->done ? RGB(140, 140, 140) : RGB(60, 60, 60));
    wchar_t info[64] = L"";
    if (t->dueDate[0]) {
        wchar_t wd[TODO_STORE_DATE_LEN];
        if (MultiByteToWideChar(CP_UTF8, 0, t->dueDate, -1, wd, _countof(wd)))
            _snwprintf_s(info, _countof(info), _TRUNCATE, L"截止 %s%s",
                         wd, t->done ? L" · 已完成" : L"");
    } else if (t->done) {
        wcscpy_s(info, _countof(info), L"已完成");
    } else if ((InterlockedIncrement(&s_hintFlip) / 60) % 2 == 0) {
        wcscpy_s(info, _countof(info), L"双击编辑内容");
    } else {
        wcscpy_s(info, _countof(info), L"双击标题栏收起");
    }
    DrawTextW(hdc, info, -1, &br, DT_LEFT | DT_TOP | DT_NOPREFIX);
}
