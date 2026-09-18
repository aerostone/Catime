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

#include "todo_store.h"
#include "todo_sync.h"

static CRITICAL_SECTION s_lock;
static BOOL s_lockInit = FALSE;
static TodoTask s_tasks[TODO_STORE_MAX_TASKS];
static int s_count = 0;
static char s_iniPath[MAX_PATH] = "";
static long s_nextId = 1;

static void Lock(void) { EnterCriticalSection(&s_lock); }
static void Unlock(void) { LeaveCriticalSection(&s_lock); }

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

static int FindIndex(const char *id) {
    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_tasks[i].id, id) == 0) return i;
    }
    return -1;
}

static void SaveLocked(void) {
    char buf[32];
    _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%d", s_count);
    WritePrivateProfileStringA("Todo", "Count", buf, s_iniPath);
    _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%ld", s_nextId);
    WritePrivateProfileStringA("Todo", "NextId", buf, s_iniPath);
    for (int i = 0; i < s_count; i++) {
        char sec[32], ib[16], db[16];
        _snprintf_s(sec, sizeof(sec), _TRUNCATE, "Task%d", i);
        TodoTask *t = &s_tasks[i];
        WritePrivateProfileStringA(sec, "Id", t->id, s_iniPath);
        WritePrivateProfileStringA(sec, "Title", t->title, s_iniPath);
        _snprintf_s(ib, sizeof(ib), _TRUNCATE, "%d", (int)t->importance);
        WritePrivateProfileStringA(sec, "Importance", ib, s_iniPath);
        _snprintf_s(db, sizeof(db), _TRUNCATE, "%d", t->done ? 1 : 0);
        WritePrivateProfileStringA(sec, "Done", db, s_iniPath);
        WritePrivateProfileStringA(sec, "Due", t->dueDate, s_iniPath);
        WritePrivateProfileStringA(sec, "DoneAt", t->doneAt, s_iniPath);
        WritePrivateProfileStringA(sec, "Created", t->createdAt, s_iniPath);
    }
}

static void LoadLocked(void) {
    s_count = 0;
    s_nextId = GetPrivateProfileIntA("Todo", "NextId", 1, s_iniPath);
    if (s_nextId < 1) s_nextId = 1;
    int n = GetPrivateProfileIntA("Todo", "Count", 0, s_iniPath);
    if (n < 0) n = 0;
    if (n > TODO_STORE_MAX_TASKS) n = TODO_STORE_MAX_TASKS;
    for (int i = 0; i < n; i++) {
        char sec[32];
        _snprintf_s(sec, sizeof(sec), _TRUNCATE, "Task%d", i);
        TodoTask t;
        memset(&t, 0, sizeof(t));
        t.source = TODO_SOURCE_LOCAL;
        GetPrivateProfileStringA(sec, "Id", "", t.id, sizeof(t.id), s_iniPath);
        GetPrivateProfileStringA(sec, "Title", "", t.title, sizeof(t.title), s_iniPath);
        if (!t.id[0] || !t.title[0]) continue;
        t.importance = (TodoImportance)GetPrivateProfileIntA(sec, "Importance", 0, s_iniPath);
        if (t.importance < TODO_IMPORTANCE_NONE) t.importance = TODO_IMPORTANCE_NONE;
        if (t.importance > TODO_IMPORTANCE_HIGH) t.importance = TODO_IMPORTANCE_HIGH;
        t.done = GetPrivateProfileIntA(sec, "Done", 0, s_iniPath) != 0;
        GetPrivateProfileStringA(sec, "Due", "", t.dueDate, sizeof(t.dueDate), s_iniPath);
        GetPrivateProfileStringA(sec, "DoneAt", "", t.doneAt, sizeof(t.doneAt), s_iniPath);
        GetPrivateProfileStringA(sec, "Created", "", t.createdAt, sizeof(t.createdAt), s_iniPath);
        if (!ValidDate(t.dueDate)) t.dueDate[0] = '\0';
        if (!ValidDate(t.doneAt)) t.doneAt[0] = '\0';
        s_tasks[s_count++] = t;
    }
}

