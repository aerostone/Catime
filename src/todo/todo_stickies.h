/**
 * @file todo_stickies.h
 * @brief Stickies desktop notes: lifecycle + window management.
 *
 * Each sticky is a small topmost tool window bound to one local task.
 * Position/size persist in todo.ini [Sticky<id>] sections.
 * Pure Win32, no global hooks (CatimeInteractionSafety compliant).
 */
#ifndef CATIME_TODO_STICKIES_H
#define CATIME_TODO_STICKIES_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Show (or create window for) one sticky by local task id. */
void TodoStickies_Show(const char *taskId);
/* Hide one sticky window (task data kept). */
void TodoStickies_Hide(const char *taskId);
/* Hide all sticky windows. */
void TodoStickies_HideAll(void);
/* Show windows for all pinned tasks (called at startup). */
void TodoStickies_RestoreAll(void);
/* Close + forget window for a removed task. */
void TodoStickies_Forget(const char *taskId);
/* Whether a sticky window is currently visible for task id. */
BOOL TodoStickies_IsVisible(const char *taskId);
/* Pin flag stored per task; drives RestoreAll. */
BOOL TodoSticky_IsPinned(const char *taskId);
BOOL TodoSticky_SetPinned(const char *taskId, BOOL pinned);
void TodoSticky_LoadGeom(const char *taskId, int *x, int *y, int *w, int *h);
void TodoSticky_SaveGeom(const char *taskId, int x, int y, int w, int h);
BOOL TodoSticky_IsCollapsed(const char *taskId);
void TodoSticky_SetCollapsed(const char *taskId, BOOL collapsed);
BOOL TodoSticky_TopmostGlobal(void);
void TodoSticky_SetTopmostGlobal(BOOL topmost);
int TodoSticky_TopmostOverride(const char *taskId);
void TodoSticky_SetTopmostOverride(const char *taskId, int mode);
BOOL TodoSticky_TopmostFor(const char *taskId);
void TodoSticky_RetopAll(void);
void TodoSticky_UpdateTips(HWND hwnd, BOOL collapsed);

#ifdef __cplusplus
}
#endif

#endif /* CATIME_TODO_STICKIES_H */
