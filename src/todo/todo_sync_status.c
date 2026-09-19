/**
 * @file todo_sync_status.c
 * @brief Sync connection snapshot + conflict counter for UI surfaces.
 */
#include "todo_sync_status.h"
#include "language.h"

#include <stdio.h>
#include <string.h>

static CRITICAL_SECTION s_lock;
static BOOL s_lockInit = FALSE;
static TodoSyncState s_state = TODO_SYNC_STATE_IDLE;
static int s_code = 0;
static SYSTEMTIME s_stamp = {0};

static void EnsureLock(void) {
    if (!s_lockInit) {
        InitializeCriticalSection(&s_lock);
        s_lockInit = TRUE;
    }
}

void TodoSyncStatus_Report(TodoSyncState state, int code) {
    EnsureLock();
    EnterCriticalSection(&s_lock);
    s_state = state;
    s_code = code;
    GetLocalTime(&s_stamp);
    LeaveCriticalSection(&s_lock);
}

TodoSyncState TodoSyncStatus_Get(char *stampOut, size_t stampCap) {
    EnsureLock();
    EnterCriticalSection(&s_lock);
    TodoSyncState st = s_state;
    SYSTEMTIME c = s_stamp;
    LeaveCriticalSection(&s_lock);
    if (stampOut && stampCap) {
        if (c.wYear)
#if defined(_MSC_VER)
            _snprintf_s(stampOut, stampCap, _TRUNCATE, "%02d:%02d",
                        (int)c.wHour, (int)c.wMinute);
#else
            snprintf(stampOut, stampCap, "%02d:%02d",
                     (int)c.wHour, (int)c.wMinute);
#endif
        else
            stampOut[0] = '\0';
    }
    return st;
}

int TodoSyncStatus_ConflictCount(void) {
    extern int TodoConflict_Count(void);
    return TodoConflict_Count();
}

void TodoSyncStatus_Label(wchar_t *out, size_t cap) {
    if (!out || !cap) return;
    char stamp[16] = "";
    TodoSyncState st = TodoSyncStatus_Get(stamp, sizeof(stamp));
    const wchar_t *body = L"";
    switch (st) {
    case TODO_SYNC_STATE_OK: body = GetLocalizedString(L"\u540c\u6b65\u6b63\u5e38", L"Sync OK"); break;
    case TODO_SYNC_STATE_OFFLINE: body = GetLocalizedString(L"\u79bb\u7ebf", L"Offline"); break;
    case TODO_SYNC_STATE_AUTH: body = GetLocalizedString(L"\u6388\u6743\u5931\u8d25", L"Auth failed"); break;
    case TODO_SYNC_STATE_ERROR: body = GetLocalizedString(L"\u540c\u6b65\u5f02\u5e38", L"Sync error"); break;
    default: body = GetLocalizedString(L"\u672a\u540c\u6b65", L"Not synced"); break;
    }
    wchar_t ws[16] = L"";
    if (stamp[0]) MultiByteToWideChar(CP_UTF8, 0, stamp, -1, ws, _countof(ws));
    if (ws[0])
#if defined(_MSC_VER)
        _snwprintf_s(out, cap, _TRUNCATE, L"\u540c\u6b65\u00b7%s %s", body, ws);
#else
        swprintf(out, cap, L"\u540c\u6b65\u00b7%s %s", body, ws);
#endif
    else
#if defined(_MSC_VER)
        _snwprintf_s(out, cap, _TRUNCATE, L"\u540c\u6b65\u00b7%s", body);
#else
        swprintf(out, cap, L"\u540c\u6b65\u00b7%s", body);
#endif
}

void TodoSyncStatus_OpenTodoDir(HWND hwnd) {
    extern const char *TodoStore_TxtPath(void);
    const char *txt = TodoStore_TxtPath();
    if (!txt || !txt[0]) return;
    char dir[MAX_PATH] = "";
#if defined(_MSC_VER)
    strcpy_s(dir, sizeof(dir), txt);
#else
    snprintf(dir, sizeof(dir), "%s", txt);
#endif
    char *sep = strrchr(dir, '\\');
    if (!sep) sep = strrchr(dir, '/');
    if (sep) *sep = '\0';
    else return;
    wchar_t w[MAX_PATH];
    if (!MultiByteToWideChar(CP_UTF8, 0, dir, -1, w, _countof(w))) return;
    ShellExecuteW(hwnd, L"open", w, NULL, NULL, SW_SHOWNORMAL);
}
