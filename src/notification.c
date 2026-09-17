/**
 * @file notification.c
 * @brief Notification mode dispatch.
 */

#include "notification_internal.h"
#include "todo/timeout_fullscreen.h"

void ShowNotification(HWND hwnd, const wchar_t* message) {
    if (!message) return;
    HWND owner = NotificationGetOwnerWindow(hwnd);
    if (!owner) return;

    NotificationLoadConfigs();

    if (g_AppConfig.notification.display.disabled || g_AppConfig.notification.display.timeout_ms == 0) {
        return;
    }

    /* Fullscreen big-text timeout alert (opt-in via NOTIFICATION_FULLSCREEN_TIMEOUT).
     * Falls back to toast if overlay is already showing or type is OS/modal. */
    if (g_AppConfig.notification.display.fullscreen_timeout &&
        g_AppConfig.notification.display.type == NOTIFICATION_TYPE_CATIME &&
        !TimeoutFullscreen_IsShowing()) {
        TimeoutFullscreen_Show(owner, L"Catime", message,
                               g_AppConfig.notification.display.timeout_ms);
        if (TimeoutFullscreen_IsShowing()) return;
    }

    switch (g_AppConfig.notification.display.type) {
        case NOTIFICATION_TYPE_CATIME:
            ShowToastNotification(hwnd, message);
            break;
        case NOTIFICATION_TYPE_SYSTEM_MODAL:
            ShowModalNotification(hwnd, message);
            break;
        case NOTIFICATION_TYPE_OS:
            NotificationFallbackToTray(hwnd, message);
            break;
        default:
            ShowToastNotification(hwnd, message);
            break;
    }
}
