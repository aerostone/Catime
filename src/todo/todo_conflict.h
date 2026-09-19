/**
 * @file todo_conflict.h
 * @brief Conflict snapshot helpers shared by status + UI surfaces.
 */
#ifndef CATIME_TODO_CONFLICT_H
#define CATIME_TODO_CONFLICT_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

int TodoConflict_Count(void);
void TodoConflict_OpenDir(HWND hwnd);

#ifdef __cplusplus
}
#endif

#endif /* CATIME_TODO_CONFLICT_H */