BOOL TodoStore_Init(const wchar_t *configIniPath) {
    if (!s_lockInit) {
        InitializeCriticalSection(&s_lock);
        s_lockInit = TRUE;
    }
    Lock();
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
            }
        }
    }
    if (s_iniPath[0]) LoadLocked();
    Unlock();
    return TRUE;
}

void TodoStore_Shutdown(void) {
    /* lock intentionally kept (process-lifetime); nothing to free */
}

BOOL TodoStore_Add(const char *title, TodoImportance imp, const char *dueDate) {
    if (!title || !title[0]) return FALSE;
    if (imp < TODO_IMPORTANCE_NONE || imp > TODO_IMPORTANCE_HIGH) return FALSE;
    if (!ValidDate(dueDate)) return FALSE;
    Lock();
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
        s_tasks[s_count++] = t;
        if (s_iniPath[0]) SaveLocked();
        ok = TRUE;
    }
    Unlock();
    return ok;
}

BOOL TodoStore_SetDone(const char *id, BOOL done) {
    if (!id || !id[0]) return FALSE;
    Lock();
    int i = FindIndex(id);
    BOOL ok = FALSE;
    if (i >= 0) {
        s_tasks[i].done = done;
        if (done) TodayStr(s_tasks[i].doneAt, sizeof(s_tasks[i].doneAt));
        else s_tasks[i].doneAt[0] = '\0';
        if (s_iniPath[0]) SaveLocked();
        ok = TRUE;
    }
    Unlock();
    if (ok) TodoSync_PollNow();
    return ok;
}

BOOL TodoStore_Remove(const char *id) {
    if (!id || !id[0]) return FALSE;
    Lock();
    int i = FindIndex(id);
    BOOL ok = FALSE;
    if (i >= 0) {
        for (int k = i; k + 1 < s_count; k++) s_tasks[k] = s_tasks[k + 1];
        s_count--;
        if (s_iniPath[0]) SaveLocked();
        ok = TRUE;
    }
    Unlock();
    return ok;
}

BOOL TodoStore_SetImportance(const char *id, TodoImportance imp) {
    if (!id || !id[0]) return FALSE;
    if (imp < TODO_IMPORTANCE_NONE || imp > TODO_IMPORTANCE_HIGH) return FALSE;
    Lock();
    int i = FindIndex(id);
    BOOL ok = FALSE;
    if (i >= 0) {
        s_tasks[i].importance = imp;
        if (s_iniPath[0]) SaveLocked();
        ok = TRUE;
    }
    Unlock();
    return ok;
}

BOOL TodoStore_SetDueDate(const char *id, const char *dueDate) {
    if (!id || !id[0]) return FALSE;
    if (!ValidDate(dueDate)) return FALSE;
    Lock();
    int i = FindIndex(id);
    BOOL ok = FALSE;
    if (i >= 0) {
        if (dueDate) strcpy_s(s_tasks[i].dueDate, sizeof(s_tasks[i].dueDate), dueDate);
        else s_tasks[i].dueDate[0] = '\0';
        if (s_iniPath[0]) SaveLocked();
        ok = TRUE;
    }
    Unlock();
    return ok;
}

int TodoStore_LocalCount(void) {
    Lock();
    int n = s_count;
    Unlock();
    return n;
}

void TodoStore_Reload(void) {
    Lock();
    if (s_iniPath[0]) LoadLocked();
    Unlock();
}

int TodoStore_SnapshotLocal(TodoTask *out, int outCap) {
    if (!out || outCap <= 0) return 0;
    Lock();
    int w = s_count < outCap ? s_count : outCap;
    for (int i = 0; i < w; i++) out[i] = s_tasks[i];
    Unlock();
    return w;
}

const char *TodoStore_IniPath(void) {
    return s_iniPath;
}
