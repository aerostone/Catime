/**
 * @file todo_match.c
 * @brief TodoFilter predicate: date/keyword/importance/source checks.
 */
#include <string.h>

#include "todo_types.h"

static BOOL DateGe(const char *taskDate, const char *bound) {
    if (!bound[0]) return TRUE;
    if (!taskDate[0]) return FALSE; /* bounded query excludes dateless */
    return strcmp(taskDate, bound) >= 0;
}

static BOOL DateLe(const char *taskDate, const char *bound) {
    if (!bound[0]) return TRUE;
    if (!taskDate[0]) return FALSE;
    return strcmp(taskDate, bound) <= 0;
}

static BOOL KeywordHit(const char *title, const char *kw) {
    if (!kw[0]) return TRUE;
    if (!title[0]) return FALSE;
    return strstr(title, kw) != NULL;
}

BOOL TodoTask_MatchesFilter(const TodoTask *t, const TodoFilter *f) {
    if (!t || !f) return FALSE;
    if (t->done && !f->showDone) return FALSE;
    if (t->source == TODO_SOURCE_LOCAL && !f->showLocal) return FALSE;
    if (t->source == TODO_SOURCE_SYNC && !f->showSync) return FALSE;
    if (f->minImportance >= 0 && (int)t->importance < f->minImportance)
        return FALSE;
    if (!DateGe(t->dueDate, f->fromDate)) return FALSE;
    if (!DateLe(t->dueDate, f->toDate)) return FALSE;
    if (!DateGe(t->doneAt, f->doneFrom)) return FALSE;
    if (!DateLe(t->doneAt, f->doneTo)) return FALSE;
    if (!KeywordHit(t->title, f->keyword)) return FALSE;
    return TRUE;
}
