/**
 * @file todo_sync.c
 * @brief Catime TODO overlay sync engine (WinINet, no third-party deps).
 *
 * Style notes (match Catime conventions in update_http_client.c):
 * - WinINet InternetOpenW/InternetOpenUrlW with 10s timeouts.
 * - Minimal hand-rolled JSON extraction (titles only) — backend contract:
 *   {today_open:[{id,title,status}], overdue_open:[...], someday_open:[...],
 *    pomodoros_today_min:N, server_time:T}
 * - E2EE envelopes: backend sends title as-is (ciphertext or plaintext);
 *   locked titles render as-is; web UI owns unlock. Client shows
 *   "[locked]" placeholder when title looks like base64 envelope >64 chars
 *   with empty date? No — server keeps it simple: E2EE accounts get ciphertext
 *   titles; we detect non-printable/overlong single-token titles and show
 *   "上锁，去网页管理". Plaintext accounts render normally.
 */
#include <windows.h>
#include <wininet.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "todo_sync.h"

#pragma comment(lib, "wininet.lib")

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

static CRITICAL_SECTION g_lock;
static BOOL g_lockInit = FALSE;
static TodoCache g_cache;
static HWND g_hwnd = NULL;
static wchar_t g_iniPath[MAX_PATH] = L"";
static char g_server[TODO_URL_LEN] = "";
static char g_token[TODO_TOKEN_LEN] = "";
static BOOL g_enabled = FALSE;
static int g_pollSec = TODO_POLL_DEFAULT_S;
static HANDLE g_thread = NULL;
static volatile LONG g_running = 0;
static volatile LONG g_pollNow = 0;
static UINT_PTR g_wakeTimer = 0;

/* ---------- config ---------- */
static void LoadConfigW(void) {
    wchar_t server[TODO_URL_LEN];
    wchar_t token[TODO_TOKEN_LEN];
    GetPrivateProfileStringW(L"Sync", L"ServerUrl", L"", server, _countof(server), g_iniPath);
    GetPrivateProfileStringW(L"Sync", L"Token", L"", token, _countof(token), g_iniPath);
    g_enabled = GetPrivateProfileIntW(L"Sync", L"Enabled", 0, g_iniPath) != 0;
    g_pollSec = (int)GetPrivateProfileIntW(L"Sync", L"PollInterval", TODO_POLL_DEFAULT_S, g_iniPath);
    if (g_pollSec < 15) g_pollSec = 15;
    if (g_pollSec > 600) g_pollSec = 600;
    WideCharToMultiByte(CP_UTF8, 0, server, -1, g_server, sizeof(g_server), NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, token, -1, g_token, sizeof(g_token), NULL, NULL);
    /* trim trailing '/' */
    size_t n = strlen(g_server);
    while (n > 0 && g_server[n - 1] == '/') g_server[--n] = '\0';
}

/* ---------- minimal JSON extraction ---------- */
/* find "key":<value> helpers; arrays of {id,title,status} only. */
static const char *FindKey(const char *json, const char *key) {
    char pat[64];
    _snprintf_s(pat, sizeof(pat), _TRUNCATE, "\"%s\"", key);
    return strstr(json, pat);
}

static BOOL ExtractStr(const char *obj, const char *key, char *out, size_t outSize) {
    const char *p = FindKey(obj, key);
    if (!p) return FALSE;
    p = strchr(p, ':');
    if (!p) return FALSE;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return FALSE;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < outSize) {
        if (*p == '\\' && *(p + 1)) {
            p++;
            switch (*p) {
            case 'n': out[i++] = '\n'; break;
            case 't': out[i++] = '\t'; break;
            case 'u': /* \uXXXX → best-effort '?'; CJK titles are raw UTF-8, not \u-escaped by Go */
                out[i++] = '?';
                if (*(p+1)) p++; if (*(p+1)) p++; if (*(p+1)) p++; if (*(p+1)) p++;
                break;
            default: out[i++] = *p; break;
            }
            p++;
        } else {
            out[i++] = *p++;
        }
    }
    out[i] = '\0';
    return TRUE;
}

