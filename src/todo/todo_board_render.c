/**
 * @file todo_board_render.c
 * @brief Painting for a board card: title bar, filter row, task rows.
 *
 * Colors: the board card is a flat "sticky note" - warm body, tinted
 * title bar, alternate row bands. Text is drawn with GDI only (no
 * theme APIs) so it also works on the layered sticky windows.
 */
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "todo_board.h"
#include "todo_store.h"

#include "todo_board_layout.h"

#define COL_BODY RGB(255, 250, 226)
#define COL_BAR RGB(246, 214, 128)
#define COL_BAR_DONE RGB(214, 214, 214)
#define COL_LINE RGB(232, 220, 190)
#define COL_TEXT RGB(48, 44, 36)
#define COL_DIM RGB(128, 122, 108)
#define COL_DUE RGB(190, 84, 56)
#define COL_SEL RGB(255, 240, 196)

#include "todo_board_render.h"

static void SetFont(HDC hdc, HFONT f) {
    if (f) SelectObject(hdc, f);
}

static void TextAt(HDC hdc, int x, int y, const wchar_t *s, COLORREF c) {
    SetTextColor(hdc, c);
    TextOutW(hdc, x, y, s, (int)wcslen(s));
}

void TodoBoard_BarColor(TodoImportance imp, BOOL done, COLORREF *out) {
    if (done) {
        *out = COL_BAR_DONE;
        return;
    }
    switch (imp) {
    case TODO_IMPORTANCE_HIGH: *out = RGB(244, 168, 150); break;
    case TODO_IMPORTANCE_MEDIUM: *out = RGB(246, 214, 128); break;
    case TODO_IMPORTANCE_LOW: *out = RGB(206, 226, 186); break;
    default: *out = COL_BAR; break;
    }
}

/* Importance marker drawn in front of a row title. */
static const wchar_t *ImpTag(TodoImportance imp) {
    switch (imp) {
    case TODO_IMPORTANCE_HIGH: return L"(A)";
    case TODO_IMPORTANCE_MEDIUM: return L"(B)";
    case TODO_IMPORTANCE_LOW: return L"(C)";
    default: return L"-";
    }
}

/* --- title bar ------------------------------------------------------ */
void TodoBoard_PaintTitle(HDC hdc, const RECT *rc, const char *board,
                          int openCount, BOOL collapsed, int pomoRemSec,
                          HFONT fTitle) {
    RECT bar = *rc;
    bar.bottom = bar.top + BOARD_BAR_H;
    HBRUSH b = CreateSolidBrush(COL_BAR);
    if (b) {
        FillRect(hdc, &bar, b);
        DeleteObject(b);
    }
    SetBkMode(hdc, TRANSPARENT);
    SetFont(hdc, fTitle);
    wchar_t wname[64] = L"";
    MultiByteToWideChar(CP_UTF8, 0, board ? board : "", -1, wname, 64);
    wchar_t wbuf[96];
    if (pomoRemSec > 0) {
        _snwprintf_s(wbuf, 96, _TRUNCATE, L"%s  %d   P %02d:%02d", wname,
                     openCount, pomoRemSec / 60, pomoRemSec % 60);
    } else if (collapsed) {
        _snwprintf_s(wbuf, 96, _TRUNCATE, L"%s (%d)", wname, openCount);
    } else {
        _snwprintf_s(wbuf, 96, _TRUNCATE, L"%s  %d", wname, openCount);
    }
    RECT tx = bar;
    tx.left += 6;
    tx.right -= (BOARD_HIT_CLOSE + BOARD_HIT_LIST + BOARD_HIT_FOLD);
    if (tx.right < tx.left) tx.right = tx.left;
    SetTextColor(hdc, COL_TEXT);
    DrawTextW(hdc, wbuf, -1, &tx,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS |
                  DT_NOPREFIX);
    /* right-hand glyphs: fold, list, close */
    SetTextColor(hdc, COL_TEXT);
    int right = bar.right;
    const wchar_t *gFold = collapsed ? L"▢" : L"–";
    TextOutW(hdc, right - BOARD_HIT_CLOSE - BOARD_HIT_LIST -
                      BOARD_HIT_FOLD + 4,
             bar.top + 6, gFold, (int)wcslen(gFold));
    TextOutW(hdc, right - BOARD_HIT_CLOSE - BOARD_HIT_LIST + 4, bar.top + 6,
             L"≡", 1);
    TextOutW(hdc, right - BOARD_HIT_CLOSE + 3, bar.top + 6, L"×", 1);
    HPEN pen = CreatePen(PS_SOLID, 1, COL_LINE);
    if (pen) {
        HPEN old = (HPEN)SelectObject(hdc, pen);
        MoveToEx(hdc, rc->left, bar.bottom - 1, NULL);
        LineTo(hdc, rc->right, bar.bottom - 1);
        SelectObject(hdc, old);
        DeleteObject(pen);
    }
}

