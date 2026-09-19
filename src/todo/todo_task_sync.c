/**
 * @file todo_task_sync.c
 * @brief Entry-level sync payload codec (pull changed_since, push tasks).
 *
 * Reuses TodoSyncJson_ExtractStr/Int from the sync engine for parsing;
 * push body is built with manual JSON escaping (titles are UTF-8, quotes
 * and backslashes escaped; control chars stripped).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "todo_sync_internal.h"
#include "todo_task_sync.h"

static void JsonEscape(const char *src, char *dst, size_t cap) {
    if (!dst || cap == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    size_t w = 0;
    for (size_t i = 0; src[i] && w + 1 < cap; i++) {
        char c = src[i];
        if (c == '"' || c == '\\') {
            if (w + 2 >= cap) break;
            dst[w++] = '\\';
            dst[w++] = c;
        } else if ((unsigned char)c < 0x20) {
            if (w + 1 >= cap) break;
            dst[w++] = ' ';
        } else {
            dst[w++] = c;
        }
    }
    dst[w] = '\0';
}

/* Extract one flat object slice at key[idx] helpers. */
static int FindArrayObjects(const char *json, const char *key,
                            const char **arrStart) {
    char pat[64];
    _snprintf_s(pat, sizeof(pat), _TRUNCATE, "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) return 0;
    p = strchr(p, '[');
    if (!p) return 0;
    *arrStart = p + 1;
    return 1;
}

static int NextObject(const char **cursor, char *slice, size_t cap) {
    const char *p = *cursor;
    const char *end = strchr(p, ']');
    const char *o = strchr(p, '{');
    if (!o || (end && o > end)) return 0;
    const char *oe = strchr(o, '}');
    if (!oe) return 0;
    size_t len = (size_t)(oe - o + 1);
    if (len + 1 > cap) len = cap - 1;
    memcpy(slice, o, len);
    slice[len] = '\0';
    *cursor = oe + 1;
    return 1;
}

static void FillItem(const char *slice, TaskSyncItem *t) {
    memset(t, 0, sizeof(*t));
    /* server uuid always in "id"; Catime stable id in "client_id" */
    TodoSyncJson_ExtractStr(slice, "id", t->serverId, sizeof(t->serverId));
    TodoSyncJson_ExtractStr(slice, "client_id", t->clientId,
                            sizeof(t->clientId));
    TodoSyncJson_ExtractStr(slice, "title", t->title, sizeof(t->title));
    TodoSyncJson_ExtractStr(slice, "date", t->date, sizeof(t->date));
    TodoSyncJson_ExtractStr(slice, "due_date", t->dueDate, sizeof(t->dueDate));
    char status[16] = "";
    TodoSyncJson_ExtractStr(slice, "status", status, sizeof(status));
    t->done = strcmp(status, "done") == 0;
    char del[16] = "";
    TodoSyncJson_ExtractStr(slice, "deleted", del, sizeof(del));
    if (strcmp(del, "true") == 0) t->deleted = TRUE;
    long long pr = 0, up = 0;
    if (TodoSyncJson_ExtractInt(slice, "priority", &pr)) {
        if (pr < 0) pr = 0;
        if (pr > 3) pr = 3;
        t->importance = (TodoImportance)pr;
    }
    if (TodoSyncJson_ExtractInt(slice, "updated_at", &up)) t->updatedAt = up;
}

int TaskSync_ParsePull(const char *json, TaskSyncItem *out, int cap,
                       long long *serverTimeOut) {
    if (!json || !out || cap <= 0) return 0;
    if (serverTimeOut) {
        long long v = 0;
        if (TodoSyncJson_ExtractInt(json, "server_time", &v))
            *serverTimeOut = v;
    }
    const char *cur = NULL;
    if (!FindArrayObjects(json, "changed_since", &cur)) return 0;
    int n = 0;
    char slice[1024];
    while (n < cap && NextObject(&cur, slice, sizeof(slice))) {
        FillItem(slice, &out[n]);
        if (out[n].serverId[0]) n++;
    }
    return n;
}

void TaskSync_BuildPush(const TodoTask *tasks, int count,
                        const char deleted[][TODO_STORE_ID_LEN], int delCount,
                        char *dst, size_t cap) {
    if (!dst || cap == 0) return;
    dst[0] = '\0';
    size_t w = 0;
    w += _snprintf_s(dst + w, cap - w, _TRUNCATE, "{\"tasks\":[");
    for (int i = 0; i < count && tasks; i++) {
        const TodoTask *t = &tasks[i];
        char et[TODO_STORE_TITLE_LEN * 2], ed[32], edue[32];
        JsonEscape(t->title, et, sizeof(et));
        JsonEscape(t->createdAt, ed, sizeof(ed));
        JsonEscape(t->dueDate, edue, sizeof(edue));
        char esrv[TODO_STORE_UUID_LEN + 8];
        JsonEscape(t->serverId, esrv, sizeof(esrv));
        w += _snprintf_s(dst + w, cap - w, _TRUNCATE,
                         "%s{\"client_id\":\"%s\",\"server_id\":\"%s\","
                         "\"title\":\"%s\","
                         "\"date\":\"%s\",\"due_date\":\"%s\","
                         "\"priority\":%d,\"status\":\"%s\",\"updated_at\":%lld}",
                         i ? "," : "", t->id, esrv, et, ed, edue,
                         (int)t->importance, t->done ? "done" : "open",
                         t->updatedAt);
        if (w + 1 >= cap) break;
    }
    w += _snprintf_s(dst + w, cap - w, _TRUNCATE, "],\"deleted\":[");
    for (int i = 0; i < delCount && deleted; i++) {
        w += _snprintf_s(dst + w, cap - w, _TRUNCATE, "%s\"%s\"",
                         i ? "," : "", deleted[i]);
        if (w + 1 >= cap) break;
    }
    _snprintf_s(dst + w, cap - w, _TRUNCATE, "]}");
}

int TaskSync_ParsePushResp(const char *json, TaskSyncItem *conflicts, int cap) {
    if (!json || !conflicts || cap <= 0) return 0;
    const char *cur = NULL;
    if (!FindArrayObjects(json, "conflicts", &cur)) return 0;
    int n = 0;
    char slice[1024];
    while (n < cap && NextObject(&cur, slice, sizeof(slice))) {
        FillItem(slice, &conflicts[n]);
        if (conflicts[n].serverId[0] || conflicts[n].clientId[0]) n++;
    }
    return n;
}

int TaskSync_ParseCreated(const char *json, TaskSyncItem *out, int cap) {
    if (!json || !out || cap <= 0) return 0;
    const char *cur = NULL;
    if (!FindArrayObjects(json, "created", &cur)) return 0;
    int n = 0;
    char slice[512];
    while (n < cap && NextObject(&cur, slice, sizeof(slice))) {
        memset(&out[n], 0, sizeof(out[n]));
        TodoSyncJson_ExtractStr(slice, "client_id", out[n].clientId,
                                sizeof(out[n].clientId));
        TodoSyncJson_ExtractStr(slice, "server_id", out[n].serverId,
                                sizeof(out[n].serverId));
        if (out[n].clientId[0] && out[n].serverId[0]) n++;
    }
    return n;
}
