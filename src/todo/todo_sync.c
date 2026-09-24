/**
 * @file todo_sync.c
 * @brief Catime TODO overlay sync engine (WinINet, no third-party deps).
 *
 * Lifecycle, config, background poll thread and public API live here.
 * JSON extraction lives in todo_sync_json.c, HTTP transport in
 * todo_sync_http.c (shared state via todo_sync_internal.h).
 */
#include "todo_sync_internal.h"
#include "todo_sync_status.h"
#include "todo_types.h"
#include "todo_store.h"
#include "config/config_ini_api.h"

CRITICAL_SECTION g_todoSyncLock;
BOOL g_todoSyncLockInit = FALSE;
TodoCache g_todoSyncCache;
HWND g_todoSyncHwnd = NULL;

/* Main-window handle for dialogs/child windows owned by TODO features. */
HWND TodoSync_MainHwnd(void) { return g_todoSyncHwnd; }
wchar_t g_todoSyncIniPath[MAX_PATH] = L"";
char g_todoSyncServer[TODO_URL_LEN] = "";
char g_todoSyncToken[TODO_TOKEN_LEN] = "";
char g_todoSyncCalendarId[64] = ""; /* #28b: which tweek calendar to pull */
BOOL g_todoSyncEnabled = FALSE;
int g_todoSyncPollSec = TODO_POLL_DEFAULT_S;
HANDLE g_todoSyncThread = NULL;
volatile LONG g_todoSyncRunning = 0;
volatile LONG g_todoSyncPollNow = 0;

/* ---------- config ---------- */
static void LoadConfigW(void) {
    wchar_t server[TODO_URL_LEN];
    wchar_t token[TODO_TOKEN_LEN];
    GetPrivateProfileStringW(L"Sync", L"ServerUrl", L"", server, _countof(server), g_todoSyncIniPath);
    GetPrivateProfileStringW(L"Sync", L"Token", L"", token, _countof(token), g_todoSyncIniPath);
    g_todoSyncEnabled = GetPrivateProfileIntW(L"Sync", L"Enabled", 0, g_todoSyncIniPath) != 0;
    {
        wchar_t wcal[64];
        GetPrivateProfileStringW(L"Sync", L"CalendarId", L"", wcal, _countof(wcal), g_todoSyncIniPath);
        if (!WideCharToMultiByte(CP_UTF8, 0, wcal, -1, g_todoSyncCalendarId, sizeof(g_todoSyncCalendarId), NULL, NULL))
            g_todoSyncCalendarId[0] = '\0';
    }
    g_todoSyncPollSec = (int)GetPrivateProfileIntW(L"Sync", L"PollInterval", TODO_POLL_DEFAULT_S, g_todoSyncIniPath);
    if (g_todoSyncPollSec < 15) g_todoSyncPollSec = 15;
    if (g_todoSyncPollSec > 600) g_todoSyncPollSec = 600;
    if (!WideCharToMultiByte(CP_UTF8, 0, server, -1, g_todoSyncServer, sizeof(g_todoSyncServer), NULL, NULL))
        g_todoSyncServer[0] = '\0';
    if (!WideCharToMultiByte(CP_UTF8, 0, token, -1, g_todoSyncToken, sizeof(g_todoSyncToken), NULL, NULL))
        g_todoSyncToken[0] = '\0';
    /* trim trailing '/' */
    size_t n = strlen(g_todoSyncServer);
    while (n > 0 && g_todoSyncServer[n - 1] == '/') g_todoSyncServer[--n] = '\0';
}

/* ---------- poll ---------- */
static void DoPoll(void) {
    char server[TODO_URL_LEN], token[TODO_TOKEN_LEN];
    EnterCriticalSection(&g_todoSyncLock);
    strcpy_s(server, sizeof(server), g_todoSyncServer);
    strcpy_s(token, sizeof(token), g_todoSyncToken);
    BOOL en = g_todoSyncEnabled;
    LeaveCriticalSection(&g_todoSyncLock);
    if (!en || !server[0] || !token[0]) return;

    char url[TODO_URL_LEN + 64];
    _snprintf_s(url, sizeof(url), _TRUNCATE, "%s/api/catime/sync?top=%d", server, TODO_MAX_TASKS);
    char *resp = TodoSyncHttp_Get(url, token);
    if (!resp) {
        TodoSyncStatus_Report(TODO_SYNC_STATE_OFFLINE, 0);
        return; /* offline: keep old cache */
    }
    TodoCache nc;
    memset(&nc, 0, sizeof(nc));
    nc.todayCount = TodoSyncJson_ParseItemArray(resp, "today_open", nc.today, TODO_MAX_TASKS);
    nc.overdueCount = TodoSyncJson_ParseItemArray(resp, "overdue_open", nc.overdue, TODO_MAX_TASKS);
    nc.somedayCount = TodoSyncJson_ParseItemArray(resp, "someday_open", nc.someday, TODO_MAX_TASKS);
    long long v = 0;
    if (TodoSyncJson_ExtractInt(resp, "pomodoros_today_min", &v)) nc.pomoMin = (int)v;
    if (TodoSyncJson_ExtractInt(resp, "server_time", &v)) nc.serverTime = v;
    free(resp);
    EnterCriticalSection(&g_todoSyncLock);
    g_todoSyncCache = nc;
    LeaveCriticalSection(&g_todoSyncLock);
    {
        TodoSyncStatus_Report(TODO_SYNC_STATE_OK, 200);
    }
    if (g_todoSyncHwnd) InvalidateRect(g_todoSyncHwnd, NULL, TRUE);
    /* entry-level bidirectional merge (pull changed_since + push dirty) */
    TodoSyncMerge_Run(server, token);
}

