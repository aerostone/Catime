/**
 * @file dialog_todo_list_cmd.c
 * @brief TODO list dialog commands: add/done/delete/pin/selection.
 *
 * Split from dialog_todo_list.c to respect the 300-line gate.
 * Operates on the shared dialog snapshot owned by dialog_todo_list.c.
 * Delete confirms (D3) and states the server-side consequence.
 */
#include <string.h>

#include "../../resource/resource.h"
#include "language.h"
#include "todo/todo_store.h"
#include "todo/todo_sync.h"

#include "dialog/dialog_todo.h"
#include "dialog/dialog_todo_list_state.h"
#include "todo/todo_conflict.h"

#include "dialog/dialog_todo_parts.h"
#include "todo/todo_board.h"
#include "todo/todo_stickies.h"
void TodoDlg_RefreshList(HWND hdlg);


static TodoTask *SelectedTask(void) {
    TodoDlgState *s = TodoDlg_State();
    if (!s || s->selected < 0 || s->selected >= s->rowCount) return NULL;
    return &s->rows[s->selected];
}

void TodoDlg_OnAdd(HWND hdlg) {
    wchar_t wt[TODO_STORE_TITLE_LEN];
    char due[TODO_STORE_DATE_LEN] = "";
    GetDlgItemTextW(hdlg, IDC_TODO_NEW_EDIT, wt, _countof(wt));
    TodoDlg_GetDue(hdlg, due, sizeof(due)); /* RD8: date picker */
    if (!wt[0]) {
        MessageBoxW(hdlg,
            GetLocalizedString(L"\u8bf7\u8f93\u5165\u4efb\u52a1\u6807\u9898",
                               L"Enter a task title"),
            GetLocalizedString(L"TODO", L"TODO"), MB_ICONINFORMATION);
        return;
    }
    char title[TODO_STORE_TITLE_LEN] = "";
    WideCharToMultiByte(CP_UTF8, 0, wt, -1, title, sizeof(title), NULL, NULL);
    LRESULT imp = SendDlgItemMessageW(hdlg, IDC_TODO_NEW_IMPORTANCE,
                                      CB_GETCURSEL, 0, 0);
    TodoImportance level = TODO_IMPORTANCE_NONE;
    if (imp == 1) level = TODO_IMPORTANCE_LOW;
    else if (imp == 2) level = TODO_IMPORTANCE_MEDIUM;
    else if (imp == 3) level = TODO_IMPORTANCE_HIGH;
    char board[TODO_STORE_BOARD_LEN * 2];
    TodoDlg_GetBoard(hdlg, board, sizeof(board));
    /* adding on the sync book creates the task on tweek (next push) */
    BOOL intoSync = (strcmp(board, TODO_BOARD_SYNC) == 0);
    if (!TodoStore_AddTo(title, level, due, board)) {
        MessageBoxW(hdlg,
            GetLocalizedString(L"\u6dfb\u52a0\u5931\u8d25\uff08\u65e5\u671f\u683c\u5f0f YYYY-MM-DD\uff1f\uff09",
                               L"Add failed (date format YYYY-MM-DD?)"),
            GetLocalizedString(L"TODO", L"TODO"), MB_ICONWARNING);
        return;
    }
    SetDlgItemTextW(hdlg, IDC_TODO_NEW_EDIT, L"");
    { /* RD7: the new row stays visible and selected */
        char nid[TODO_STORE_ID_LEN] = "";
        TodoStore_LastAddedId(nid, sizeof(nid));
        if (nid[0]) {
            if (intoSync) TodoStore_MarkSynced(nid);
            TodoDlg_Preselect(nid);
        }
    }
    TodoStickies_RefreshAll();
    TodoDlg_RefreshList(hdlg);
}

/* Every listed row lives in the local store, so all rows are editable:
 * sync rows carry a serverId, and a local edit is pushed back to tweek on
 * the next round (newer-wins by updated_at). */
void TodoDlg_OnToggleDone(HWND hdlg) {
    TodoTask *t = SelectedTask();
    if (!t) return;
    TodoStore_SetDone(t->id, !t->done);
    TodoStickies_RefreshAll();
    TodoDlg_RefreshList(hdlg);
}

