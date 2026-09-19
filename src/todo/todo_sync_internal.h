/**
 * @file todo_sync_internal.h
 * @brief Shared state and helpers for the TODO sync engine split modules.
 *
 * Internal header (not installed). Included by todo_sync.c,
 * todo_sync_json.c and todo_sync_http.c.
 */

#ifndef CATIME_TODO_SYNC_INTERNAL_H
#define CATIME_TODO_SYNC_INTERNAL_H

#include <windows.h>
#include <wininet.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "todo_sync.h"

#define TODO_MAX_TASKS 20
#define TODO_TITLE_LEN 128
#define TODO_ID_LEN 64
#define TODO_URL_LEN 512
#define TODO_TOKEN_LEN 128
#define TODO_POLL_DEFAULT_S 60
#define TODO_HTTP_TIMEOUT_MS 10000
#define TODO_RESP_MAX (256u * 1024u)

typedef struct {
    char id[TODO_ID_LEN];
    char title[TODO_TITLE_LEN];
    char status[16];
} TodoItem;

typedef struct {
    TodoItem today[TODO_MAX_TASKS];
    int todayCount;
    TodoItem overdue[TODO_MAX_TASKS];
    int overdueCount;
    TodoItem someday[TODO_MAX_TASKS];
    int somedayCount;
    int pomoMin;
    long long serverTime;
} TodoCache;

/* Shared state owned by todo_sync.c */
extern CRITICAL_SECTION g_todoSyncLock;
extern BOOL g_todoSyncLockInit;
extern TodoCache g_todoSyncCache;
extern HWND g_todoSyncHwnd;
extern wchar_t g_todoSyncIniPath[MAX_PATH];
extern char g_todoSyncServer[TODO_URL_LEN];
extern char g_todoSyncToken[TODO_TOKEN_LEN];
extern BOOL g_todoSyncEnabled;
extern int g_todoSyncPollSec;
extern HANDLE g_todoSyncThread;
extern volatile LONG g_todoSyncRunning;
extern volatile LONG g_todoSyncPollNow;

/* todo_sync_json.c */
BOOL TodoSyncJson_ExtractStr(const char *obj, const char *key, char *out, size_t outSize);
BOOL TodoSyncJson_ExtractInt(const char *json, const char *key, long long *out);
int TodoSyncJson_ParseItemArray(const char *json, const char *key,
                                TodoItem *items, int max);
BOOL TodoSyncJson_LooksLocked(const char *title);

/* todo_sync_http.c */
char *TodoSyncHttp_Get(const char *url, const char *bearer);
BOOL TodoSyncHttp_Post(const char *url, const char *bearer, const char *bodyJson);
BOOL TodoSyncHttp_Patch(const char *url, const char *bearer, const char *bodyJson);
char *TodoSyncHttp_PostResp(const char *url, const char *bearer,
                            const char *bodyJson);

#endif /* CATIME_TODO_SYNC_INTERNAL_H */
