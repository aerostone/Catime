/**
 * @file todo_store_core.c
 * @brief Local TODO store: lifecycle, CRUD, persistence (todo.ini).
 *
 * File layout: [Todo] Count=N + per-task [Task<i>] sections.
 * Dates stored as YYYY-MM-DD strings. All disk I/O is UTF-8 via
 * Get/WritePrivateProfileStringA on the todo.ini path.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "todo_normalize.h"
#include "todo_store.h"
#include "todo_sync.h"
#include "todo_txt.h"

static CRITICAL_SECTION s_lock;
static BOOL s_lockInit = FALSE;
static TodoTask s_tasks[TODO_STORE_MAX_TASKS];
static int s_count = 0;
static char s_iniPath[MAX_PATH] = "";
static char s_txtPath[MAX_PATH] = "";
static long s_nextId = 1;

void TodoStore_Lock(void) { EnterCriticalSection(&s_lock); }
void TodoStore_Unlock(void) { LeaveCriticalSection(&s_lock); }

static void TodayStr(char *out, size_t cap) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    _snprintf_s(out, cap, _TRUNCATE, "%04d-%02d-%02d",
                (int)st.wYear, (int)st.wMonth, (int)st.wDay);
}

static BOOL ValidDate(const char *d) {
    if (!d || !d[0]) return TRUE; /* empty = no date */
    if (strlen(d) != 10 || d[4] != '-' || d[7] != '-') return FALSE;
    for (int i = 0; d[i]; i++) {
        if (i == 4 || i == 7) continue;
        if (d[i] < '0' || d[i] > '9') return FALSE;
    }
    return TRUE;
}

static void MakeId(char *out, size_t cap) {
    _snprintf_s(out, cap, _TRUNCATE, "L%ld", s_nextId++);
}

int TodoStore_FindIndex(const char *id) {
    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_tasks[i].id, id) == 0) return i;
    }
    return -1;
}

static void SaveLocked(void) {
/* forward for TodoStore_Save */
    if (s_txtPath[0])
        TodoTxt_SaveFile(s_txtPath, s_tasks, s_count);
}

/* One-time import: legacy todo.ini sections -> memory. Runs only when
 * todo.txt is missing but todo.ini exists with Count > 0. */
static void ImportLegacyIniLocked(void) {
    int n = GetPrivateProfileIntA("Todo", "Count", 0, s_iniPath);
    if (n <= 0) return;
    if (n > TODO_STORE_MAX_TASKS) n = TODO_STORE_MAX_TASKS;
    for (int i = 0; i < n && s_count < TODO_STORE_MAX_TASKS; i++) {
        char sec[32];
        _snprintf_s(sec, sizeof(sec), _TRUNCATE, "Task%d", i);
        TodoTask t;
        memset(&t, 0, sizeof(t));
        t.source = TODO_SOURCE_LOCAL;
        char rawTitle[TODO_STORE_TITLE_LEN] = "";
        GetPrivateProfileStringA(sec, "Id", "", t.id, sizeof(t.id), s_iniPath);
        GetPrivateProfileStringA(sec, "Title", "", rawTitle, sizeof(rawTitle),
                                 s_iniPath);
        if (!t.id[0] || !rawTitle[0]) continue;
        TodoNormalize_Copy(rawTitle, t.title, sizeof(t.title));
        t.importance =
            (TodoImportance)GetPrivateProfileIntA(sec, "Importance", 0,
                                                  s_iniPath);
        if (t.importance < TODO_IMPORTANCE_NONE)
            t.importance = TODO_IMPORTANCE_NONE;
        if (t.importance > TODO_IMPORTANCE_HIGH)
            t.importance = TODO_IMPORTANCE_HIGH;
        t.done = GetPrivateProfileIntA(sec, "Done", 0, s_iniPath) != 0;
        GetPrivateProfileStringA(sec, "Due", "", t.dueDate, sizeof(t.dueDate),
                                 s_iniPath);
        GetPrivateProfileStringA(sec, "DoneAt", "", t.doneAt, sizeof(t.doneAt),
                                 s_iniPath);
        GetPrivateProfileStringA(sec, "Created", "", t.createdAt,
                                 sizeof(t.createdAt), s_iniPath);
        s_tasks[s_count++] = t;
    }
    SaveLocked(); /* persist imported rows as todo.txt immediately */
}

