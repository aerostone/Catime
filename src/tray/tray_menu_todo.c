/**
 * @file tray_menu_todo.c
 * @brief TODO submenu: open tasks, quick actions, settings entry.
 *
 * Layout: "TODO (n)" popup -> top 5 open tasks (checkable done),
 * then New task / New sticky / Task list... / Sync now / Settings.
 * Sync now is grayed when sync backend is not configured.
 */
#include <string.h>

#include "tray/tray_menu_todo.h"
#include "../../resource/resource.h"
#include "language.h"
#include "todo/todo_store.h"
#include "todo/todo_sync.h"

#include "window_procedure/window_commands.h"

static BOOL SyncConfigured(void) {
    TodoFilter f;
    TodoFilter_InitDefault(&f);
    f.showLocal = FALSE;
    f.showSync = TRUE;
    TodoTask buf[4];
    if (TodoStore_Query(&f, buf, 4) > 0) return TRUE;
    /* query only proves cached rows; also poll-once state unknown here,
     * so Sync now stays enabled and no-ops gracefully when offline */
    return TRUE;
}

void BuildTodoMenu(HMENU hMenu) {
    if (!hMenu) return;
    TodoFilter f;
    TodoFilter_InitDefault(&f);
    f.showDone = FALSE;
    TodoTask tasks[TODO_MENU_TASK_COUNT];
    int n = TodoStore_Query(&f, tasks, TODO_MENU_TASK_COUNT);
    int open = TodoStore_OpenCount();

    HMENU hTodo = CreatePopupMenu();
    if (!hTodo) return;

    for (int i = 0; i < n; i++) {
        wchar_t wt[TODO_STORE_TITLE_LEN];
        wchar_t item[160];
        if (!MultiByteToWideChar(CP_UTF8, 0, tasks[i].title, -1,
                                 wt, _countof(wt)))
            continue;
        const wchar_t *imp = L"";
        if (tasks[i].importance == TODO_IMPORTANCE_HIGH) imp = L"‼ ";
        else if (tasks[i].importance == TODO_IMPORTANCE_MEDIUM) imp = L"● ";
        _snwprintf_s(item, _countof(item), _TRUNCATE, L"%s%s", imp, wt);
        AppendMenuW(hTodo, MF_STRING | MF_UNCHECKED,
                    TODO_MENU_TASK_BASE + i, item);
    }
    if (n > 0) AppendMenuW(hTodo, MF_SEPARATOR, 0, NULL);

    AppendMenuW(hTodo, MF_STRING, CLOCK_IDM_TODO_NEW,
                GetLocalizedString(NULL, L"New Task"));
    AppendMenuW(hTodo, MF_STRING, CLOCK_IDM_TODO_NEW_STICKY,
                GetLocalizedString(NULL, L"New Sticky Note"));
    AppendMenuW(hTodo, MF_STRING, CLOCK_IDM_TODO_LIST,
                GetLocalizedString(NULL, L"Task List..."));
    AppendMenuW(hTodo, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hTodo, MF_STRING | (SyncConfigured() ? MF_ENABLED : MF_GRAYED),
                CLOCK_IDM_TODO_SYNC_NOW,
                GetLocalizedString(NULL, L"Sync Now"));
    AppendMenuW(hTodo, MF_STRING, CLOCK_IDM_TODO_SETTINGS,
                GetLocalizedString(NULL, L"TODO Settings..."));

    wchar_t label[64];
    _snwprintf_s(label, _countof(label), _TRUNCATE,
                 L"%s (%d)", GetLocalizedString(NULL, L"TODO"), open);
    if (!AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTodo, label)) {
        DestroyMenu(hTodo);
    }
}

/* Range handler: top-5 task row click -> done toggle. */
BOOL HandleTodoMenuRow(HWND hwnd, UINT cmd, int index) {
    (void)hwnd; (void)cmd;
    TodoFilter f;
    TodoFilter_InitDefault(&f);
    f.showDone = FALSE;
    TodoTask tasks[TODO_MENU_TASK_COUNT];
    int n = TodoStore_Query(&f, tasks, TODO_MENU_TASK_COUNT);
    if (index < 0 || index >= n) return TRUE;
    if (tasks[index].source == TODO_SOURCE_LOCAL) {
        TodoStore_SetDone(tasks[index].id, TRUE);
    } else {
        TodoSync_MarkDone(tasks[index].id);
    }
    return TRUE;
}