static BOOL ExtractInt(const char *json, const char *key, long long *out) {
    const char *p = FindKey(json, key);
    if (!p) return FALSE;
    p = strchr(p, ':');
    if (!p) return FALSE;
    *out = _strtoi64(p + 1, NULL, 10);
    return TRUE;
}

/* parse [{...},{...}] array at key into items; returns count. */
static int ParseItemArray(const char *json, const char *key, TodoItem *items, int max) {
    const char *p = FindKey(json, key);
    if (!p) return 0;
    p = strchr(p, '[');
    if (!p) return 0;
    p++;
    int n = 0;
    while (*p && *p != ']' && n < max) {
        const char *o = strchr(p, '{');
        const char *end = strchr(p, ']');
        if (!o || (end && o > end)) break;
        const char *oe = strchr(o, '}');
        if (!oe) break;
        char saved = '\0';
        /* temporarily terminate: copy object slice */
        size_t len = (size_t)(oe - o + 1);
        char *slice = (char *)malloc(len + 1);
        if (!slice) break;
        memcpy(slice, o, len);
        slice[len] = '\0';
        (void)saved;
        memset(&items[n], 0, sizeof(items[n]));
        ExtractStr(slice, "id", items[n].id, sizeof(items[n].id));
        ExtractStr(slice, "title", items[n].title, sizeof(items[n].title));
        ExtractStr(slice, "status", items[n].status, sizeof(items[n].status));
        free(slice);
        if (items[n].id[0]) n++;
        p = oe + 1;
    }
    return n;
}

static BOOL LooksLocked(const char *title) {
    /* E2EE ciphertext titles: long single token, no spaces/CJK. */
    size_t n = strlen(title);
    if (n < 48) return FALSE;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)title[i];
        if (c >= 0x80 || c == ' ') return FALSE;
    }
    return TRUE;
}

/* ---------- HTTP ---------- */
static char *HttpGet(const char *url, const char *bearer) {
    HINTERNET hNet = NULL, hUrl = NULL;
    char *buf = NULL;
    size_t cap = 8192, total = 0;
    wchar_t wurl[1024], wagent[] = L"Catime-TodoSync/1.0";
    wchar_t whead[512];
    char headA[512];

    MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, _countof(wurl));
    if (bearer && *bearer) {
        _snprintf_s(headA, sizeof(headA), _TRUNCATE, "Authorization: Bearer %s", bearer);
        MultiByteToWideChar(CP_UTF8, 0, headA, -1, whead, _countof(whead));
    }
    hNet = InternetOpenW(wagent, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hNet) return NULL;
    DWORD to = TODO_HTTP_TIMEOUT_MS;
    InternetSetOptionW(hNet, INTERNET_OPTION_CONNECT_TIMEOUT, &to, sizeof(to));
    InternetSetOptionW(hNet, INTERNET_OPTION_RECEIVE_TIMEOUT, &to, sizeof(to));
    hUrl = InternetOpenUrlW(hNet, wurl, (bearer && *bearer) ? whead : NULL,
                            (bearer && *bearer) ? (DWORD)wcslen(whead) : 0,
                            INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hUrl) { InternetCloseHandle(hNet); return NULL; }
    buf = (char *)malloc(cap);
    if (!buf) { InternetCloseHandle(hUrl); InternetCloseHandle(hNet); return NULL; }
    for (;;) {
        DWORD got = 0;
        if (total + 2048 > cap) {
            cap *= 2;
            if (cap > TODO_RESP_MAX + 2048) break;
            char *nb = (char *)realloc(buf, cap);
            if (!nb) break;
            buf = nb;
        }
        if (!InternetReadFile(hUrl, buf + total, (DWORD)(cap - total - 1), &got) || got == 0) break;
        total += got;
        if (total >= TODO_RESP_MAX) break;
    }
    buf[total] = '\0';
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hNet);
    return buf;
}