static void LoadLocked(void) {
    s_count = 0;
    s_nextId = 1;
    if (s_txtPath[0]) {
        s_count = TodoTxt_LoadFile(s_txtPath, s_tasks, TODO_STORE_MAX_TASKS);
        /* recover id counter from max L-number */
        for (int i = 0; i < s_count; i++) {
            if (s_tasks[i].id[0] == 'L') {
                long v = atol(s_tasks[i].id + 1);
                if (v >= s_nextId) s_nextId = v + 1;
            }
        }
        if (s_count == 0 && s_iniPath[0])
            ImportLegacyIniLocked();
    }
}

BOOL TodoStore_Init(const wchar_t *configIniPath) {
    if (!s_lockInit) {
        InitializeCriticalSection(&s_lock);
        s_lockInit = TRUE;
    }
    TodoStore_Lock();
    s_iniPath[0] = '\0';
    if (configIniPath && configIniPath[0]) {
        char cfgA[MAX_PATH] = "";
        if (WideCharToMultiByte(CP_UTF8, 0, configIniPath, -1,
                                cfgA, sizeof(cfgA), NULL, NULL)) {
            char *slash = strrchr(cfgA, '\\');
            if (!slash) slash = strrchr(cfgA, '/');
            size_t dirLen = slash ? (size_t)(slash - cfgA + 1) : 0;
            if (dirLen + 9 < sizeof(s_iniPath)) {
                if (dirLen) memcpy(s_iniPath, cfgA, dirLen);
                strcpy_s(s_iniPath + dirLen, sizeof(s_iniPath) - dirLen, "todo.ini");
                if (dirLen + 9 < sizeof(s_txtPath)) {
                    if (dirLen) memcpy(s_txtPath, cfgA, dirLen);
                    strcpy_s(s_txtPath + dirLen, sizeof(s_txtPath) - dirLen,
                             "todo.txt");
                }
            }
        }
    }
    if (s_txtPath[0] || s_iniPath[0]) LoadLocked();
    TodoStore_Unlock();
    return TRUE;
}

void TodoStore_Shutdown(void) {
    /* lock intentionally kept (process-lifetime); nothing to free */
}

static long long StampNow(void) {
    return (long long)time(NULL);
}

BOOL TodoStore_Add(const char *title, TodoImportance imp, const char *dueDate) {
    if (!title || !title[0]) return FALSE;
    char clean[TODO_STORE_TITLE_LEN];
    TodoNormalize_Copy(title, clean, sizeof(clean));
    title = clean;
    if (imp < TODO_IMPORTANCE_NONE || imp > TODO_IMPORTANCE_HIGH) return FALSE;
    if (!ValidDate(dueDate)) return FALSE;
    TodoStore_Lock();
    BOOL ok = FALSE;
    if (s_count < TODO_STORE_MAX_TASKS) {
        TodoTask t;
        memset(&t, 0, sizeof(t));
        t.source = TODO_SOURCE_LOCAL;
        MakeId(t.id, sizeof(t.id));
        strcpy_s(t.title, sizeof(t.title), title);
        t.importance = imp;
        t.done = FALSE;
        if (dueDate) strcpy_s(t.dueDate, sizeof(t.dueDate), dueDate);
        TodayStr(t.createdAt, sizeof(t.createdAt));
        t.updatedAt = StampNow();
        s_tasks[s_count++] = t;
        if (s_txtPath[0]) SaveLocked();
        ok = TRUE;
    }
    TodoStore_Unlock();
    return ok;
}

