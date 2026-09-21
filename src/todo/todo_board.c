/**
 * @file todo_board.c
 * @brief Per-board view filter, geometry, visibility and task query.
 *
 * Stored in todo.ini (see todo_board_internal.h for section layout):
 *   [Board NAME] Scope=week|all Keyword=xxx Visible=0/1 Collapsed=0/1
 *                Topmost=-1|0|1 X= Y= W= H=
 *   [Sticky] Topmost=0|1 Opacity=30..100
 * Week scope windows only tasks that carry a due date: unscheduled
 * ("someday") tasks always stay visible.
 */
#include <stdio.h>
#include <string.h>

#include "todo_board.h"
#include "todo_date_range.h"
#include "todo_store.h"

#include "todo_board_internal.h"

TodoDueScope TodoBoard_Scope(const char *name) {
    char sec[96], v[16];
    TodoBoard_Sec(name, sec, sizeof(sec));
    TodoBoard_GetStr(sec, "Scope", "week", v, sizeof(v));
    return (strcmp(v, "all") == 0) ? TODO_DUE_SCOPE_ALL : TODO_DUE_SCOPE_WEEK;
}
/* RD7: the card owns only week/all; month/custom live in the dialog. */

void TodoBoard_SetScope(const char *name, TodoDueScope scope) {
    char sec[96];
    TodoBoard_Sec(name, sec, sizeof(sec));
    TodoBoard_PutStr(sec, "Scope",
                     scope == TODO_DUE_SCOPE_ALL ? "all" : "week");
}

void TodoBoard_Keyword(const char *name, char *out, size_t cap) {
    char sec[96];
    TodoBoard_Sec(name, sec, sizeof(sec));
    TodoBoard_GetStr(sec, "Keyword", "", out, cap);
}

void TodoBoard_SetKeyword(const char *name, const char *keyword) {
    char sec[96];
    TodoBoard_Sec(name, sec, sizeof(sec));
    TodoBoard_PutStr(sec, "Keyword", keyword ? keyword : "");
}

BOOL TodoBoard_Visible(const char *name) {
    char sec[96];
    TodoBoard_Sec(name, sec, sizeof(sec));
    const char *ini = TodoBoard_Ini();
    if (!ini) return FALSE;
    return GetPrivateProfileIntA(sec, "Visible", 0, ini) != 0;
}

void TodoBoard_SetVisible(const char *name, BOOL visible) {
    char sec[96];
    TodoBoard_Sec(name, sec, sizeof(sec));
    TodoBoard_PutInt(sec, "Visible", visible ? 1 : 0);
}

void TodoBoard_LoadGeom(const char *name, int *x, int *y, int *w, int *h) {
    char sec[96];
    TodoBoard_Sec(name, sec, sizeof(sec));
    const char *ini = TodoBoard_Ini();
    int dx = 140, dy = 140, dw = 320, dh = 260;
    if (ini) {
        dx = GetPrivateProfileIntA(sec, "X", dx, ini);
        dy = GetPrivateProfileIntA(sec, "Y", dy, ini);
        dw = GetPrivateProfileIntA(sec, "W", dw, ini);
        dh = GetPrivateProfileIntA(sec, "H", dh, ini);
    }
    if (dw < 260) dw = 260; /* RD2: +30% minimum */
    if (dh < 156) dh = 156;
    if (x) *x = dx;
    if (y) *y = dy;
    if (w) *w = dw;
    if (h) *h = dh;
}

void TodoBoard_SaveGeom(const char *name, int x, int y, int w, int h) {
    if (!name || !name[0]) return;
    char sec[96];
    TodoBoard_Sec(name, sec, sizeof(sec));
    TodoBoard_PutInt(sec, "X", x);
    TodoBoard_PutInt(sec, "Y", y);
    TodoBoard_PutInt(sec, "W", w);
    TodoBoard_PutInt(sec, "H", h);
}

BOOL TodoBoard_IsCollapsed(const char *name) {
    char sec[96];
    TodoBoard_Sec(name, sec, sizeof(sec));
    const char *ini = TodoBoard_Ini();
    if (!ini) return FALSE;
    return GetPrivateProfileIntA(sec, "Collapsed", 0, ini) != 0;
}