/* --- filter row ----------------------------------------------------- */
void TodoBoard_PaintFilter(HDC hdc, const RECT *rc, TodoDueScope scope,
                           const char *keyword, HFONT fBody) {
    (void)keyword; /* the keyword text lives in the child edit control */
    RECT fr = *rc;
    fr.top = rc->top + BOARD_BAR_H;
    fr.bottom = fr.top + BOARD_FILTER_H;
    HBRUSH b = CreateSolidBrush(COL_BODY);
    if (b) {
        FillRect(hdc, &fr, b);
        DeleteObject(b);
    }
    SetBkMode(hdc, TRANSPARENT);
    SetFont(hdc, fBody);
    BOOL week = (scope != TODO_DUE_SCOPE_ALL);
    RECT tag = fr;
    tag.left += 4;
    tag.right = tag.left + BOARD_HIT_SCOPE_W;
    HBRUSH tb = CreateSolidBrush(week ? COL_SEL : COL_BODY);
    if (tb) {
        FillRect(hdc, &tag, tb);
        DeleteObject(tb);
    }
    SetTextColor(hdc, COL_TEXT);
    DrawTextW(hdc, week ? L"本周" : L"全部", -1, &tag,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    HPEN pen = CreatePen(PS_SOLID, 1, COL_LINE);
    if (pen) {
        HPEN old = (HPEN)SelectObject(hdc, pen);
        MoveToEx(hdc, rc->left, fr.bottom - 1, NULL);
        LineTo(hdc, rc->right, fr.bottom - 1);
        SelectObject(hdc, old);
        DeleteObject(pen);
    }
}

/* --- task rows ------------------------------------------------------ */
void TodoBoard_PaintRows(HDC hdc, const RECT *rc, const TodoTask *tasks,
                         int count, HFONT fBody) {
    SetBkMode(hdc, TRANSPARENT);
    SetFont(hdc, fBody);
    if (count <= 0) {
        RECT e = *rc;
        e.top = BOARD_BODY_TOP + 8;
        SetTextColor(hdc, COL_DIM);
        const wchar_t *msg = L"没有任务 (右键添加)";
        DrawTextW(hdc, msg, -1, &e,
                  DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        return;
    }
    for (int i = 0; i < count; i++) {
        int top = BOARD_BODY_TOP + i * BOARD_ROW_H;
        if (top + BOARD_ROW_H > rc->bottom && i > 0) break;
        RECT rr = *rc;
        rr.top = top;
        rr.bottom = top + BOARD_ROW_H;
        if (i % 2) {
            HBRUSH b = CreateSolidBrush(RGB(252, 246, 224));
            if (b) {
                FillRect(hdc, &rr, b);
                DeleteObject(b);
            }
        }
        wchar_t wt[TODO_STORE_TITLE_LEN] = L"";
        MultiByteToWideChar(CP_UTF8, 0, tasks[i].title, -1, wt,
                            TODO_STORE_TITLE_LEN);
        RECT tx = rr;
        tx.left += 4;
        tx.right -= 76;
        if (tx.right < tx.left) tx.right = tx.left;
        SetTextColor(hdc, tasks[i].done ? COL_DIM : COL_TEXT);
        wchar_t line[TODO_STORE_TITLE_LEN + 8];
        _snwprintf_s(line, sizeof(line) / sizeof(line[0]), _TRUNCATE, L"%s %s",
                     ImpTag(tasks[i].importance), wt);
        DrawTextW(hdc, line, -1, &tx,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS |
                      DT_NOPREFIX);
        /* right column: due date / done marker */
        wchar_t right[24] = L"";
        if (tasks[i].done) {
            wcscpy_s(right, 24, L"✓");
        } else if (tasks[i].dueDate[0]) {
            MultiByteToWideChar(CP_UTF8, 0, tasks[i].dueDate + 5, -1, right, 24);
        }
        if (right[0]) {
            RECT r2 = rr;
            r2.right -= 4;
            r2.left = r2.right - 68;
            SetTextColor(hdc, tasks[i].done ? COL_DIM : COL_DUE);
            DrawTextW(hdc, right, -1, &r2,
                      DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
    }
}
