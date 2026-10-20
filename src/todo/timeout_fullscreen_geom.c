/**
 * @file timeout_fullscreen_geom.c
 * @brief Monitor selection for the fullscreen timeout overlay.
 *
 * The overlay must live on ONE screen. Centering it on the union of all
 * monitor rects puts the card on the seam between two side-by-side displays,
 * so the user only ever sees half the text. Prefer the monitor the cursor is
 * on (that is where the user is looking), then the primary, then the whole
 * virtual screen as a last resort.
 */
#include <windows.h>
#include <string.h>

#include "todo/timeout_fullscreen_geom.h"

void TimeoutFullscreen_PickMonitorRect(RECT *out) {
    if (!out) return;

    HMONITOR mon = NULL;
    POINT pt;
    if (GetCursorPos(&pt)) {
        mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONULL);
    }
    if (!mon) {
        POINT origin = {0, 0};
        mon = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
    }

    MONITORINFO mi;
    memset(&mi, 0, sizeof(mi));
    mi.cbSize = sizeof(mi);
    if (mon && GetMonitorInfoW(mon, &mi)) {
        *out = mi.rcMonitor;
        return;
    }

    out->left = 0;
    out->top = 0;
    out->right = GetSystemMetrics(SM_CXSCREEN);
    out->bottom = GetSystemMetrics(SM_CYSCREEN);
}
