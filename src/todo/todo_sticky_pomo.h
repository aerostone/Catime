/**
 * @file todo_sticky_pomo.h
 * @brief Sticky pomodoro binding state (internal header).
 */
#ifndef CATIME_TODO_STICKY_POMO_H
#define CATIME_TODO_STICKY_POMO_H

#include <windows.h>

#include "todo/todo_types.h"

void TodoStickyPomo_Start(HWND mainHwnd, const char *taskId);
void TodoStickyPomo_Clear(void);
BOOL TodoStickyPomo_Active(void);
const char *TodoStickyPomo_TaskId(void);
BOOL TodoStickyPomo_Validate(void);
int TodoStickyPomo_Remaining(void);

#endif /* CATIME_TODO_STICKY_POMO_H */
