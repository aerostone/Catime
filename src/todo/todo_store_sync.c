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
#include <time.h>

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

BOOL TodoStore_SetPinned(const char *id, BOOL pinned) {
    if (!id || !id[0]) return FALSE;
    TodoStore_Lock();
    int i = TodoStore_FindIndex(id);
    BOOL ok = FALSE;
    if (i >= 0) {
        TodoTask *t = TodoStore_RowAt(i);
        t->pinned = pinned ? TRUE : FALSE;
        t->updatedAt = (long long)time(NULL);
        TodoStore_Save();
        ok = TRUE;
    }
    TodoStore_Unlock();
    return ok;
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
    return TodoStore_UpdateFromSyncStamp(id, title, dueDate, imp, done, 0,
                                         NULL);
}

BOOL TodoStore_UpdateFromSyncStamp(const char *id, const char *title,
                                   const char *dueDate, TodoImportance imp,
                                   BOOL done, long long srvStamp,
                                   const char *srvId) {
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
        /* server stamp adopted: local push skips until newer local edit */
        if (srvStamp > 0) t->updatedAt = srvStamp;
        if (srvId && srvId[0])
            strcpy_s(t->serverId, sizeof(t->serverId), srvId);
        /* pulled rows land on the sync board and are never local ones */
        strcpy_s(t->board, sizeof(t->board), TODO_BOARD_SYNC);
        t->source = TODO_SOURCE_SYNC;
        TodoStore_Save();
        ok = TRUE;
    }
    TodoStore_Unlock();
    return ok;
}

BOOL TodoStore_InsertSynced(const char *id, const char *title,
                            const char *dueDate, TodoImportance imp,
                            BOOL done) {
    return TodoStore_InsertSyncedStamp(id, title, dueDate, imp, done, 0,
                                       NULL);
}

BOOL TodoStore_InsertSyncedStamp(const char *id, const char *title,
                                 const char *dueDate, TodoImportance imp,
                                 BOOL done, long long srvStamp,
                                 const char *srvId) {
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
        if (srvStamp > 0) t->updatedAt = srvStamp;
        else t->updatedAt = 0; /* unknown age: pull wins once, then stable */
        if (srvId && srvId[0])
            strcpy_s(t->serverId, sizeof(t->serverId), srvId);
        /* pulled rows land on the sync board and are never local ones */
        strcpy_s(t->board, sizeof(t->board), TODO_BOARD_SYNC);
        t->source = TODO_SOURCE_SYNC;
        TodoStore_Save();
        ok = TRUE;
    }
    TodoStore_Unlock();
    return ok;
}

void TodoStore_RecordDeleted(const char *id) {
    const char *txt = TodoStore_TxtPath();
    if (!txt || !txt[0] || !id || !id[0]) return;
    char p[MAX_PATH];
    _snprintf_s(p, sizeof(p), _TRUNCATE, "%s.deleted", txt);
    FILE *fp = NULL;
    if (fopen_s(&fp, p, "a") != 0 || !fp) return;
    fputs(id, fp);
    fputc('\n', fp);
    fclose(fp);
}

/* Read deleted ids into out (cap rows). Returns count. */
int TodoStore_DeletedIds(char out[][TODO_STORE_ID_LEN], int cap) {
    const char *txt = TodoStore_TxtPath();
    if (!txt || !txt[0] || !out || cap <= 0) return 0;
    char p[MAX_PATH];
    _snprintf_s(p, sizeof(p), _TRUNCATE, "%s.deleted", txt);
    FILE *fp = NULL;
    if (fopen_s(&fp, p, "r") != 0 || !fp) return 0;
    int n = 0;
    char line[128];
    while (n < cap && fgets(line, sizeof(line), fp)) {
        size_t L = strlen(line);
        while (L && (line[L - 1] == '\n' || line[L - 1] == '\r')) line[--L] = '\0';
        if (!line[0]) continue;
        strcpy_s(out[n], TODO_STORE_ID_LEN, line);
        n++;
    }
    fclose(fp);
    return n;
}

/* Remove one id from the deleted sidecar (server tombstone echo). */
void TodoStore_StripDeleted(const char *id) {
    char kept[64][TODO_STORE_ID_LEN];
    int n = TodoStore_DeletedIds(kept, 64);
    if (n <= 0 || !id || !id[0]) return;
    const char *txt = TodoStore_TxtPath();
    char p[MAX_PATH];
    _snprintf_s(p, sizeof(p), _TRUNCATE, "%s.deleted", txt);
    FILE *fp = NULL;
    if (fopen_s(&fp, p, "w") != 0 || !fp) return;
    for (int i = 0; i < n; i++) {
        if (strcmp(kept[i], id) == 0) continue;
        fputs(kept[i], fp);
        fputc('\n', fp);
    }
    fclose(fp);
}

/* Clear the deleted sidecar (call after successful push). */
void TodoStore_ClearDeleted(void) {
    const char *txt = TodoStore_TxtPath();
    if (!txt || !txt[0]) return;
    char p[MAX_PATH];
    _snprintf_s(p, sizeof(p), _TRUNCATE, "%s.deleted", txt);
    DeleteFileA(p);
}

/* Mark a freshly added row as living on the sync book: it shows on the
 * sync board and is pushed up on the next round. */
BOOL TodoStore_MarkSynced(const char *id) {
    if (!id || !id[0]) return FALSE;
    TodoStore_Lock();
    int i = TodoStore_FindIndex(id);
    BOOL ok = FALSE;
    if (i >= 0) {
        TodoTask *t = TodoStore_RowAt(i);
        strcpy_s(t->board, sizeof(t->board), TODO_BOARD_SYNC);
        t->source = TODO_SOURCE_SYNC;
        TodoStore_Save();
        ok = TRUE;
    }
    TodoStore_Unlock();
    return ok;
}