static BOOL HttpPost(const char *url, const char *bearer, const char *bodyJson) {
    /* minimal POST via WinINet: split url -> host/path, HttpSendRequestA. */
    const char *p = strstr(url, "://");
    p = p ? p + 3 : url;
    const char *slash = strchr(p, '/');
    char host[256] = "";
    char path[512] = "/";
    wchar_t whost[256], wpath[512];
    if (slash) {
        size_t hn = (size_t)(slash - p);
        if (hn >= sizeof(host)) hn = sizeof(host) - 1;
        memcpy(host, p, hn);
        host[hn] = '\0';
        _snprintf_s(path, sizeof(path), _TRUNCATE, "%s", slash);
    } else {
        _snprintf_s(host, sizeof(host), _TRUNCATE, "%s", p);
    }
    /* strip :port from host for InternetConnect */
    char *colon = strchr(host, ':');
    int port = INTERNET_DEFAULT_HTTP_PORT;
    if (colon) { *colon = '\0'; port = atoi(colon + 1); }
    MultiByteToWideChar(CP_UTF8, 0, host, -1, whost, _countof(whost));
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, _countof(wpath));
    HINTERNET hNet = InternetOpenW(L"Catime-TodoSync/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hNet) return FALSE;
    HINTERNET hConn = InternetConnectW(hNet, whost, (INTERNET_PORT)port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConn) { InternetCloseHandle(hNet); return FALSE; }
    HINTERNET hReq = HttpOpenRequestW(hConn, L"POST", wpath, NULL, NULL, NULL,
                                      INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hReq) { InternetCloseHandle(hConn); InternetCloseHandle(hNet); return FALSE; }
    char heads[512];
    _snprintf_s(heads, sizeof(heads), _TRUNCATE,
                "Content-Type: application/json\r\nAuthorization: Bearer %s\r\n",
                bearer ? bearer : "");
    BOOL ok = HttpSendRequestA(hReq, heads, (DWORD)strlen(heads),
                               (LPVOID)bodyJson, (DWORD)strlen(bodyJson));
    if (hReq) InternetCloseHandle(hReq);
    if (hConn) InternetCloseHandle(hConn);
    if (hNet) InternetCloseHandle(hNet);
    return ok;
}

static BOOL HttpPatch(const char *url, const char *bearer, const char *bodyJson) {
    /* same as POST but verb PATCH */
    const char *p = strstr(url, "://");
    p = p ? p + 3 : url;
    const char *slash = strchr(p, '/');
    char host[256] = "";
    char path[512] = "/";
    wchar_t whost[256], wpath[512];
    if (slash) {
        size_t hn = (size_t)(slash - p);
        if (hn >= sizeof(host)) hn = sizeof(host) - 1;
        memcpy(host, p, hn);
        host[hn] = '\0';
        _snprintf_s(path, sizeof(path), _TRUNCATE, "%s", slash);
    } else {
        _snprintf_s(host, sizeof(host), _TRUNCATE, "%s", p);
    }
    char *colon = strchr(host, ':');
    int port = INTERNET_DEFAULT_HTTP_PORT;
    if (colon) { *colon = '\0'; port = atoi(colon + 1); }
    MultiByteToWideChar(CP_UTF8, 0, host, -1, whost, _countof(whost));
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, _countof(wpath));
    HINTERNET hNet = InternetOpenW(L"Catime-TodoSync/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hNet) return FALSE;
    HINTERNET hConn = InternetConnectW(hNet, whost, (INTERNET_PORT)port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConn) { InternetCloseHandle(hNet); return FALSE; }
    HINTERNET hReq = HttpOpenRequestW(hConn, L"PATCH", wpath, NULL, NULL, NULL,
                                      INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hReq) { InternetCloseHandle(hConn); InternetCloseHandle(hNet); return FALSE; }
    char heads[512];
    _snprintf_s(heads, sizeof(heads), _TRUNCATE,
                "Content-Type: application/json\r\nAuthorization: Bearer %s\r\n",
                bearer ? bearer : "");
    BOOL ok = HttpSendRequestA(hReq, heads, (DWORD)strlen(heads),
                               (LPVOID)bodyJson, (DWORD)strlen(bodyJson));
    if (hReq) InternetCloseHandle(hReq);
    if (hConn) InternetCloseHandle(hConn);
    if (hNet) InternetCloseHandle(hNet);
    return ok;
}

