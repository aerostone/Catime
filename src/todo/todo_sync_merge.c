/**
 * @file todo_sync_merge.c
 * @brief Bidirectional merge driver: pull changed_since + push local dirty.
 *
 * Called from the sync poll thread (todo_sync.c DoPoll tail):
 * - Pull GET /api/catime/sync?updated_after=<cursor>: server-newer rows
 *   overwrite local by external_id; local-newer rows are left alone (they
 *   will be pushed). Server tombstones delete local rows. Local losers are
 *   appended to todo.txt.conflict-<ts> before overwrite (never silent).
 * - Push POST /api/catime/push when todo.txt mtime moved since last push.
 * Cursor (server_time) persists in todo.cursor beside todo.txt.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "todo_normalize.h"
#include "todo_store.h"
#include "todo_sync.h"
#include "todo_sync_internal.h"
#include "todo_task_sync.h"
#include "todo_txt.h"

/* ---- file helpers (UTF-8 paths, ANSI Win32) ---- */
static BOOL FileMtime(const char *path, long long *out) {
    if (!path || !path[0]) return FALSE;
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    FILETIME ft;
    BOOL ok = GetFileTime(h, NULL, NULL, &ft);
    CloseHandle(h);
    if (!ok) return FALSE;
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    if (out) *out = (long long)u.QuadPart;
    return TRUE;
}

static void CursorPath(char *out, size_t cap) {
    const char *txt = TodoStore_TxtPath();
    if (!txt || !txt[0]) {
        if (cap) out[0] = '\0';
        return;
    }
    _snprintf_s(out, cap, _TRUNCATE, "%s.cursor", txt);
}

static long long ReadCursor(void) {
    char p[MAX_PATH];
    CursorPath(p, sizeof(p));
    if (!p[0]) return 0;
    FILE *fp = NULL;
    if (fopen_s(&fp, p, "r") != 0 || !fp) return 0;
    long long v = 0;
    fscanf_s(fp, "%lld", &v);
    fclose(fp);
    return v < 0 ? 0 : v;
}

static void WriteCursor(long long v) {
    char p[MAX_PATH];
    CursorPath(p, sizeof(p));
    if (!p[0]) return;
    FILE *fp = NULL;
    if (fopen_s(&fp, p, "w") != 0 || !fp) return;
    fprintf(fp, "%lld\n", v);
    fclose(fp);
}

static void AppendConflict(const TaskSyncItem *srv) {
    const char *txt = TodoStore_TxtPath();
    if (!txt || !txt[0] || !srv) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    char p[MAX_PATH];
    _snprintf_s(p, sizeof(p), _TRUNCATE,
                "%s.conflict-%04d%02d%02d-%02d%02d%02d", txt, (int)st.wYear,
                (int)st.wMonth, (int)st.wDay, (int)st.wHour, (int)st.wMinute,
                (int)st.wSecond);
    FILE *fp = NULL;
    if (fopen_s(&fp, p, "a") != 0 || !fp) return;
    TodoTask t;
    memset(&t, 0, sizeof(t));
    t.source = TODO_SOURCE_LOCAL;
    strcpy_s(t.id, sizeof(t.id), srv->clientId);
    TodoNormalize_Copy(srv->title, t.title, sizeof(t.title));
    t.importance = srv->importance;
    t.done = srv->done;
    if (srv->dueDate[0]) strcpy_s(t.dueDate, sizeof(t.dueDate), srv->dueDate);
    else if (srv->date[0]) strcpy_s(t.dueDate, sizeof(t.dueDate), srv->date);
    char line[512];
    TodoTxt_FormatLine(&t, line, sizeof(line));
    fputs("# server-won conflict snapshot\n", fp);
    fputs(line, fp);
    fputc('\n', fp);
    fclose(fp);
}

/* Local id for a pulled row: client_id when known, else C:<uuid>. */
static void PulledLocalId(const TaskSyncItem *srv, char *out, size_t cap) {
    if (srv->clientId[0]) {
        strcpy_s(out, cap, srv->clientId);
    } else {
        _snprintf_s(out, cap, _TRUNCATE, "C:%s", srv->serverId);
    }
}

/* Apply one pulled server row to the local store (id-keyed, never row pos).
 * - tombstone: delete local row by id (RecordDeleted suppressed: row came
 *   from server, re-push of its id is skipped via serverId match below).
 * - existing: server-newer (updated_at) wins; loser snapshotted to conflict.
 * - missing: insert with server stamp + serverId (no ping-pong). */