void TodoBoard_SetCollapsed(const char *name, BOOL collapsed) {
    char sec[96];
    TodoBoard_Sec(name, sec, sizeof(sec));
    TodoBoard_PutInt(sec, "Collapsed", collapsed ? 1 : 0);
}

BOOL TodoBoard_TopmostGlobal(void) {
    const char *ini = TodoBoard_Ini();
    if (!ini) return TRUE;
    return GetPrivateProfileIntA("Sticky", "Topmost", 1, ini) != 0;
}

void TodoBoard_SetTopmostGlobal(BOOL topmost) {
    TodoBoard_PutStr("Sticky", "Topmost", topmost ? "1" : "0");
}

int TodoBoard_Opacity(void) {
    const char *ini = TodoBoard_Ini();
    int pct = ini ? GetPrivateProfileIntA("Sticky", "Opacity", 96, ini) : 96;
    if (pct < 30) pct = 30;
    if (pct > 100) pct = 100;
    return pct;
}

void TodoBoard_SetOpacity(int pct) {
    if (pct < 30) pct = 30;
    if (pct > 100) pct = 100;
    TodoBoard_PutInt("Sticky", "Opacity", pct);
}

int TodoBoard_TopmostOverride(const char *name) {
    char sec[96];
    TodoBoard_Sec(name, sec, sizeof(sec));
    const char *ini = TodoBoard_Ini();
    if (!ini) return -1;
    return GetPrivateProfileIntA(sec, "Topmost", -1, ini);
}

void TodoBoard_SetTopmostOverride(const char *name, int mode) {
    if (mode < -1) mode = -1;
    if (mode > 1) mode = 1;
    char sec[96];
    TodoBoard_Sec(name, sec, sizeof(sec));
    TodoBoard_PutInt(sec, "Topmost", mode);
}

BOOL TodoBoard_TopmostFor(const char *name) {
    int ov = TodoBoard_TopmostOverride(name);
    if (ov == 0) return FALSE;
    if (ov == 1) return TRUE;
    return TodoBoard_TopmostGlobal();
}

/* Week scope drops dated tasks outside Mon..Sun; dateless tasks stay. */
static BOOL InScope(const TodoTask *t, TodoDueScope scope) {
    if (scope != TODO_DUE_SCOPE_WEEK) return TRUE;
    if (!t->dueDate[0]) return TRUE;
    char from[TODO_STORE_DATE_LEN], to[TODO_STORE_DATE_LEN];
    TodoDateRange_CurrentWeek(from, sizeof(from), to, sizeof(to));
    return strcmp(t->dueDate, from) >= 0 && strcmp(t->dueDate, to) <= 0;
}

int TodoBoard_Tasks(const char *name, TodoTask *out, int cap) {
    if (!name || !name[0] || !out || cap <= 0) return 0;
    BOOL sync = (strcmp(name, TODO_BOARD_SYNC) == 0);
    char kw[TODO_STORE_TITLE_LEN];
    TodoBoard_Keyword(name, kw, sizeof(kw));
    TodoDueScope scope = TodoBoard_Scope(name);
    TodoFilter f;
    TodoFilter_InitDefault(&f);
    f.showDone = FALSE;
    f.showLocal = TRUE;
    f.showSync = TRUE;
    strcpy_s(f.keyword, sizeof(f.keyword), kw);
    TodoTask buf[TODO_STORE_MAX_TASKS];
    int n = TodoStore_Query(&f, buf, TODO_STORE_MAX_TASKS);
    int k = 0;
    for (int i = 0; i < n && k < cap; i++) {
        const char *tb = buf[i].board[0] ? buf[i].board : TODO_BOARD_DEFAULT;
        if (strcmp(tb, name) != 0) continue;
        if (!!(buf[i].source == TODO_SOURCE_SYNC) != !!sync) continue;
        if (!InScope(&buf[i], scope)) continue;
        out[k++] = buf[i];
    }
    return k;
}

int TodoBoard_OpenCount(const char *name) {
    TodoTask buf[TODO_STORE_MAX_TASKS];
    return TodoBoard_Tasks(name, buf, TODO_STORE_MAX_TASKS);
}

BOOL TodoBoard_AssignTask(const char *taskId, const char *boardName) {
    return TodoStore_SetBoard(taskId, boardName);
}