/* ---------- poll ---------- */
static void DoPoll(void) {
    char server[TODO_URL_LEN], token[TODO_TOKEN_LEN];
    EnterCriticalSection(&g_lock);
    strcpy_s(server, sizeof(server), g_server);
    strcpy_s(token, sizeof(token), g_token);
    BOOL en = g_enabled;
    LeaveCriticalSection(&g_lock);
    if (!en || !server[0] || !token[0]) return;

    char url[TODO_URL_LEN + 64];
    _snprintf_s(url, sizeof(url), _TRUNCATE, "%s/api/catime/sync?top=%d", server, TODO_MAX_TASKS);
    char *resp = HttpGet(url, token);
    if (!resp) return; /* offline: keep old cache */
    TodoCache nc;
    memset(&nc, 0, sizeof(nc));
    nc.todayCount = ParseItemArray(resp, "today_open", nc.today, TODO_MAX_TASKS);
    nc.overdueCount = ParseItemArray(resp, "overdue_open", nc.overdue, TODO_MAX_TASKS);
    nc.somedayCount = ParseItemArray(resp, "someday_open", nc.someday, TODO_MAX_TASKS);
    long long v = 0;
    if (ExtractInt(resp, "pomodoros_today_min", &v)) nc.pomoMin = (int)v;
    if (ExtractInt(resp, "server_time", &v)) nc.serverTime = v;
    free(resp);
    EnterCriticalSection(&g_lock);
    g_cache = nc;
    LeaveCriticalSection(&g_lock);
    if (g_hwnd) InvalidateRect(g_hwnd, NULL, TRUE);
}

static unsigned __stdcall PollThread(void *arg) {
    (void)arg;
    int waited = 0;
    while (InterlockedCompareExchange(&g_running, 1, 1)) {
        if (InterlockedExchange(&g_pollNow, 0)) {
            DoPoll();
            waited = 0;
        }
        Sleep(1000);
        int poll = TODO_POLL_DEFAULT_S;
        EnterCriticalSection(&g_lock);
        poll = g_pollSec;
        LeaveCriticalSection(&g_lock);
        if (++waited >= poll) {
            waited = 0;
            DoPoll();
        }
    }
    return 0;
}

/* ---------- public API ---------- */
BOOL TodoSync_Init(HWND hwndMain, const wchar_t *iniPath) {
    if (!g_lockInit) {
        InitializeCriticalSection(&g_lock);
        g_lockInit = TRUE;
    }
    g_hwnd = hwndMain;
    if (iniPath) wcsncpy_s(g_iniPath, _countof(g_iniPath), iniPath, _TRUNCATE);
    LoadConfigW();
    memset(&g_cache, 0, sizeof(g_cache));
    InterlockedExchange(&g_running, 1);
    g_thread = (HANDLE)_beginthreadex(NULL, 0, PollThread, NULL, 0, NULL);
    if (!g_thread) {
        InterlockedExchange(&g_running, 0);
        return FALSE;
    }
    InterlockedExchange(&g_pollNow, 1); /* first poll immediately */
    return TRUE;
}

void TodoSync_Shutdown(void) {
    InterlockedExchange(&g_running, 0);
    if (g_thread) {
        WaitForSingleObject(g_thread, 3000);
        CloseHandle(g_thread);
        g_thread = NULL;
    }
}

void TodoSync_PollNow(void) {
    InterlockedExchange(&g_pollNow, 1);
}

