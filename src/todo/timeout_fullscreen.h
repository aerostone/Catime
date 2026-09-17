/**
 * @file timeout_fullscreen.h
 * @brief Fullscreen dim overlay + centered big-text dialog for timeout/pomodoro alerts.
 *
 * Design: a fullscreen WS_POPUP layered dim window (70% black, click-through
 * EXCEPT center dialog) + a centered dialog with 48px+ title text and message.
 * Any key / click on dialog button dismisses. Auto-dismiss after timeoutMs.
 * Enabled by default (NOTIFICATION_FULLSCREEN_TIMEOUT=TRUE). Set FALSE to fall back to small toast.
 */
#ifndef CATIME_TIMEOUT_FULLSCREEN_H
#define CATIME_TIMEOUT_FULLSCREEN_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Shows fullscreen timeout alert. Blocks ~100ms to create windows, then returns
 * immediately (auto-dismiss timer inside). Safe to call from timer thread via
 * PostMessage pattern — caller must be UI thread (same as ShowNotification). */
void TimeoutFullscreen_Show(HWND hwndOwner, const wchar_t *title, const wchar_t *message, int timeoutMs);

/* Dismiss if showing (called on any key/click/timeout). No-op if not showing. */
void TimeoutFullscreen_Dismiss(void);

/* TRUE while overlay is on screen (used to avoid stacking). */
BOOL TimeoutFullscreen_IsShowing(void);

#ifdef __cplusplus
}
#endif

#endif /* CATIME_TIMEOUT_FULLSCREEN_H */
