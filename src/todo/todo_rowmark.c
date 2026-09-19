/**
 * @file todo_rowmark.c
 * @brief Unified [A]/[B]/[C]/[ ]/[x] row marks with overdue override.
 */
#include "todo_rowmark.h"

#include <string.h>
#include <time.h>

static BOOL RowOverdue(const TodoTask *t) {
    if (!t || t->done || !t->dueDate[0] || strlen(t->dueDate) < 10)
        return FALSE;
    time_t now = time(NULL);
    struct tm lt;
#if defined(_MSC_VER)
    if (localtime_s(&lt, &now) != 0) return FALSE;
#else
    struct tm *p = localtime(&now);
    if (!p) return FALSE;
    lt = *p;
#endif
    char today[16] = "";
#if defined(_MSC_VER)
    _snprintf_s(today, sizeof(today), _TRUNCATE, "%04d-%02d-%02d",
                lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
#else
    snprintf(today, sizeof(today), "%04d-%02d-%02d",
             lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
#endif
    return strcmp(t->dueDate, today) < 0;
}

const char *TodoRowMark(const TodoTask *t) {
    if (!t) return "[ ]";
    if (t->done) return "[x]";
    if (RowOverdue(t)) return "[A]";
    switch (t->importance) {
    case TODO_IMPORTANCE_HIGH: return "[A]";
    case TODO_IMPORTANCE_MEDIUM: return "[B]";
    case TODO_IMPORTANCE_LOW: return "[C]";
    default: return "[ ]";
    }
}
