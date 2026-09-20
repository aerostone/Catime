/**
 * @file tray_menu_todo.c
 * @brief TODO submenu: open tasks, quick actions, settings entry.
 *
 * Layout: "TODO (n)" popup -> header info row, top 5 open tasks
 * (click = jump to list with row preselected, never destructive),
 * then New Sticky / Task list... / Sync state / Conflicts /
 * Sync now / Settings. Row marks use TodoRowMark ([A]/[B]/[C]).
 */
#include <stdio.h>
#include <string.h>

#include "tray/tray_menu_todo.h"
#include "../../resource/resource.h"
#include "language.h"
#include "todo/todo_store.h"
#include "todo/todo_sync.h"
#include "todo/todo_rowmark.h"
#include "todo/todo_conflict.h"
#include "todo/todo_sync_status.h"

#include "window_procedure/window_commands.h"

#define TODO_MENU_CONFLICT_BASE 5700

void ShowTodoListDialog(HWND hwndParent);
void TodoDlg_Preselect(const char *taskId);

static void RowLabel(const TodoTask *t, wchar_t *out, size_t cap) {
    char row[256];
#if defined(_MSC_VER)
    _snprintf_s(row, sizeof(row), _TRUNCATE, "%s %s%s",
                TodoRowMark(t), t->title,
                t->done ? "" : "");
#else
    snprintf(row, sizeof(row), "%s %s", TodoRowMark(t), t->title);
#endif
    wchar_t wr[256];
    if (MultiByteToWideChar(CP_UTF8, 0, row, -1, wr, _countof(wr)))
#if defined(_MSC_VER)
        wcsncpy_s(out, cap, wr, _TRUNCATE);
#else
        wcsncpy(out, wr, cap);
#endif
    else
        out[0] = L'\0';
    if (cap) out[cap - 1] = L'\0';
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

    /* header: info only, disabled */
    wchar_t head[128];
#if defined(_MSC_VER)
    _snwprintf_s(head, _countof(head), _TRUNCATE, L"%d open \u00b7 click a task to open the list",
                 open);
#else
    swprintf(head, 128, L"%d open \u00b7 click a task to open the list", open);
#endif
    AppendMenuW(hTodo, MF_STRING | MF_DISABLED | MF_GRAYED, 0, head);
    AppendMenuW(hTodo, MF_SEPARATOR, 0, NULL);

    for (int i = 0; i < n; i++) {
        wchar_t item[160];
        RowLabel(&tasks[i], item, _countof(item));
        AppendMenuW(hTodo, MF_STRING, TODO_MENU_TASK_BASE + i, item);
    }
    if (n > 0) AppendMenuW(hTodo, MF_SEPARATOR, 0, NULL);

    AppendMenuW(hTodo, MF_STRING, CLOCK_IDM_TODO_NEW_STICKY,
                GetLocalizedString(L"\u65b0\u5efa\u4efb\u52a1\u2026", L"New Task..."));
    AppendMenuW(hTodo, MF_STRING, CLOCK_IDM_TODO_LIST,
                GetLocalizedString(L"\u4efb\u52a1\u5217\u8868\u2026", L"Task List..."));
    AppendMenuW(hTodo, MF_SEPARATOR, 0, NULL);
    wchar_t stLabel[96];
    TodoSyncStatus_Label(stLabel, _countof(stLabel));
    AppendMenuW(hTodo, MF_STRING | MF_DISABLED | MF_GRAYED, 0, stLabel);
    int conflicts = TodoConflict_Count();
    if (conflicts > 0) {
        wchar_t cf[96];
#if defined(_MSC_VER)
        _snwprintf_s(cf, _countof(cf), _TRUNCATE, L"\u51b2\u7a81 (%d) \u00b7 \u6253\u5f00\u76ee\u5f55",
                     conflicts);
#else
        swprintf(cf, 96, L"\u51b2\u7a81 (%d) \u00b7 \u6253\u5f00\u76ee\u5f55", conflicts);
#endif
        AppendMenuW(hTodo, MF_STRING, TODO_MENU_CONFLICT_BASE, cf);
    }
    AppendMenuW(hTodo, MF_STRING, CLOCK_IDM_TODO_SYNC_NOW,
                GetLocalizedString(L"\u7acb\u5373\u540c\u6b65", L"Sync Now"));
    AppendMenuW(hTodo, MF_STRING, CLOCK_IDM_TODO_SETTINGS,
                GetLocalizedString(L"TODO \u8bbe\u7f6e\u2026", L"TODO Settings..."));

    wchar_t label[64];
    int nConf = TodoConflict_Count();
    if (nConf > 0)
#if defined(_MSC_VER)
        _snwprintf_s(label, _countof(label), _TRUNCATE, L"%s (%d)(!)",
                     GetLocalizedString(L"TODO", L"TODO"), open);
#else
        swprintf(label, 64, L"%s (%d)(!)", GetLocalizedString(L"TODO", L"TODO"), open);
#endif
    else
#if defined(_MSC_VER)
        _snwprintf_s(label, _countof(label), _TRUNCATE, L"%s (%d)",
                     GetLocalizedString(L"TODO", L"TODO"), open);
#else
        swprintf(label, 64, L"%s (%d)", GetLocalizedString(L"TODO", L"TODO"), open);
#endif
    if (!AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTodo, label)) {
        DestroyMenu(hTodo);
    }
}

/* Range handler: top-5 task row click -> open list preselected (A1 fix).
 * Never destructive from the tray: completion lives in list/sticky. */
BOOL HandleTodoMenuRow(HWND hwnd, UINT cmd, int index) {
    (void)cmd;
    TodoFilter f;
    TodoFilter_InitDefault(&f);
    f.showDone = FALSE;
    TodoTask tasks[TODO_MENU_TASK_COUNT];
    int n = TodoStore_Query(&f, tasks, TODO_MENU_TASK_COUNT);
    if (index < 0 || index >= n) return TRUE;
    TodoDlg_Preselect(tasks[index].id);
    if (hwnd) ShowTodoListDialog(hwnd);
    return TRUE;
}

/* Conflict row: open the todo dir in explorer. */
BOOL HandleTodoMenuConflict(HWND hwnd, UINT cmd, int index) {
    (void)cmd; (void)index;
    TodoConflict_OpenDir(hwnd);
    return TRUE;
}

UINT TodoMenu_ConflictId(void) {
    return TODO_MENU_CONFLICT_BASE;
}
