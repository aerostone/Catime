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
#include "todo_rowmark.h"
#include "todo_store.h"
#include "language.h"

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

/* Importance marker drawn in front of a row title.
 * [!] is reserved for overdue (L3); [A]/[B]/[C] only reflect the
 * user-set importance so the two meanings never share a symbol. */
static const wchar_t *ImpTag(const TodoTask *t) {
    if (!t->done && t->dueDate[0] && TodoTask_IsOverdue(t)) return L"[!]";
    switch (t->importance) {
    case TODO_IMPORTANCE_HIGH: return L"(A)";
    case TODO_IMPORTANCE_MEDIUM: return L"(B)";
    case TODO_IMPORTANCE_LOW: return L"(C)";
    default: return L"-";
    }
}

/* --- title bar (N4/N6): drag + fold + list only. Hide lives in the
 * card menu; the close glyph is a pin (hide), never a lying x. ---- */
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
        wchar_t fmt[32];
        wcsncpy_s(fmt, 32,
                  GetLocalizedString(L"%s  %d   \u756A\u8304 %02d:%02d",
                                     L"%s  %d   Pomodoro %02d:%02d"),
                  _TRUNCATE);
        _snwprintf_s(wbuf, 96, _TRUNCATE, fmt, wname,
                     openCount, pomoRemSec / 60, pomoRemSec % 60);
    } else if (collapsed) {
        _snwprintf_s(wbuf, 96, _TRUNCATE, L"%s (%d)", wname, openCount);
    } else {
        _snwprintf_s(wbuf, 96, _TRUNCATE, L"%s  %d", wname, openCount);
    }
    RECT tx = bar;
    tx.left += 6;
    tx.right -= (BOARD_HIT_LIST + BOARD_HIT_FOLD);
    if (tx.right < tx.left) tx.right = tx.left;
    SetTextColor(hdc, COL_TEXT);
    DrawTextW(hdc, wbuf, -1, &tx,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS |
                  DT_NOPREFIX);
    /* right-hand buttons (RD6): explicit fold/unfold + open-manager.
     * Glyphs are drawn centered in their 26px hit zones. */
    SetTextColor(hdc, COL_TEXT);
    int right = bar.right;
    {
        const wchar_t *gFold = collapsed ? L"\u25A2" : L"\u2013";
        int cx = right - BOARD_HIT_LIST - BOARD_HIT_FOLD +
                 BOARD_HIT_FOLD / 2 - 5;
        TextOutW(hdc, cx, bar.top + 8, gFold, (int)wcslen(gFold));
    }
    {
        int cx = right - BOARD_HIT_LIST + BOARD_HIT_LIST / 2 - 5;
        TextOutW(hdc, cx, bar.top + 8, L"\u2261", 1);
    }
    HPEN pen = CreatePen(PS_SOLID, 1, COL_LINE);
    if (pen) {
        HPEN old = (HPEN)SelectObject(hdc, pen);
        MoveToEx(hdc, rc->left, bar.bottom - 1, NULL);
        LineTo(hdc, rc->right, bar.bottom - 1);
        SelectObject(hdc, old);
        DeleteObject(pen);
    }
}

/* --- minimized dot (RD6): translucent 52px dot tinted by peak
 * importance; the count rides inside so nothing else is needed. ---- */
void TodoBoard_PaintDot(HDC hdc, const RECT *rc, TodoImportance peak,
                        int openCount) {
    COLORREF fill;
    TodoBoard_BarColor(peak, FALSE, &fill);
    HBRUSH b = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 2, COL_LINE);
    HBRUSH oldB = NULL;
    HPEN oldP = NULL;
    if (b) oldB = (HBRUSH)SelectObject(hdc, b);
    if (pen) oldP = (HPEN)SelectObject(hdc, pen);
    Ellipse(hdc, rc->left + 2, rc->top + 2, rc->right - 2, rc->bottom - 2);
    if (oldB) SelectObject(hdc, oldB);
    if (oldP) SelectObject(hdc, oldP);
    if (b) DeleteObject(b);
    if (pen) DeleteObject(pen);
    wchar_t wnum[16];
    _snwprintf_s(wnum, 16, _TRUNCATE, L"%d", openCount);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, COL_TEXT);
    DrawTextW(hdc, wnum, -1, (LPRECT)rc,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
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
    TodoBoard_PaintRowsEx(hdc, rc, tasks, count, fBody, NULL, -1);
}

#define COL_HOVER RGB(255, 243, 205)
#define COL_SELFOCUS RGB(255, 232, 170)

void TodoBoard_PaintRowsEx(HDC hdc, const RECT *rc, const TodoTask *tasks,
                           int count, HFONT fBody, const char *selId,
                           int hoverRow) {
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
    int more = 0; /* N2: rows that do not fit are announced, never cut */
    for (int i = 0; i < count; i++) {
        int top = BOARD_BODY_TOP + i * BOARD_ROW_H;
        if (top + BOARD_ROW_H > rc->bottom && i > 0) {
            more = count - i;
            break;
        }
        RECT rr = *rc;
        rr.top = top;
        rr.bottom = top + BOARD_ROW_H;
        {
            BOOL sel = selId && selId[0] &&
                       strcmp(tasks[i].id, selId) == 0;
            COLORREF band = 0;
            if (sel) band = COL_SELFOCUS;
            else if (i == hoverRow) band = COL_HOVER;
            if (band) {
                HBRUSH hb = CreateSolidBrush(band);
                if (hb) {
                    FillRect(hdc, &rr, hb);
                    DeleteObject(hb);
                }
            } else if (i % 2) {
                HBRUSH b = CreateSolidBrush(RGB(252, 246, 224));
                if (b) {
                    FillRect(hdc, &rr, b);
                    DeleteObject(b);
                }
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
                     ImpTag(&tasks[i]), wt);
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
    if (more > 0) {
        RECT mr = *rc;
        mr.top = rc->bottom - BOARD_ROW_H;
        mr.bottom = rc->bottom;
        if (mr.top >= BOARD_BODY_TOP) {
            SetTextColor(hdc, COL_DIM);
            wchar_t mm[64];
            _snwprintf_s(mm, 64, _TRUNCATE,
                         L"\u2026\u8FD8\u6709 %d \u9879\uFF0C\u53CC\u51FB\u6253\u5F00\u5217\u8868", more);
            DrawTextW(hdc, mm, -1, &mr,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
    }
}
