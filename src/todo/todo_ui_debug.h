/**
 * @file todo_ui_debug.h
 * @brief TODO UI debug overlay: control metrics dump + layout verify.
 *
 * Three-layer switch (any layer enables):
 *  1. compile: -DCATIME_UI_DEBUG=1 (CI debug artifact forces it on).
 *  2. env: CATIME_UI_DEBUG=1 at process start.
 *  3. ini: [Debug] UiDebug=1 beside config.ini (runtime toggle).
 * Release artifact compiles the switch OFF and strips all dump code
 * via TodoUiDebug_Enabled() gating (no hot-path cost).
 *
 * When enabled, each TODO dialog/sticky dumps id/x/y/w/h/font on
 * WM_INITDIALOG (client coords, 96-DPI normalized) to the log, and
 * paints a dashed overlay rect + id label around every child control
 * so layout regressions (overlap/shift) are visible on screenshots.
 */
#ifndef CATIME_TODO_UI_DEBUG_H
#define CATIME_TODO_UI_DEBUG_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CATIME_UI_DEBUG
/* Runtime state: call once at startup (reads env + ini). */
void TodoUiDebug_Init(const char *iniDirA);
/* Any-layer enable check (cheap cached flag). */
BOOL TodoUiDebug_Enabled(void);
/* Dump one dialog's child metrics to log with tag prefix. */
void TodoUiDebug_DumpDialog(HWND hdlg, const char *tag);
/* Overlay paint hook: call at end of WM_PAINT (no-op when disabled). */
void TodoUiDebug_PaintOverlay(HWND hdlg);
/* Toggle at runtime (settings checkbox), persists to ini. */
void TodoUiDebug_SetEnabled(BOOL on);
#else
/* Release: zero-cost stubs (nothing linked, checkbox hidden). */
static __inline void TodoUiDebug_Init(const char *iniDirA) {
    (void)iniDirA;
}
static __inline BOOL TodoUiDebug_Enabled(void) {
    return FALSE;
}
static __inline void TodoUiDebug_DumpDialog(HWND hdlg, const char *tag) {
    (void)hdlg;
    (void)tag;
}
static __inline void TodoUiDebug_PaintOverlay(HWND hdlg) {
    (void)hdlg;
}
static __inline void TodoUiDebug_SetEnabled(BOOL on) {
    (void)on;
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* CATIME_TODO_UI_DEBUG_H */
