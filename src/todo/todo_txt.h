/**
 * @file todo_txt.h
 * @brief todo.txt parse/serialize for the local task store.
 *
 * Line grammar (normalized input):
 *   [x <done-date>](<P>) [<created-date>] <title>
 *     [due:YYYY-MM-DD] [id:xxx] [pin:1] [+proj] [@ctx]
 * (A)=High (B)=Medium (C)=Low. Unknown key:value pairs are dropped
 * on parse (single source of truth = TodoTask).
 */
#ifndef CATIME_TODO_TXT_H
#define CATIME_TODO_TXT_H

#include "todo_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Parse one normalized line. Returns FALSE for blank/invalid lines. */
BOOL TodoTxt_ParseLine(const char *line, TodoTask *out, const char *defDate);

/* Serialize one task to a todo.txt line (dst cap incl. NUL). */
void TodoTxt_FormatLine(const TodoTask *t, char *dst, size_t cap);

/* Load whole file into tasks (cap TODO_STORE_MAX_TASKS). Returns count. */
int TodoTxt_LoadFile(const char *path, TodoTask *out, int cap);

/* Save tasks to file atomically (tmp + rename). Returns TRUE on success. */
BOOL TodoTxt_SaveFile(const char *path, const TodoTask *tasks, int count);

#ifdef __cplusplus
}
#endif

#endif /* CATIME_TODO_TXT_H */
