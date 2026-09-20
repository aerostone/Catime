/**
 * @file dialog_todo_refresh.c
 * @brief TODO list refresh: query + row render + empty/conflict hints.
 *
 * Split from dialog_todo_list.c to respect the 300-line gate.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dialog/dialog_todo.h"
#include "../../resource/resource.h"
#include "dialog/dialog_registry.h"
#include "language.h"
#include "todo/todo_store.h"
#include "todo/todo_sync_internal.h"
#include "todo/todo_sync.h"
#include "todo/todo_rowmark.h"
#include "todo/todo_conflict.h"

#include "dialog/dialog_todo_list_state.h"
#include "dialog/dialog_todo_parts.h"

void TodoDlg_RefreshInto(HWND hdlg) {
    TodoDlgState *ps = TodoDlg_State();
#define s_state (*ps)
    char *s_preselect = TodoDlg_PreselectBuf();
    TodoDlg_ReadFilter(hdlg, &s_state.filter);
    {
        /* keep showing freshly added local tasks (the dialog never
         * reloads from disk, so the store snapshot is authoritative) */
        s_state.rowCount = TodoStore_Query(&s_state.filter, s_state.rows,
                                           TODO_DLG_MAX_ROWS);
    }
    HWND list = GetDlgItem(hdlg, IDC_TODO_LIST_VIEW);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    /* keep selection on same id across refreshes (B3 fix) */
    char keepId[TODO_STORE_ID_LEN] = "";
    if (s_state.selected >= 0 && s_state.selected < s_state.rowCount)
        strcpy_s(keepId, sizeof(keepId), s_state.rows[s_state.selected].id);
    if (s_preselect[0]) {
        strcpy_s(keepId, sizeof(keepId), s_preselect);
        s_preselect[0] = '\0';
    }
    s_state.selected = -1;
    for (int i = 0; i < s_state.rowCount; i++) {
        TodoTask *t = &s_state.rows[i];
        char row[320];
        const char *tag = t->source == TODO_SOURCE_SYNC ? " [\u540c\u6b65]"
                                                        : "";
        if (t->dueDate[0])
            _snprintf_s(row, sizeof(row), _TRUNCATE, "%s %s %s%s",
                        TodoRowMark(t), t->dueDate, t->title, tag);
        else
            _snprintf_s(row, sizeof(row), _TRUNCATE, "%s %s%s",
                        TodoRowMark(t), t->title, tag);
        wchar_t wr[320];
        if (MultiByteToWideChar(CP_UTF8, 0, row, -1, wr, _countof(wr))) {
            int idx = (int)SendMessageW(list, LB_ADDSTRING, 0, (LPARAM)wr);
            if (keepId[0] && strcmp(t->id, keepId) == 0 && idx >= 0) {
                SendMessageW(list, LB_SETCURSEL, (WPARAM)idx, 0);
                s_state.selected = i;
            }
        }
    }
    if (s_state.rowCount == 0) {
        /* B7: empty-state placeholder distinguishes no-match vs offline */
        BOOL configured = FALSE;
        {
            BOOL en = FALSE;
            char su[TODO_URL_LEN] = "", st[TODO_TOKEN_LEN] = "";
            int p = 60;
            TodoSync_GetSettings(&en, su, sizeof(su), st, sizeof(st), &p);
            configured = su[0] && st[0];
        }
        const wchar_t *msg = !configured ?
            GetLocalizedString(L"\uff08\u672c\u5468\u65e0\u4efb\u52a1 \u00b7 \u672a\u914d\u7f6e\u540c\u6b65\uff0c\u4ec5\u663e\u793a\u672c\u5730\uff09",
                               L"(No tasks this week - sync not configured, local only)") :
            GetLocalizedString(L"\uff08\u65e0\u5339\u914d\u4efb\u52a1\uff09", L"(No matching tasks)");
        SendMessageW(list, LB_ADDSTRING, 0, (LPARAM)msg);
    }
    if (TodoConflict_Count() > 0) {
        SendMessageW(list, LB_ADDSTRING, 0,
            (LPARAM)GetLocalizedString(
                L"\u26a0 \u6709\u540c\u6b65\u51b2\u7a81\u5feb\u7167\uff0c\u70b9\u51fb Sync \u67e5\u770b\u76ee\u5f55",
                L"! Sync conflicts exist, click Sync to open folder"));
    }
}
#undef s_state