static unsigned __stdcall PollThread(void *arg) {
    (void)arg;
    int waited = 0;
    while (InterlockedCompareExchange(&g_todoSyncRunning, 1, 1)) {
        if (InterlockedExchange(&g_todoSyncPollNow, 0)) {
            DoPoll();
            waited = 0;
        }
        Sleep(1000);
        int poll = TODO_POLL_DEFAULT_S;
        EnterCriticalSection(&g_todoSyncLock);
        poll = g_todoSyncPollSec;
        LeaveCriticalSection(&g_todoSyncLock);
        if (++waited >= poll) {
            waited = 0;
            DoPoll();
        }
    }
    return 0;
}

/* ---------- public API ---------- */
BOOL TodoSync_Init(HWND hwndMain, const wchar_t *iniPath) {
    if (!g_todoSyncLockInit) {
        InitializeCriticalSection(&g_todoSyncLock);
        g_todoSyncLockInit = TRUE;
    }
    g_todoSyncHwnd = hwndMain;
    if (iniPath) {
        if (wcsncpy_s(g_todoSyncIniPath, _countof(g_todoSyncIniPath), iniPath, _TRUNCATE) != 0)
            g_todoSyncIniPath[0] = L'\0';
    }
    LoadConfigW();
    memset(&g_todoSyncCache, 0, sizeof(g_todoSyncCache));
    InterlockedExchange(&g_todoSyncRunning, 1);
    /* 4 MiB: the merge pass holds 200-entry pull + push arrays together on
     * the first full sync, well past the 1 MiB default stack. */
    g_todoSyncThread = (HANDLE)_beginthreadex(NULL, 4u * 1024u * 1024u,
                                              PollThread, NULL, 0, NULL);
    if (!g_todoSyncThread) {
        InterlockedExchange(&g_todoSyncRunning, 0);
        return FALSE;
    }
    InterlockedExchange(&g_todoSyncPollNow, 1); /* first poll immediately */
    return TRUE;
}

void TodoSync_Shutdown(void) {
    InterlockedExchange(&g_todoSyncRunning, 0);
    if (g_todoSyncThread) {
        WaitForSingleObject(g_todoSyncThread, 3000);
        CloseHandle(g_todoSyncThread);
        g_todoSyncThread = NULL;
    }
}

void TodoSync_PollNow(void) {
    InterlockedExchange(&g_todoSyncPollNow, 1);
}

BOOL TodoSync_SetEnabled(BOOL enabled) {
    EnterCriticalSection(&g_todoSyncLock);
    g_todoSyncEnabled = enabled;
    LeaveCriticalSection(&g_todoSyncLock);
    char iniA[MAX_PATH] = "";
    if (WideCharToMultiByte(CP_UTF8, 0, g_todoSyncIniPath, -1,
                            iniA, sizeof(iniA), NULL, NULL) && iniA[0]) {
        WriteIniBool("Sync", "Enabled", enabled, iniA);
    }
    if (enabled) TodoSync_PollNow();
    return TRUE;
}

void TodoSync_Reload(void) {
    EnterCriticalSection(&g_todoSyncLock);
    LoadConfigW();
    LeaveCriticalSection(&g_todoSyncLock);
    TodoSync_PollNow();
}

void TodoSync_GetSettings(BOOL *enabled, char *serverUrl, size_t urlCap,
                          char *token, size_t tokenCap, int *pollSec) {
    EnterCriticalSection(&g_todoSyncLock);
    if (enabled) *enabled = g_todoSyncEnabled;
    if (serverUrl && urlCap) strcpy_s(serverUrl, urlCap, g_todoSyncServer);
    if (token && tokenCap) strcpy_s(token, tokenCap, g_todoSyncToken);
    if (pollSec) *pollSec = g_todoSyncPollSec;
    LeaveCriticalSection(&g_todoSyncLock);
}

