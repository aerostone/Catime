/**
 * @file todo_sync.h
 * @brief Catime TODO overlay — sync engine against tweek-clone backend.
 *
 * Drop-in: copy src/todo/* into Catime tree, include from WinMain.
 * Deps: Win32 + WinINet only (already used by update_http_client.c).
 * Threading: one background thread polls; UI reads cache under CRITICAL_SECTION.
 * Auth: Bearer device token from config.ini [Sync] Token= (issued in web UI).
 */
#ifndef TWEEK_TODO_SYNC_H
#define TWEEK_TODO_SYNC_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Lifecycle */
BOOL TodoSync_Init(HWND hwndMain, const wchar_t *iniPath);
/* Main window handle (NULL before Init) for dialogs/window parents. */
HWND TodoSync_MainHwnd(void);
void TodoSync_Shutdown(void);
/* Force one poll now (tray menu / wake / settings change). */
void TodoSync_PollNow(void);
/* Enable/disable without restart (writes config.ini). */
BOOL TodoSync_SetEnabled(BOOL enabled);

/* Pomodoro write-back: taskId may be NULL (=first today task). minutes e.g. 25. */
BOOL TodoSync_OnPomodoroComplete(const char *taskId, int minutes);
/* Mark a task done (PATCH status=done). Cache self-heals on next poll. */
BOOL TodoSync_MarkDone(const char *taskId);
/* Re-read Sync section from ini (settings dialog) without restart. */
void TodoSync_Reload(void);
/* Read current Sync settings for the settings dialog. */
void TodoSync_GetSettings(BOOL *enabled, char *serverUrl, size_t urlCap,
                          char *token, size_t tokenCap, int *pollSec);
/* Write Sync settings via INI cache layer (no bare ProfileString). */
BOOL TodoSync_ApplySettings(BOOL enabled, const char *serverUrl,
                            const char *token, int pollSec);

/* UI snapshot: fills up to maxLines UTF-8 lines " [ ] title" / " [x] title".
 * Returns number of lines written. Thread-safe, never blocks >50ms. */
int TodoSync_GetLines(char lines[][256], int maxLines);
/* Counts for overlay header, e.g. "今日 3 · 逾期 1 · 池 5". Thread-safe. */
void TodoSync_GetCounts(int *todayOpen, int *overdueOpen, int *somedayOpen,
                        int *pomoMin);

#ifdef __cplusplus
}
#endif

#endif /* TWEEK_TODO_SYNC_H */
