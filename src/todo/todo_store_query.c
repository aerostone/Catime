/**
 * @file todo_store_query.c
 * @brief Merged local+sync query: snapshot, filter, sort.
 *
 * Sort: overdue (past-due, open) first, then due date asc
 * (dateless last), then importance desc, then title.
 */
#include <stdlib.h>
#include <string.h>

#include "todo_store.h"
#include "todo_sync.h"

int TodoStore_SnapshotLocal(TodoTask *o, int cap);

static int AppendSync(TodoTask *out, int cap) {
    char lines[24][256];
    int n = TodoSync_GetLines(lines, 24);
    int today = 0, overdue = 0, someday = 0, pomo = 0;
    TodoSync_GetCounts(&today, &overdue, &someday, &pomo);
    (void)today; (void)someday; (void)pomo;
    int count = 0;
    for (int i = 0; i < n && count < cap; i++) {
        TodoTask t;
        memset(&t, 0, sizeof(t));
        t.source = TODO_SOURCE_SYNC;
        _snprintf_s(t.id, sizeof(t.id), _TRUNCATE, "S%d", i);
        /* lines look like " [ ] title" / " ! title": strip marker */
        const char *p = lines[i];
        while (*p == ' ') p++;
        if (p[0] == '[' && p[3] == ']') p += 4;
        else if (p[0] == '!') p += 1;
        while (*p == ' ') p++;
        strcpy_s(t.title, sizeof(t.title), p);
        t.done = FALSE;
        /* first `overdue` rows are overdue: backfill due as yesterday */
        if (i < overdue) {
            SYSTEMTIME st;
            GetLocalTime(&st);
            int d = (int)st.wDay - 1; /* sort hint only */
            if (d < 1) d = 1;
            _snprintf_s(t.dueDate, sizeof(t.dueDate), _TRUNCATE,
                        "%04d-%02d-%02d", (int)st.wYear, (int)st.wMonth, d);
        }
        t.importance = (i < overdue) ? TODO_IMPORTANCE_HIGH : TODO_IMPORTANCE_NONE;
        out[count++] = t;
    }
    return count;
}

static BOOL IsOverdue(const TodoTask *t, const char *todayStr) {
    if (t->done || !t->dueDate[0]) return FALSE;
    return strcmp(t->dueDate, todayStr) < 0;
}

static char g_sortToday[16] = "";

static int CmpTask(const void *a, const void *b) {
    const TodoTask *x = (const TodoTask *)a;
    const TodoTask *y = (const TodoTask *)b;
    BOOL xo = IsOverdue(x, g_sortToday);
    BOOL yo = IsOverdue(y, g_sortToday);
    if (xo != yo) return xo ? -1 : 1;
    BOOL xd = x->dueDate[0] != '\0';
    BOOL yd = y->dueDate[0] != '\0';
    if (xd && yd) {
        int c = strcmp(x->dueDate, y->dueDate);
        if (c) return c;
    } else if (xd != yd) {
        return xd ? -1 : 1;
    }
    if ((int)x->importance != (int)y->importance)
        return (int)y->importance - (int)x->importance;
    return strcmp(x->title, y->title);
}

int TodoStore_Query(const TodoFilter *filter, TodoTask *out, int outCap) {
    if (!out || outCap <= 0) return 0;
    TodoFilter df;
    if (!filter) {
        TodoFilter_InitDefault(&df);
        filter = &df;
    }
    TodoTask buf[TODO_STORE_MAX_TASKS];
    int n = TodoStore_SnapshotLocal(buf, TODO_STORE_MAX_TASKS);
    {
        TodoTask sync[24];
        int sn = AppendSync(sync, 24);
        for (int i = 0; i < sn && n < TODO_STORE_MAX_TASKS; i++)
            buf[n++] = sync[i];
    }
    TodoTask kept[TODO_STORE_MAX_TASKS];
    int k = 0;
    for (int i = 0; i < n; i++) {
        if (TodoTask_MatchesFilter(&buf[i], filter))
            kept[k++] = buf[i];
    }
    {
        SYSTEMTIME st;
        GetLocalTime(&st);
        _snprintf_s(g_sortToday, sizeof(g_sortToday), _TRUNCATE,
                    "%04d-%02d-%02d", (int)st.wYear, (int)st.wMonth, (int)st.wDay);
    }
    qsort(kept, (size_t)k, sizeof(kept[0]), CmpTask);
    int w = k < outCap ? k : outCap;
    for (int i = 0; i < w; i++) out[i] = kept[i];
    return w;
}

int TodoStore_OpenCount(void) {
    TodoFilter f;
    TodoFilter_InitDefault(&f);
    f.showDone = FALSE;
    TodoTask buf[TODO_STORE_MAX_TASKS];
    return TodoStore_Query(&f, buf, TODO_STORE_MAX_TASKS);
}
