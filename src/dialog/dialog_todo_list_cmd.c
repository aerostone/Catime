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
#include "todo/todo_stickies.h"
#include "todo/todo_sync.h"

#include "dialog/dialog_todo_list_state.h"

TodoDlgState *TodoDlg_State(void);
void TodoDlg_RefreshList(HWND hdlg);
void TodoDlg_ReadFilter(HWND hdlg, TodoFilter *f);

static TodoTask *SelectedTask(void) {
    TodoDlgState *s = TodoDlg_State();
    if (!s || s->selected < 0 || s->selected >= s->rowCount) return NULL;
    return &s->rows[s->selected];
}

void TodoDlg_OnAdd(HWND hdlg) {
    wchar_t wt[TODO_STORE_TITLE_LEN], wd[TODO_STORE_DATE_LEN];
    GetDlgItemTextW(hdlg, IDC_TODO_NEW_EDIT, wt, _countof(wt));
    GetDlgItemTextW(hdlg, IDC_TODO_NEW_DUE, wd, _countof(wd));
    if (!wt[0]) {
        MessageBoxW(hdlg,
            GetLocalizedString(L"\u8bf7\u8f93\u5165\u4efb\u52a1\u6807\u9898",
                               L"Enter a task title"),
            GetLocalizedString(L"TODO", L"TODO"), MB_ICONINFORMATION);
        return;
    }
    char title[TODO_STORE_TITLE_LEN] = "", due[TODO_STORE_DATE_LEN] = "";
    WideCharToMultiByte(CP_UTF8, 0, wt, -1, title, sizeof(title), NULL, NULL);
    if (wd[0])
        WideCharToMultiByte(CP_UTF8, 0, wd, -1, due, sizeof(due), NULL, NULL);
    LRESULT imp = SendDlgItemMessageW(hdlg, IDC_TODO_NEW_IMPORTANCE,
                                      CB_GETCURSEL, 0, 0);
    TodoImportance level = TODO_IMPORTANCE_NONE;
    if (imp == 1) level = TODO_IMPORTANCE_LOW;
    else if (imp == 2) level = TODO_IMPORTANCE_MEDIUM;
    else if (imp == 3) level = TODO_IMPORTANCE_HIGH;
    if (!TodoStore_Add(title, level, due)) {
        MessageBoxW(hdlg,
            GetLocalizedString(L"\u6dfb\u52a0\u5931\u8d25\uff08\u65e5\u671f\u683c\u5f0f YYYY-MM-DD\uff1f\uff09",
                               L"Add failed (date format YYYY-MM-DD?)"),
            GetLocalizedString(L"TODO", L"TODO"), MB_ICONWARNING);
        return;
    }
    SetDlgItemTextW(hdlg, IDC_TODO_NEW_EDIT, L"");
    TodoDlg_RefreshList(hdlg);
}

void TodoDlg_OnToggleDone(HWND hdlg) {
    TodoTask *t = SelectedTask();
    if (!t) return;
    if (t->source == TODO_SOURCE_LOCAL ||
        (t->id[0] == 'C' && t->id[1] == ':')) {
        TodoStore_SetDone(t->id, !t->done);
    } else {
        /* V: view-only: nothing actionable */
    }
    TodoDlg_RefreshList(hdlg);
}

void TodoDlg_OnDelete(HWND hdlg) {
    TodoTask *t = SelectedTask();
    if (!t) return;
    if (t->source == TODO_SOURCE_LOCAL ||
        (t->id[0] == 'C' && t->id[1] == ':')) {
        int rc = MessageBoxW(hdlg,
            GetLocalizedString(
                L"\u5220\u9664\u540e\u5c06\u540c\u6b65\u5220\u9664\u670d\u52a1\u7aef\u4efb\u52a1\uff0c\u786e\u8ba4\u5220\u9664\uff1f",
                L"Delete will also remove the server task on next sync. Delete?"),
            GetLocalizedString(L"TODO", L"TODO"),
            MB_OKCANCEL | MB_ICONWARNING);
        if (rc != IDOK) return;
        char id[TODO_STORE_ID_LEN];
        strcpy_s(id, sizeof(id), t->id);
        TodoStickies_Forget(id);
        TodoStore_Remove(id);
        TodoDlg_RefreshList(hdlg);
    } else {
        MessageBoxW(hdlg,
            GetLocalizedString(L"\u8be5\u884c\u4e3a\u53ea\u8bfb\u89c6\u56fe\uff0c\u65e0\u53ef\u64cd\u4f5c\u4efb\u52a1",
                               L"This row is a read-only view"),
            GetLocalizedString(L"TODO", L"TODO"), MB_ICONINFORMATION);
    }
}

void TodoDlg_OnPin(HWND hdlg) {
    TodoTask *t = SelectedTask();
    if (!t) return;
    if (t->source != TODO_SOURCE_LOCAL) {
        MessageBoxW(hdlg,
            GetLocalizedString(L"\u53ea\u6709\u672c\u5730\u4efb\u52a1\u53ef\u4ee5\u7f6e\u9876\u4e3a\u4fbf\u7b3e",
                               L"Only local tasks can be pinned as stickies"),
            GetLocalizedString(L"TODO", L"TODO"), MB_ICONINFORMATION);
        return;
    }
    BOOL pinned = TodoSticky_IsPinned(t->id);
    TodoSticky_SetPinned(t->id, !pinned);
    TodoDlg_RefreshList(hdlg);
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
            extern void TodoConflict_OpenDir(HWND hwnd);
            TodoConflict_OpenDir(hdlg);
        }
        s->selected = -1;
        SendMessageW(list, LB_SETCURSEL, (WPARAM)-1, 0);
        return;
    }
    if (listIndex >= 0 && listIndex < s->rowCount)
        s->selected = listIndex;
    else
        s->selected = -1;
    (void)hdlg;
}
