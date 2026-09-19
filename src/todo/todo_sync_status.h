/**
 * @file todo_sync_status.h
 * @brief Sync connection state + conflict badge source for UI.
 *
 * Written by the merge thread (todo_sync_merge.c), read by tray menu,
 * task-list dialog and settings dialog. Plain last-write-wins snapshot
 * under a lock: UI never blocks the poll thread.
 */
#ifndef CATIME_TODO_SYNC_STATUS_H
#define CATIME_TODO_SYNC_STATUS_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TODO_SYNC_STATE_IDLE = 0,    /* never polled / disabled */
    TODO_SYNC_STATE_OK = 1,      /* last poll succeeded */
    TODO_SYNC_STATE_OFFLINE = 2, /* network/HTTP failure */
    TODO_SYNC_STATE_AUTH = 3,    /* 401/403 from backend */
    TODO_SYNC_STATE_ERROR = 4    /* other failure */
} TodoSyncState;

/* Merge thread reports outcome (code: HTTP-ish, 0 = transport fail). */
void TodoSyncStatus_Report(TodoSyncState state, int code);
/* UI reads. stampOut: "HH:MM" of last report, "" when never. */
TodoSyncState TodoSyncStatus_Get(char *stampOut, size_t stampCap);
/* Short label for menus/dialogs, e.g. L"\u540c\u6b65\u6b63\u5e38 12:03". */
void TodoSyncStatus_Label(wchar_t *out, size_t cap);
/* Conflict snapshots on disk: count of todo.txt.conflict-* files. */
int TodoSyncStatus_ConflictCount(void);
/* Open explorer at the todo dir (conflict badge click-through). */
void TodoSyncStatus_OpenTodoDir(HWND hwnd);

#ifdef __cplusplus
}
#endif

#endif /* CATIME_TODO_SYNC_STATUS_H */