void TodoSync_GetCalendarId(char *out, size_t cap) {
    EnterCriticalSection(&g_todoSyncLock);
    if (out && cap) strcpy_s(out, cap, g_todoSyncCalendarId);
    LeaveCriticalSection(&g_todoSyncLock);
}

void TodoSync_SetCalendarId(const char *calendarId) {
    EnterCriticalSection(&g_todoSyncLock);
    if (calendarId) strcpy_s(g_todoSyncCalendarId, sizeof(g_todoSyncCalendarId), calendarId);
    LeaveCriticalSection(&g_todoSyncLock);
    char iniA[MAX_PATH] = "";
    if (WideCharToMultiByte(CP_UTF8, 0, g_todoSyncIniPath, -1,
                            iniA, sizeof(iniA), NULL, NULL) && iniA[0]) {
        WriteIniString("Sync", "CalendarId", calendarId ? calendarId : "", iniA);
    }
    TodoSync_PollNow(); /* repull under the new book */
}

BOOL TodoSync_ApplySettings(BOOL enabled, const char *serverUrl,
                            const char *token, int pollSec) {
    if (pollSec < 15) pollSec = 15;
    if (pollSec > 600) pollSec = 600;
    EnterCriticalSection(&g_todoSyncLock);
    g_todoSyncEnabled = enabled;
    if (serverUrl) strcpy_s(g_todoSyncServer, sizeof(g_todoSyncServer), serverUrl);
    if (token) strcpy_s(g_todoSyncToken, sizeof(g_todoSyncToken), token);
    g_todoSyncPollSec = pollSec;
    LeaveCriticalSection(&g_todoSyncLock);
    char iniA[MAX_PATH] = "";
    if (!WideCharToMultiByte(CP_UTF8, 0, g_todoSyncIniPath, -1,
                             iniA, sizeof(iniA), NULL, NULL) || !iniA[0])
        return FALSE;
    char pollA[16];
    _snprintf_s(pollA, sizeof(pollA), _TRUNCATE, "%d", pollSec);
    IniKeyValue updates[5];
    updates[0].section = "Sync";
    updates[0].key = "Enabled";
    updates[0].value = enabled ? "1" : "0";
    updates[1].section = "Sync";
    updates[1].key = "ServerUrl";
    updates[1].value = serverUrl ? serverUrl : "";
    updates[2].section = "Sync";
    updates[2].key = "Token";
    updates[2].value = token ? token : "";
    updates[3].section = "Sync";
    updates[3].key = "PollInterval";
    updates[3].value = pollA;
    updates[4].section = "Sync";
    updates[4].key = "CalendarId";
    updates[4].value = g_todoSyncCalendarId;
    return WriteIniMultipleAtomic(iniA, updates, 5);
}

int TodoSync_GetLines(char lines[][256], int maxLines) {
    int n = 0;
    if (!lines || maxLines <= 0) return 0;
    EnterCriticalSection(&g_todoSyncLock);
#define EMIT(mark, item) do { \
    if (n < maxLines) { \
        const char *t = (item).title[0] ? (item).title : "(untitled)"; \
        char disp[TODO_TITLE_LEN]; \
        strcpy_s(disp, sizeof(disp), TodoSyncJson_LooksLocked(t) ? "[locked]" : t); \
        _snprintf_s(lines[n], 256, _TRUNCATE, " %s %s", mark, disp); \
        n++; \
    } \
} while (0)
    for (int i = 0; i < g_todoSyncCache.overdueCount; i++) EMIT("!", g_todoSyncCache.overdue[i]);
    for (int i = 0; i < g_todoSyncCache.todayCount; i++) EMIT("[ ]", g_todoSyncCache.today[i]);
    for (int i = 0; i < g_todoSyncCache.somedayCount && n < maxLines; i++) EMIT("[ ]", g_todoSyncCache.someday[i]);
#undef EMIT
    LeaveCriticalSection(&g_todoSyncLock);
    return n;
}

void TodoSync_GetCounts(int *todayOpen, int *overdueOpen, int *somedayOpen, int *pomoMin) {
    EnterCriticalSection(&g_todoSyncLock);
    if (todayOpen) *todayOpen = g_todoSyncCache.todayCount;
    if (overdueOpen) *overdueOpen = g_todoSyncCache.overdueCount;
    if (somedayOpen) *somedayOpen = g_todoSyncCache.somedayCount;
    if (pomoMin) *pomoMin = g_todoSyncCache.pomoMin;
    LeaveCriticalSection(&g_todoSyncLock);
}