BOOL TodoSync_SetEnabled(BOOL enabled) {
    EnterCriticalSection(&g_lock);
    g_enabled = enabled;
    LeaveCriticalSection(&g_lock);
    WritePrivateProfileStringW(L"Sync", L"Enabled", enabled ? L"1" : L"0", g_iniPath);
    if (enabled) TodoSync_PollNow();
    return TRUE;
}

BOOL TodoSync_OnPomodoroComplete(const char *taskId, int minutes) {
    char server[TODO_URL_LEN], token[TODO_TOKEN_LEN];
    char tid[TODO_ID_LEN] = "";
    EnterCriticalSection(&g_lock);
    strcpy_s(server, sizeof(server), g_server);
    strcpy_s(token, sizeof(token), g_token);
    BOOL en = g_enabled;
    if ((!taskId || !*taskId) && g_cache.todayCount > 0)
        strcpy_s(tid, sizeof(tid), g_cache.today[0].id);
    else if (taskId)
        strcpy_s(tid, sizeof(tid), taskId);
    LeaveCriticalSection(&g_lock);
    if (!en || !server[0] || !token[0]) return FALSE;
    char url[TODO_URL_LEN + 32], body[256];
    _snprintf_s(url, sizeof(url), _TRUNCATE, "%s/api/pomodoro", server);
    _snprintf_s(body, sizeof(body), _TRUNCATE,
                "{\"task_id\":\"%s\",\"minutes\":%d,\"origin\":\"catime\"}", tid, minutes);
    BOOL ok = HttpPost(url, token, body);
    if (ok) TodoSync_PollNow();
    return ok;
}

BOOL TodoSync_MarkDone(const char *taskId) {
    char server[TODO_URL_LEN], token[TODO_TOKEN_LEN];
    EnterCriticalSection(&g_lock);
    strcpy_s(server, sizeof(server), g_server);
    strcpy_s(token, sizeof(token), g_token);
    BOOL en = g_enabled;
    LeaveCriticalSection(&g_lock);
    if (!en || !server[0] || !token[0] || !taskId || !*taskId) return FALSE;
    char url[TODO_URL_LEN + 128];
    _snprintf_s(url, sizeof(url), _TRUNCATE, "%s/api/tasks/%s", server, taskId);
    BOOL ok = HttpPatch(url, token, "{\"status\":\"done\"}");
    if (ok) TodoSync_PollNow();
    return ok;
}

int TodoSync_GetLines(char lines[][256], int maxLines) {
    int n = 0;
    EnterCriticalSection(&g_lock);
#define EMIT(mark, item) do { \
    if (n < maxLines) { \
        const char *t = (item).title[0] ? (item).title : "(untitled)"; \
        char disp[TODO_TITLE_LEN]; \
        strcpy_s(disp, sizeof(disp), LooksLocked(t) ? "[locked]" : t); \
        _snprintf_s(lines[n], 256, _TRUNCATE, " %s %s", mark, disp); \
        n++; \
    } \
} while (0)
    for (int i = 0; i < g_cache.overdueCount; i++) EMIT("!", g_cache.overdue[i]);
    for (int i = 0; i < g_cache.todayCount; i++) EMIT("[ ]", g_cache.today[i]);
    for (int i = 0; i < g_cache.somedayCount && n < maxLines; i++) EMIT("[ ]", g_cache.someday[i]);
#undef EMIT
    LeaveCriticalSection(&g_lock);
    return n;
}

void TodoSync_GetCounts(int *todayOpen, int *overdueOpen, int *somedayOpen, int *pomoMin) {
    EnterCriticalSection(&g_lock);
    if (todayOpen) *todayOpen = g_cache.todayCount;
    if (overdueOpen) *overdueOpen = g_cache.overdueCount;
    if (somedayOpen) *somedayOpen = g_cache.somedayCount;
    if (pomoMin) *pomoMin = g_cache.pomoMin;
    LeaveCriticalSection(&g_lock);
}
