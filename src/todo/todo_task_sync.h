/**
 * @file todo_task_sync.h
 * @brief Entry-level bidirectional sync: TodoTask[] <-> tweek task API.
 *
 * Pull: GET /api/catime/sync?updated_after=<cursor> -> changed_since[]
 *   merged into local store by id (server newer wins; local loser is NOT
 *   dropped silently -- caller writes it to a conflict file first).
 * Push: POST /api/catime/push {tasks, deleted} with client stable ids for
 * the rows on the sync book; the poll timer drives both directions, and the
 * caller gates the push on the payload changing (not on todo.txt mtime).
 */
#ifndef CATIME_TODO_TASK_SYNC_H
#define CATIME_TODO_TASK_SYNC_H

#include <windows.h>

#include "todo_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One merged server task (subset of tweek Task wire model). */
typedef struct {
    char clientId[TODO_STORE_ID_LEN]; /* external_id on server (may be "") */
    char serverId[TODO_STORE_UUID_LEN]; /* tweek tasks.id (always present) */
    char title[TODO_STORE_TITLE_LEN];
    char date[TODO_STORE_DATE_LEN];     /* calendar day */
    char dueDate[TODO_STORE_DATE_LEN];  /* from due_at */
    TodoImportance importance;          /* from priority */
    BOOL done;                          /* status == done */
    BOOL deleted;                       /* tombstone */
    long long updatedAt;
    char calendarId[TODO_STORE_UUID_LEN]; /* tweek calendar_id (#28a/28b) */
} TaskSyncItem;

/* Parse pull response body. changed_since[] -> items. cursor -> server_time.
 * Returns item count (tombstones included). */
int TaskSync_ParsePull(const char *json, TaskSyncItem *out, int cap,
                       long long *serverTimeOut);


/* #28b: parse top-level "calendars":[{id,name,color}] for the book picker. */
typedef struct {
    char id[TODO_STORE_UUID_LEN];
    char name[TODO_STORE_BOARD_LEN];
} TaskSyncCal;
int TaskSync_ParseCalendars(const char *json, TaskSyncCal *out, int cap);

/* Build push request body from local tasks + deleted ids.
 * calendarId (tweek calendar uuid, may be NULL) tags the batch so the
 * server files rows into the selected book. */
void TaskSync_BuildPushEx(const TodoTask *tasks, int count,
                          const char deleted[][TODO_STORE_ID_LEN], int delCount,
                          const char *calendarId, char *dst, size_t cap);
void TaskSync_BuildPush(const TodoTask *tasks, int count,
                        const char deleted[][TODO_STORE_ID_LEN], int delCount,
                        char *dst, size_t cap);
/* Parse push response: conflicts[] items (server-won losers, caller writes
 * a conflict file) + applied_rows[] server stamps for every row touched
 * (created/updated/equal), used to adopt the server clock domain. */
int TaskSync_ParsePushResp(const char *json, TaskSyncItem *conflicts, int cap);
int TaskSync_ParseApplied(const char *json, TaskSyncItem *out, int cap);


#ifdef __cplusplus
}
#endif

#endif /* CATIME_TODO_TASK_SYNC_H */
