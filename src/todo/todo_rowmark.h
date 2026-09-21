/**
 * @file todo_rowmark.h
 * @brief Unified importance/overdue row marks (ASCII, font-stable).
 *
 * One symbol system shared by tray menu, task-list dialog and sticky
 * title bars, replacing three ad-hoc schemes (tray U+203C/U+25CF,
 * list [!]/[ ], sticky color-only). Pure text so any menu/list/GDI
 * font renders it deterministically.
 *
 * Marks: [!] overdue, [A] high, [B] medium, [C] low, [ ] none, [x] done.
 */
#ifndef CATIME_TODO_ROWMARK_H
#define CATIME_TODO_ROWMARK_H

#include "todo_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Static 3-char mark for a task. */
const char *TodoRowMark(const TodoTask *t);
/* Shared overdue predicate (sticky ImpTag reuses it). */
BOOL TodoTask_IsOverdue(const TodoTask *t);

#ifdef __cplusplus
}
#endif

#endif /* CATIME_TODO_ROWMARK_H */