void TodoDlg_OnDelete(HWND hdlg) {
    TodoTask *t = SelectedTask();
    if (!t) return;
    /* only server-backed rows warn about the v -> server tombstone */
    int rc;
    if (t->source == TODO_SOURCE_SYNC) {
        rc = MessageBoxW(hdlg,
            GetLocalizedString(
                L"\u5220\u9664\u540e\u5c06\u540c\u6b65\u5220\u9664\u670d\u52a1\u7aef\u4efb\u52a1\uff0c\u786e\u8ba4\u5220\u9664\uff1f",
                L"Delete will also remove the server task on next sync. Delete?"),
            GetLocalizedString(L"TODO", L"TODO"),
            MB_OKCANCEL | MB_ICONWARNING);
    } else {
        rc = MessageBoxW(hdlg,
            GetLocalizedString(L"\u786e\u8ba4\u5220\u9664\u8be5\u4efb\u52a1\uff1f",
                               L"Delete this task?"),
            GetLocalizedString(L"TODO", L"TODO"),
            MB_OKCANCEL | MB_ICONWARNING);
    }
    if (rc != IDOK) return;
    char id[TODO_STORE_ID_LEN];
    strcpy_s(id, sizeof(id), t->id);
    TodoStore_Remove(id);
    TodoStickies_RefreshAll();
    TodoDlg_RefreshList(hdlg);
}

/* RD9: selecting a row fills the edit row (title/date/priority). */
static void FillEditRow(HWND hdlg, const TodoTask *t) {
    wchar_t w[TODO_STORE_TITLE_LEN];
    MultiByteToWideChar(CP_UTF8, 0, t->title, -1, w, TODO_STORE_TITLE_LEN);
    SetDlgItemTextW(hdlg, IDC_TODO_NEW_EDIT, w);
    TodoDlg_SetDue(hdlg, t->dueDate); /* RD8: date picker */
    int idx = t->importance == TODO_IMPORTANCE_HIGH ? 3 :
              t->importance == TODO_IMPORTANCE_MEDIUM ? 2 :
              t->importance == TODO_IMPORTANCE_LOW ? 1 : 0;
    SendDlgItemMessageW(hdlg, IDC_TODO_NEW_IMPORTANCE, CB_SETCURSEL,
                        (WPARAM)idx, 0);
}

void TodoDlg_OnSelect(HWND hdlg, int listIndex) {
    TodoDlgState *s = TodoDlg_State();
    if (!s) return;
    /* placeholder rows (empty-state/conflict hint) are not selectable */
    HWND list = GetDlgItem(hdlg, IDC_TODO_LIST_VIEW);
    int lbCount = (int)SendMessageW(list, LB_GETCOUNT, 0, 0);
    if (lbCount > s->rowCount && listIndex >= s->rowCount) {
        /* clicked a hint row: conflict hint opens the dir */
        wchar_t wb[256];
        SendMessageW(list, LB_GETTEXT, (WPARAM)listIndex, (LPARAM)wb);
        if (wcsstr(wb, L"\u51b2\u7a81") || wcsstr(wb, L"conflict")) {
            TodoConflict_OpenDir(hdlg);
        }
        s->selected = -1;
        SendMessageW(list, LB_SETCURSEL, (WPARAM)-1, 0);
        return;
    }
    if (listIndex >= 0 && listIndex < s->rowCount) {
        s->selected = listIndex;
        FillEditRow(hdlg, &s->rows[listIndex]); /* RD9 */
    } else {
        s->selected = -1;
    }
}

/* RD9: save the edit row back into the selected task. */
void TodoDlg_OnSaveEdit(HWND hdlg) {
    TodoTask *t = SelectedTask();
    if (!t) {
        MessageBoxW(hdlg,
            GetLocalizedString(L"\u5148\u9009\u62E9\u4E00\u4E2A\u4EFB\u52A1",
                               L"Select a task first"),
            L"TODO", MB_ICONINFORMATION);
        return;
    }
    wchar_t wt[TODO_STORE_TITLE_LEN];
    char title[TODO_STORE_TITLE_LEN] = "", due[TODO_STORE_DATE_LEN] = "";
    GetDlgItemTextW(hdlg, IDC_TODO_NEW_EDIT, wt, _countof(wt));
    if (wt[0])
        WideCharToMultiByte(CP_UTF8, 0, wt, -1, title, sizeof(title),
                            NULL, NULL);
    TodoDlg_GetDue(hdlg, due, sizeof(due)); /* RD8: date picker */
    LRESULT imp = SendDlgItemMessageW(hdlg, IDC_TODO_NEW_IMPORTANCE,
                                      CB_GETCURSEL, 0, 0);
    TodoImportance level = TODO_IMPORTANCE_NONE;
    if (imp == 1) level = TODO_IMPORTANCE_LOW;
    else if (imp == 2) level = TODO_IMPORTANCE_MEDIUM;
    else if (imp == 3) level = TODO_IMPORTANCE_HIGH;
    if (title[0]) TodoStore_SetTitle(t->id, title);
    TodoStore_SetDueDate(t->id, due);
    TodoStore_SetImportance(t->id, level);
    TodoStickies_RefreshAll();
    TodoDlg_RefreshList(hdlg);
}
