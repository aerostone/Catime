/**
 * @file todo_sync_merge.c
 * @brief Bidirectional merge driver: pull changed_since + push sync rows.
 *
 * Called from the sync poll thread (todo_sync.c DoPoll tail):
 * - Pull GET /api/catime/sync?updated_after=<cursor>: server-newer rows
 *   overwrite local by external_id; local-newer rows are left alone (they
 *   will be pushed). Server tombstones delete local rows. Local losers are
 *   appended to todo.txt.conflict-<ts> before overwrite (never silent).
 *   updated_after=0 is a full backlog sync (cursor paging until caught up).
 * - Push POST /api/catime/push with the rows that live on the sync book
 *   (source == TODO_SOURCE_SYNC) plus deleted ids; rows on local books are
 *   never uploaded. Guarded by a payload compare, not todo.txt mtime, so
 *   the pull's own write cannot bounce rows back.
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

/* Push request body + its last-successful copy. File-scope because 256 KiB
 * on the worker stack is not affordable; TodoSyncMerge_Run is only ever
 * called from the single poll thread, so this is not reentrant-safe by
 * design. */
static char s_pushBody[TODO_RESP_MAX];
static char s_lastPushed[TODO_RESP_MAX];

/* ---- file helpers (UTF-8 paths, ANSI Win32) ---- */
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
static void ApplyPulledEx(const TaskSyncItem *srv, const char *calFilter); /* fwd */

static void ApplyPulled(const TaskSyncItem *srv) {
    ApplyPulledEx(srv, NULL);
}

static void ApplyPulledEx(const TaskSyncItem *srv, const char *calFilter) {
    /* #28b: only rows from the selected tweek calendar land on the board.
     * Empty filter = legacy all-calendars behavior (no setting yet). */
    if (calFilter && calFilter[0] && srv->calendarId[0] &&
        strcmp(srv->calendarId, calFilter) != 0)
        return;
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
    /* selected sync book (tweek calendar id); "" = all calendars (#28b) */
    char calFilter[TODO_STORE_UUID_LEN] = "";
    TodoSync_GetCalendarId(calFilter, sizeof(calFilter));

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
        for (int i = 0; i < n; i++) ApplyPulledEx(&items[i], calFilter);
        if (srvTime > cursor) WriteCursor(srvTime);
        free(resp);
    }

    /* --- push: only when the sync-row payload actually changed ---
     * Gating on todo.txt mtime is wrong here: the pull above rewrites the
     * file, so every cycle would push back rows the server just sent
     * (equal updated_at -> conflicts[] spam). Compare payloads instead. */
    TodoTask tasks[TODO_STORE_MAX_TASKS];
    int n = TodoStore_SnapshotSync(tasks, TODO_STORE_MAX_TASKS);
    char deleted[64][TODO_STORE_ID_LEN];
    int dn = TodoStore_DeletedIds(deleted, 64);
    if (n <= 0 && dn <= 0) return;
    char *body = s_pushBody;
    TaskSync_BuildPushEx(tasks, n, dn ? deleted : NULL, dn, calFilter,
                         body, sizeof(s_pushBody));
    if (strcmp(body, s_lastPushed) == 0) return;
    char purl[TODO_URL_LEN + 32];
    _snprintf_s(purl, sizeof(purl), _TRUNCATE, "%s/api/catime/push", server);
    char *presp = TodoSyncHttp_PostResp(purl, token, body);
    if (presp) {
        TaskSyncItem conf[64];
        int cn = TaskSync_ParsePushResp(presp, conf, 64);
        for (int i = 0; i < cn; i++) {
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
            ApplyPulledEx(&conf[i], calFilter);
        }
        /* Adopt the server's authoritative stamp and id for every row the
         * push touched: keeps updated_at in the server clock domain, so a
         * skewed local clock neither wins forever nor hides remote edits. */
        TaskSyncItem ap[64];
        int an = TaskSync_ParseApplied(presp, ap, 64);
        for (int i = 0; i < an; i++) {
            TodoTask cur;
            memset(&cur, 0, sizeof(cur));
            if (!TodoStore_FindById(ap[i].clientId, &cur)) continue;
            TodoStore_UpdateFromSyncStamp(ap[i].clientId, NULL, NULL,
                                          cur.importance, cur.done,
                                          ap[i].updatedAt, ap[i].serverId);
        }
        free(presp);
        if (dn > 0) TodoStore_ClearDeleted();
        strcpy_s(s_lastPushed, sizeof(s_lastPushed), s_pushBody);
    }
}
