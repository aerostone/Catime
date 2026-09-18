/**
 * @file todo_sync_http.c
 * @brief WinINet HTTP transport for the TODO sync engine.
 *
 * Style notes (match Catime conventions in update_http_client.c):
 * - WinINet InternetOpenW/InternetOpenUrlW with 10s timeouts.
 * - No third-party deps.
 */
#include "todo_sync_internal.h"

#pragma comment(lib, "wininet.lib")

char *TodoSyncHttp_Get(const char *url, const char *bearer) {
    HINTERNET hNet = NULL, hUrl = NULL;
    char *buf = NULL;
    size_t cap = 8192, total = 0;
    wchar_t wurl[1024], wagent[] = L"Catime-TodoSync/1.0";
    wchar_t whead[512];
    char headA[512];

    if (!MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, _countof(wurl))) return NULL;
    if (bearer && *bearer) {
        _snprintf_s(headA, sizeof(headA), _TRUNCATE, "Authorization: Bearer %s", bearer);
        if (!MultiByteToWideChar(CP_UTF8, 0, headA, -1, whead, _countof(whead))) return NULL;
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

/* Split an http(s) URL then open a WinINet request with the given verb. */
static HINTERNET OpenVerbRequest(const char *url, const wchar_t *verb,
                                 HINTERNET *outConn, HINTERNET *outNet) {
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
    if (!hNet) return NULL;
    HINTERNET hConn = InternetConnectW(hNet, whost, (INTERNET_PORT)port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConn) { InternetCloseHandle(hNet); return NULL; }
    HINTERNET hReq = HttpOpenRequestW(hConn, verb, wpath, NULL, NULL, NULL,
                                      INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hReq) { InternetCloseHandle(hConn); InternetCloseHandle(hNet); return NULL; }
    *outConn = hConn;
    *outNet = hNet;
    return hReq;
}

static BOOL SendJson(const char *url, const wchar_t *verb,
                     const char *bearer, const char *bodyJson) {
    HINTERNET hConn = NULL, hNet = NULL;
    HINTERNET hReq = OpenVerbRequest(url, verb, &hConn, &hNet);
    if (!hReq) return FALSE;
    if (!bodyJson) {
        InternetCloseHandle(hReq);
        InternetCloseHandle(hConn);
        InternetCloseHandle(hNet);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
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

BOOL TodoSyncHttp_Post(const char *url, const char *bearer, const char *bodyJson) {
    return SendJson(url, L"POST", bearer, bodyJson);
}

BOOL TodoSyncHttp_Patch(const char *url, const char *bearer, const char *bodyJson) {
    return SendJson(url, L"PATCH", bearer, bodyJson);
}
