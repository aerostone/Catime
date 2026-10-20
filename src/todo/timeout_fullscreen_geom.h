/**
 * @file timeout_fullscreen_geom.h
 * @brief Monitor/geometry helpers for the fullscreen timeout overlay.
 *
 * Split out of timeout_fullscreen.c to keep both files under the 300-line
 * source-layout gate. See that file for the multi-monitor rationale.
 */
#ifndef CATIME_TIMEOUT_FULLSCREEN_GEOM_H
#define CATIME_TIMEOUT_FULLSCREEN_GEOM_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Rect of the single monitor the alert should occupy: the monitor under the
 * cursor when there is one, else the primary, else the whole virtual screen.
 * Never the union of all monitors - see timeout_fullscreen.c's header. */
void TimeoutFullscreen_PickMonitorRect(RECT *out);

#ifdef __cplusplus
}
#endif

#endif /* CATIME_TIMEOUT_FULLSCREEN_GEOM_H */
