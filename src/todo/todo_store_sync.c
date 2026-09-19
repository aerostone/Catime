/**
 * @file todo_store_sync.c
 * @brief Local store server-row upserts (merge driver backend).
 *
 * FindById + UpdateFromSync + InsertSynced share the core lock and
 * txt save path via extern accessors (no cross-TU statics).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "todo_normalize.h"
#include "todo_store.h"

void TodoStore_Lock(void);
void TodoStore_Unlock(void);
int TodoStore_FindIndex(const char *id);
int TodoStore_Count(void);
TodoTask *TodoStore_RowAt(int index);
const char *TodoStore_TxtPath(void);
void TodoStore_Save(void);

static void StampToday(char *out, size_t cap) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    _snprintf_s(out, cap, _TRUNCATE, "%04d-%02d-%02d", (int)st.wYear,
                (int)st.wMonth, (int)st.wDay);
}

BOOL TodoStore_FindById(const char *id, TodoTask *out) {
    if (!id || !id[0]) return FALSE;
    TodoStore_Lock();
    int i = TodoStore_FindIndex(id);
    BOOL ok = FALSE;
    if (i >= 0) {
        if (out) *out = *TodoStore_RowAt(i);
        ok = TRUE;
    }
    TodoStore_Unlock();
    return ok;
}

BOOL TodoStore_UpdateFromSync(const char *id, const char *title,
                              const char *dueDate, TodoImportance imp,
                              BOOL done) {
    if (!id || !id[0]) return FALSE;
    TodoStore_Lock();
    int i = TodoStore_FindIndex(id);
    BOOL ok = FALSE;
    if (i >= 0) {
        TodoTask *t = TodoStore_RowAt(i);
        if (title) {
            char clean[TODO_STORE_TITLE_LEN];
            TodoNormalize_Copy(title, clean, sizeof(clean));
            strcpy_s(t->title, sizeof(t->title), clean);
        }
        if (dueDate) strcpy_s(t->dueDate, sizeof(t->dueDate), dueDate);
        if (imp >= TODO_IMPORTANCE_NONE && imp <= TODO_IMPORTANCE_HIGH)
            t->importance = imp;
        if (t->done != done) {
            t->done = done;
            if (done) StampToday(t->doneAt, sizeof(t->doneAt));
            else t->doneAt[0] = '\0';
        }
        TodoStore_Save();
        ok = TRUE;
    }
    TodoStore_Unlock();
    return ok;
}

BOOL TodoStore_InsertSynced(const char *id, const char *title,
                            const char *dueDate, TodoImportance imp,
                            BOOL done) {
    if (!id || !id[0] || !title || !title[0]) return FALSE;
    TodoStore_Lock();
    BOOL ok = FALSE;
    if (TodoStore_Count() < TODO_STORE_MAX_TASKS &&
        TodoStore_FindIndex(id) < 0) {
        /* append via public Add then patch id: keeps id-counter intact */
        TodoStore_Unlock();
        if (!TodoStore_Add(title, imp, dueDate)) return FALSE;
        TodoStore_Lock();
        int i = TodoStore_Count() - 1;
        TodoTask *t = TodoStore_RowAt(i);
        strcpy_s(t->id, sizeof(t->id), id);
        t->done = done;
        if (done) StampToday(t->doneAt, sizeof(t->doneAt));
        TodoStore_Save();
        ok = TRUE;
    }
    TodoStore_Unlock();
    return ok;
}