BOOL TodoStore_SetDone(const char *id, BOOL done) {
    if (!id || !id[0]) return FALSE;
    TodoStore_Lock();
    int i = TodoStore_FindIndex(id);
    BOOL ok = FALSE;
    if (i >= 0) {
        s_tasks[i].done = done;
        if (done) TodayStr(s_tasks[i].doneAt, sizeof(s_tasks[i].doneAt));
        else s_tasks[i].doneAt[0] = '\0';
        s_tasks[i].updatedAt = StampNow();
        if (s_txtPath[0]) SaveLocked();
        ok = TRUE;
    }
    TodoStore_Unlock();
    if (ok) TodoSync_PollNow();
    return ok;
}

void TodoStore_RecordDeleted(const char *id);

BOOL TodoStore_Remove(const char *id) {
    if (!id || !id[0]) return FALSE;
    TodoStore_Lock();
    int i = TodoStore_FindIndex(id);
    BOOL ok = FALSE;
    if (i >= 0) {
        TodoStore_RecordDeleted(s_tasks[i].id);
        for (int k = i; k + 1 < s_count; k++) s_tasks[k] = s_tasks[k + 1];
        s_count--;
        if (s_txtPath[0]) SaveLocked();
        ok = TRUE;
    }
    TodoStore_Unlock();
    return ok;
}

BOOL TodoStore_SetImportance(const char *id, TodoImportance imp) {
    if (!id || !id[0]) return FALSE;
    if (imp < TODO_IMPORTANCE_NONE || imp > TODO_IMPORTANCE_HIGH) return FALSE;
    TodoStore_Lock();
    int i = TodoStore_FindIndex(id);
    BOOL ok = FALSE;
    if (i >= 0) {
        s_tasks[i].importance = imp;
        s_tasks[i].updatedAt = StampNow();
        if (s_txtPath[0]) SaveLocked();
        ok = TRUE;
    }
    TodoStore_Unlock();
    return ok;
}

BOOL TodoStore_SetDueDate(const char *id, const char *dueDate) {
    if (!id || !id[0]) return FALSE;
    if (!ValidDate(dueDate)) return FALSE;
    TodoStore_Lock();
    int i = TodoStore_FindIndex(id);
    BOOL ok = FALSE;
    if (i >= 0) {
        if (dueDate) strcpy_s(s_tasks[i].dueDate, sizeof(s_tasks[i].dueDate), dueDate);
        else s_tasks[i].dueDate[0] = '\0';
        s_tasks[i].updatedAt = StampNow();
        if (s_txtPath[0]) SaveLocked();
        ok = TRUE;
    }
    TodoStore_Unlock();
    return ok;
}

int TodoStore_LocalCount(void) {
    TodoStore_Lock();
    int n = s_count;
    TodoStore_Unlock();
    return n;
}

void TodoStore_Reload(void) {
    TodoStore_Lock();
    if (s_txtPath[0] || s_iniPath[0]) LoadLocked();
    TodoStore_Unlock();
}

int TodoStore_SnapshotLocal(TodoTask *out, int outCap) {
    if (!out || outCap <= 0) return 0;
    TodoStore_Lock();
    int w = s_count < outCap ? s_count : outCap;
    for (int i = 0; i < w; i++) out[i] = s_tasks[i];
    TodoStore_Unlock();
    return w;
}

void TodoStore_Save(void) {
    SaveLocked();
}

int TodoStore_Count(void) {
    return s_count; /* caller holds lock */
}

TodoTask *TodoStore_RowAt(int index) {
    if (index < 0 || index >= s_count) return NULL;
    return &s_tasks[index]; /* caller holds lock */
}

const char *TodoStore_IniPath(void) {
    return s_iniPath;
}

const char *TodoStore_TxtPath(void) {
    return s_txtPath;
}
