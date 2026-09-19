/**
 * @file todo_txt_file.c
 * @brief todo.txt file load/save: format lines + atomic persistence.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "todo_normalize.h"
#include "todo_txt.h"

void TodoTxt_FormatLine(const TodoTask *t, char *dst, size_t cap) {
    if (!dst || cap == 0) return;
    if (!t) {
        dst[0] = '\0';
        return;
    }
    char title[TODO_STORE_TITLE_LEN];
    TodoNormalize_Copy(t->title, title, sizeof(title));
    const char *prio = "";
    if (t->importance == TODO_IMPORTANCE_HIGH) prio = "(A) ";
    else if (t->importance == TODO_IMPORTANCE_MEDIUM) prio = "(B) ";
    else if (t->importance == TODO_IMPORTANCE_LOW) prio = "(C) ";
    /* tail extensions (spec order: due id pin upd srv) */
    char tail[192] = "";
    _snprintf_s(tail, sizeof(tail), _TRUNCATE, "%s%s%s%s%s%s",
                t->dueDate[0] ? " due:" : "", t->dueDate,
                t->id[0] ? " id:" : "", t->id,
                t->pinned ? " pin:1" : "",
                t->updatedAt > 0 ? " upd:" : "");
    if (t->updatedAt > 0) {
        char us[32];
        _snprintf_s(us, sizeof(us), _TRUNCATE, "%lld", t->updatedAt);
        strcat_s(tail, sizeof(tail), us);
    }
    if (t->serverId[0]) {
        strcat_s(tail, sizeof(tail), " srv:");
        strcat_s(tail, sizeof(tail), t->serverId);
    }
    if (t->done) {
        if (t->doneAt[0])
            _snprintf_s(dst, cap, _TRUNCATE, "x %s %s%s%s", t->doneAt,
                        prio, title, tail);
        else
            _snprintf_s(dst, cap, _TRUNCATE, "x %s%s%s", prio, title, tail);
    } else {
        if (t->createdAt[0])
            _snprintf_s(dst, cap, _TRUNCATE, "%s%s %s%s", prio,
                        t->createdAt, title, tail);
        else
            _snprintf_s(dst, cap, _TRUNCATE, "%s%s%s", prio, title, tail);
    }
}

int TodoTxt_LoadFile(const char *path, TodoTask *out, int cap) {
    if (!path || !path[0] || !out || cap <= 0) return 0;
    FILE *fp = NULL;
    if (fopen_s(&fp, path, "r") != 0 || !fp) return 0;
    char today[16] = "";
    SYSTEMTIME st;
    GetLocalTime(&st);
    _snprintf_s(today, sizeof(today), _TRUNCATE, "%04d-%02d-%02d",
                (int)st.wYear, (int)st.wMonth, (int)st.wDay);
    int n = 0;
    char line[1024];
    while (n < cap && fgets(line, sizeof(line), fp)) {
        TodoTask t;
        if (!TodoTxt_ParseLine(line, &t, today)) continue;
        out[n++] = t;
    }
    fclose(fp);
    return n;
}

BOOL TodoTxt_SaveFile(const char *path, const TodoTask *tasks, int count) {
    if (!path || !path[0] || !tasks || count < 0) return FALSE;
    char tmp[MAX_PATH];
    _snprintf_s(tmp, sizeof(tmp), _TRUNCATE, "%s.tmp", path);
    FILE *fp = NULL;
    if (fopen_s(&fp, tmp, "w") != 0 || !fp) return FALSE;
    for (int i = 0; i < count; i++) {
        char line[512];
        TodoTxt_FormatLine(&tasks[i], line, sizeof(line));
        fputs(line, fp);
        fputc('\n', fp);
    }
    BOOL ok = fclose(fp) == 0;
    if (!ok) {
        DeleteFileA(tmp);
        return FALSE;
    }
    /* atomic replace: tmp -> path */
    if (!MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileA(tmp);
        return FALSE;
    }
    return TRUE;
}
