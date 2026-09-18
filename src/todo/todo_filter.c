/**
 * @file todo_filter.c
 * @brief TodoFilter default init + single-task predicate.
 */
#include <string.h>

#include "todo_types.h"

void TodoFilter_InitDefault(TodoFilter *f) {
    if (!f) return;
    memset(f, 0, sizeof(*f));
    f->dueScope = TODO_DUE_SCOPE_WEEK;
    f->showDone = FALSE;
    f->showLocal = TRUE;
    f->showSync = TRUE;
    f->minImportance = -1;
}