static void ApplyPulled(const TaskSyncItem *srv) {
    char lid[TODO_STORE_ID_LEN] = "";
    PulledLocalId(srv, lid, sizeof(lid));
    if (!lid[0]) return;
    const char *due = srv->dueDate[0] ? srv->dueDate : srv->date;
    if (srv->deleted) {
        TodoTask cur;
        memset(&cur, 0, sizeof(cur));
        if (TodoStore_FindById(lid, &cur)) {
            /* server tombstone wins unconditionally: drop local row.
             * ClearDeleted-sidecar note: Remove() would record the id;
             * strip it right after so we never push back a server delete. */
            TodoStore_Remove(lid);
            TodoStore_StripDeleted(lid);
        }
        return;
    }
    if (!srv->title[0]) return;
    TodoTask cur;
    memset(&cur, 0, sizeof(cur));
    if (TodoStore_FindById(lid, &cur)) {
        if (srv->updatedAt <= cur.updatedAt) return; /* local newer: keep */
        BOOL same = strcmp(cur.title, srv->title) == 0 &&
                    cur.done == srv->done &&
                    strcmp(cur.dueDate, due) == 0 &&
                    (int)cur.importance == (int)srv->importance;
        if (same) {
            /* content same, adopt stamp+serverId only */
            TodoStore_UpdateFromSyncStamp(lid, NULL, NULL, cur.importance,
                                          cur.done, srv->updatedAt,
                                          srv->serverId);
            return;
        }
        TaskSyncItem loser;
        memset(&loser, 0, sizeof(loser));
        strcpy_s(loser.clientId, sizeof(loser.clientId), cur.id);
        strcpy_s(loser.title, sizeof(loser.title), cur.title);
        strcpy_s(loser.date, sizeof(loser.date), cur.dueDate);
        loser.importance = cur.importance;
        loser.done = cur.done;
        AppendConflict(&loser);
        TodoStore_UpdateFromSyncStamp(lid, srv->title, due, srv->importance,
                                      srv->done, srv->updatedAt,
                                      srv->serverId);
    } else {
        TodoStore_InsertSyncedStamp(lid, srv->title, due, srv->importance,
                                    srv->done, srv->updatedAt, srv->serverId);
    }
}

/* Entry called from DoPoll with server+token snapshot. */
void TodoSyncMerge_Run(const char *server, const char *token) {
    if (!server || !server[0] || !token || !token[0]) return;
    const char *txt = TodoStore_TxtPath();
    if (!txt || !txt[0]) return;

    /* --- pull --- */
    long long cursor = ReadCursor();
    char url[TODO_URL_LEN + 64];
    _snprintf_s(url, sizeof(url), _TRUNCATE,
                "%s/api/catime/sync?updated_after=%lld&top=50", server, cursor);
    char *resp = TodoSyncHttp_Get(url, token);
    if (resp) {
        TaskSyncItem items[200];
        long long srvTime = 0;
        int n = TaskSync_ParsePull(resp, items, 200, &srvTime);
        for (int i = 0; i < n; i++) ApplyPulled(&items[i]);
        if (srvTime > cursor) WriteCursor(srvTime);
        free(resp);
    }

    /* --- push: only when txt changed since last push --- */
    static long long s_lastPushMtime = 0;
    long long mt = 0;
    if (!FileMtime(txt, &mt)) return;
    if (mt == s_lastPushMtime) return;
    TodoTask tasks[TODO_STORE_MAX_TASKS];
    int n = TodoStore_SnapshotLocal(tasks, TODO_STORE_MAX_TASKS);
    char deleted[64][TODO_STORE_ID_LEN];
    int dn = TodoStore_DeletedIds(deleted, 64);
    char body[TODO_RESP_MAX];
    TaskSync_BuildPush(tasks, n, dn ? deleted : NULL, dn, body, sizeof(body));
    char purl[TODO_URL_LEN + 32];
    _snprintf_s(purl, sizeof(purl), _TRUNCATE, "%s/api/catime/push", server);
    char *presp = TodoSyncHttp_PostResp(purl, token, body);
    if (presp) {
        TaskSyncItem conf[64];
        int cn = TaskSync_ParsePushResp(presp, conf, 64);
        for (int i = 0; i < cn; i++) {
            /* server won: snapshot local loser, then take server row */
            char lid[TODO_STORE_ID_LEN] = "";
            PulledLocalId(&conf[i], lid, sizeof(lid));
            TodoTask cur;
            memset(&cur, 0, sizeof(cur));
            if (lid[0] && TodoStore_FindById(lid, &cur)) {
                TaskSyncItem loser;
                memset(&loser, 0, sizeof(loser));
                strcpy_s(loser.clientId, sizeof(loser.clientId), cur.id);
                strcpy_s(loser.title, sizeof(loser.title), cur.title);
                strcpy_s(loser.date, sizeof(loser.date), cur.dueDate);
                loser.importance = cur.importance;
                loser.done = cur.done;
                AppendConflict(&loser);
            }
            ApplyPulled(&conf[i]);
        }
        /* created[] mappings: adopt serverId locally (closes the loop) */
        TaskSyncItem mk[64];
        int mn = TaskSync_ParseCreated(presp, mk, 64);
        for (int i = 0; i < mn; i++) {
            TodoTask cur;
            memset(&cur, 0, sizeof(cur));
            if (TodoStore_FindById(mk[i].clientId, &cur) &&
                !cur.serverId[0]) {
                TodoStore_UpdateFromSyncStamp(mk[i].clientId, NULL, NULL,
                                              cur.importance, cur.done, 0,
                                              mk[i].serverId);
            }
        }
        free(presp);
        if (dn > 0) TodoStore_ClearDeleted();
        FileMtime(txt, &s_lastPushMtime);
    }
}
