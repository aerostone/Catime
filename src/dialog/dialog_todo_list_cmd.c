/**
 * @file dialog_todo_list_cmd.c
 * @brief TODO list dialog commands: add/done/delete/pin/selection.
 *
 * Split from dialog_todo_list.c to respect the 300-line gate.
 * Operates on the shared dialog snapshot owned by dialog_todo_list.c.
 */
#include <string.h>

#include "../../resource/resource.h"
#include "todo/todo_store.h"
#include "todo/todo_stickies.h"
#include "todo/todo_sync.h"

#include "dialog/dialog_todo_list_state.h"

TodoDlgState *TodoDlg_State(void);
void TodoDlg_RefreshList(HWND hdlg);

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
        MessageBoxW(hdlg, L"请输入任务标题", L"TODO", MB_ICONINFORMATION);
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
        MessageBoxW(hdlg, L"添加失败（日期格式 YYYY-MM-DD？）",
                    L"TODO", MB_ICONWARNING);
        return;
    }
    SetDlgItemTextW(hdlg, IDC_TODO_NEW_EDIT, L"");
    TodoDlg_RefreshList(hdlg);
}

void TodoDlg_OnToggleDone(HWND hdlg) {
    TodoTask *t = SelectedTask();
    if (!t) return;
    if (t->source == TODO_SOURCE_LOCAL) {
        TodoStore_SetDone(t->id, !t->done);
    } else {
        TodoSync_MarkDone(t->id);
    }
    TodoDlg_RefreshList(hdlg);
}

void TodoDlg_OnDelete(HWND hdlg) {
    TodoTask *t = SelectedTask();
    if (!t) return;
    if (t->source == TODO_SOURCE_LOCAL) {
        char id[TODO_STORE_ID_LEN];
        strcpy_s(id, sizeof(id), t->id);
        TodoStickies_Forget(id);
        TodoStore_Remove(id);
        TodoDlg_RefreshList(hdlg);
    } else {
        MessageBoxW(hdlg, L"同步任务请在服务端删除", L"TODO", MB_ICONINFORMATION);
    }
}

void TodoDlg_OnPin(HWND hdlg) {
    TodoTask *t = SelectedTask();
    if (!t) return;
    if (t->source != TODO_SOURCE_LOCAL) {
        MessageBoxW(hdlg, L"只有本地任务可以置顶为便签", L"TODO",
                    MB_ICONINFORMATION);
        return;
    }
    BOOL pinned = TodoSticky_IsPinned(t->id);
    TodoSticky_SetPinned(t->id, !pinned);
    TodoDlg_RefreshList(hdlg);
}

void TodoDlg_OnSelect(HWND hdlg, int listIndex) {
    TodoDlgState *s = TodoDlg_State();
    if (!s) return;
    if (listIndex >= 0 && listIndex < s->rowCount)
        s->selected = listIndex;
    else
        s->selected = -1;
    (void)hdlg;
}
