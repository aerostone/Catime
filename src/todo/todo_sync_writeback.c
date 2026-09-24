/**
 * @file todo_sync_writeback.c
 * @brief One-way write-backs kept after the push-chain removal (#28c).
 *
 * Pomodoro completion and done-marking are single REST calls, not a
 * bulk push: the next pull (newer-wins) converges the local row.
 */
#include "todo_sync_internal.h"
#include "todo_store.h"

BOOL TodoSync_OnPomodoroComplete(const char *taskId, int minutes) {
    char server[TODO_URL_LEN], token[TODO_TOKEN_LEN];
    char tid[TODO_ID_LEN] = "";
    /* local stable id -> server uuid via store serverId; C:uuid strips prefix */
    if (taskId && *taskId) {
        if (taskId[0] == 'C' && taskId[1] == ':') {
            strcpy_s(tid, sizeof(tid), taskId + 2);
        } else {
            TodoTask lt;
            memset(&lt, 0, sizeof(lt));
            if (TodoStore_FindById(taskId, &lt) && lt.serverId[0])
                strcpy_s(tid, sizeof(tid), lt.serverId);
            else
                strcpy_s(tid, sizeof(tid), taskId);
        }
    }
    EnterCriticalSection(&g_todoSyncLock);
    strcpy_s(server, sizeof(server), g_todoSyncServer);
    strcpy_s(token, sizeof(token), g_todoSyncToken);
    BOOL en = g_todoSyncEnabled;
    if (!tid[0] && g_todoSyncCache.todayCount > 0)
        strcpy_s(tid, sizeof(tid), g_todoSyncCache.today[0].id);
    LeaveCriticalSection(&g_todoSyncLock);
    if (!en || !server[0] || !token[0]) return FALSE;
    char url[TODO_URL_LEN + 32], body[256];
    _snprintf_s(url, sizeof(url), _TRUNCATE, "%s/api/pomodoro", server);
    _snprintf_s(body, sizeof(body), _TRUNCATE,
                "{\"task_id\":\"%s\",\"minutes\":%d,\"origin\":\"catime\"}", tid, minutes);
    BOOL ok = TodoSyncHttp_Post(url, token, body);
    if (ok) TodoSync_PollNow();
    return ok;
}

BOOL TodoSync_MarkDone(const char *taskId) {
    char server[TODO_URL_LEN], token[TODO_TOKEN_LEN];
    EnterCriticalSection(&g_todoSyncLock);
    strcpy_s(server, sizeof(server), g_todoSyncServer);
    strcpy_s(token, sizeof(token), g_todoSyncToken);
    BOOL en = g_todoSyncEnabled;
    LeaveCriticalSection(&g_todoSyncLock);
    if (!en || !server[0] || !token[0] || !taskId || !*taskId) return FALSE;
    char url[TODO_URL_LEN + 128];
    _snprintf_s(url, sizeof(url), _TRUNCATE, "%s/api/tasks/%s", server, taskId);
    BOOL ok = TodoSyncHttp_Patch(url, token, "{\"status\":\"done\"}");
    if (ok) TodoSync_PollNow();
    return ok;
}

