/**
 * @file todo_sticky_pomo.c
 * @brief Sticky pomodoro binding: task id + countdown state.
 *
 * Binding is per sticky window (memory only, in slot.pomoTaskId).
 * Start: TodoStickyPomo_Start(mainHwnd, taskId) launches the shared
 * pomodoro timer and records the binding. Tick: 1s window timer updates
 * remaining display. Completion flows through the existing pomodoro path
 * which reports task_id via TodoSync_OnPomodoroComplete.
 * Row deleted -> FindById fails -> binding auto-cleared on next tick.
 */
#include <string.h>

#include "todo_stickies.h"
#include "todo_store.h"

#include "todo_sticky_pomo.h"

static char s_pomoTaskId[TODO_STORE_ID_LEN] = "";
static HWND s_mainHwnd = NULL;

void TodoStickyPomo_Start(HWND mainHwnd, const char *taskId) {
    void StartPomodoroTimer(HWND hwnd);
    if (!taskId || !taskId[0]) return;
    TodoTask t;
    memset(&t, 0, sizeof(t));
    if (!TodoStore_FindById(taskId, &t)) return;
    strcpy_s(s_pomoTaskId, sizeof(s_pomoTaskId), taskId);
    s_mainHwnd = mainHwnd;
    if (mainHwnd) StartPomodoroTimer(mainHwnd);
}

void TodoStickyPomo_Clear(void) {
    s_pomoTaskId[0] = '\0';
}

BOOL TodoStickyPomo_Active(void) {
    return s_pomoTaskId[0] != '\0';
}

const char *TodoStickyPomo_TaskId(void) {
    return s_pomoTaskId;
}

/* Remaining seconds of the running pomo (0 when idle). */
int TodoStickyPomo_Remaining(void) {
    extern int CLOCK_TOTAL_TIME;
    extern int countdown_elapsed_time;
    extern BOOL TimerEvents_IsActivePomodoroTimer(void);
    if (!s_pomoTaskId[0]) return 0;
    if (!TimerEvents_IsActivePomodoroTimer()) return 0;
    int rem = CLOCK_TOTAL_TIME - countdown_elapsed_time;
    return rem > 0 ? rem : 0;
}

/* Validate binding (call on tick): FALSE when row gone -> cleared. */
BOOL TodoStickyPomo_Validate(void) {
    if (!s_pomoTaskId[0]) return FALSE;
    TodoTask t;
    memset(&t, 0, sizeof(t));
    if (!TodoStore_FindById(s_pomoTaskId, &t)) {
        s_pomoTaskId[0] = '\0';
        return FALSE;
    }
    return TRUE;
}
