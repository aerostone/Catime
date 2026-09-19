/**
 * @file dialog_todo_list.c
 * @brief TODO task list dialog: filter + list + add/done/delete/pin.
 *
 * Layout (see resource/todo_dialog.rc header): GROUPBOX time-range +
 * state/search on row 1; list y=46; add row y=258; actions y=278.
 * Row marks use TodoRowMark ([A]/[B]/[C]/[x]); overdue renders as [A].
 * Date edits commit on focus-loss/Enter (B3: no per-keystroke refresh);
 * keyword keeps live filter with 200ms debounce. Custom scope reveals
 * the due-range edits only (done-range lives in the filter struct and
 * is accepted via keyword "done:YYYY-MM-DD" for power users).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dialog/dialog_todo.h"
#include "../../resource/resource.h"
#include "dialog/dialog_registry.h"
#include "language.h"
#include "todo/todo_store.h"
#include "todo/todo_stickies.h"
#include "todo/todo_sync.h"
#include "todo/todo_rowmark.h"
#include "todo/todo_sync_status.h"
#include "todo/todo_conflict.h"
#include "todo/todo_ui_debug.h"

#include "dialog/dialog_todo_list_state.h"

#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER (0x1501)
#endif
#ifndef EM_SETCUEBANNER_W
#define EM_SETCUEBANNER_W EM_SETCUEBANNER
#endif

#define TODO_KW_TIMER 7701
#define TODO_KW_DEBOUNCE_MS 200

static TodoDlgState s_state;
static char s_preselect[TODO_STORE_ID_LEN] = "";

char *TodoDlg_PreselectBuf(void) {
    return s_preselect;
}

void TodoDlg_Preselect(const char *taskId) {
    if (!taskId) { s_preselect[0] = '\0'; return; }
#if defined(_MSC_VER)
    strcpy_s(s_preselect, sizeof(s_preselect), taskId);
#else
    snprintf(s_preselect, sizeof(s_preselect), "%s", taskId);
#endif
}

TodoDlgState *TodoDlg_State(void) {
    return &s_state;
}

static void ApplyScopeRange(TodoFilter *f) {
    extern void TodoDlg_ApplyScopeRange(TodoFilter *f);
    TodoDlg_ApplyScopeRange(f);
}

static void ReadFilterFromUI(HWND hdlg, TodoFilter *f) {
    TodoFilter_InitDefault(f);
    LRESULT scope = SendDlgItemMessageW(hdlg, IDC_TODO_FILTER_DUE_SCOPE,
                                        CB_GETCURSEL, 0, 0);
    if (scope == 1) f->dueScope = TODO_DUE_SCOPE_MONTH;
    else if (scope == 2) f->dueScope = TODO_DUE_SCOPE_CUSTOM;
    else f->dueScope = TODO_DUE_SCOPE_WEEK;
    LRESULT src = SendDlgItemMessageW(hdlg, IDC_TODO_FILTER_SOURCE,
                                      CB_GETCURSEL, 0, 0);
    if (src == 1) f->showSync = FALSE;
    else if (src == 2) f->showLocal = FALSE;
    LRESULT imp = SendDlgItemMessageW(hdlg, IDC_TODO_FILTER_IMPORTANCE,
                                      CB_GETCURSEL, 0, 0);
    if (imp == 1) f->minImportance = TODO_IMPORTANCE_LOW;
    else if (imp == 2) f->minImportance = TODO_IMPORTANCE_MEDIUM;
    else if (imp == 3) f->minImportance = TODO_IMPORTANCE_HIGH;
    wchar_t w[TODO_STORE_DATE_LEN];
    char u[TODO_STORE_DATE_LEN];
    if (f->dueScope == TODO_DUE_SCOPE_CUSTOM) {
        GetDlgItemTextW(hdlg, IDC_TODO_FILTER_DUE_FROM, w, _countof(w));
        if (w[0]) {
            WideCharToMultiByte(CP_UTF8, 0, w, -1, u, sizeof(u), NULL, NULL);
            strcpy_s(f->fromDate, sizeof(f->fromDate), u);
        }
        GetDlgItemTextW(hdlg, IDC_TODO_FILTER_DUE_TO, w, _countof(w));
        if (w[0]) {
            WideCharToMultiByte(CP_UTF8, 0, w, -1, u, sizeof(u), NULL, NULL);
            strcpy_s(f->toDate, sizeof(f->toDate), u);
        }
    } else {
        ApplyScopeRange(f);
    }
    wchar_t kw[TODO_STORE_TITLE_LEN];
    GetDlgItemTextW(hdlg, IDC_TODO_FILTER_KEYWORD, kw, _countof(kw));
    if (kw[0])
        WideCharToMultiByte(CP_UTF8, 0, kw, -1, f->keyword,
                            sizeof(f->keyword), NULL, NULL);
    f->showDone = IsDlgButtonChecked(hdlg, IDC_TODO_SHOW_DONE) == BST_CHECKED;
}

/* Exported for dialog_todo_list_cmd.c: current UI filter snapshot. */
void TodoDlg_ReadFilter(HWND hdlg, TodoFilter *f) {
    ReadFilterFromUI(hdlg, f);
}

void TodoDlg_RefreshList(HWND hdlg);

static void RefreshList(HWND hdlg) {
    extern void TodoDlg_RefreshInto(HWND hdlg);
    TodoDlg_RefreshInto(hdlg);
}

void TodoDlg_OnAdd(HWND hdlg);
void TodoDlg_OnToggleDone(HWND hdlg);
void TodoDlg_OnDelete(HWND hdlg);
void TodoDlg_OnPin(HWND hdlg);
void TodoDlg_OnSelect(HWND hdlg, int listIndex);
void ShowTodoSettingsDialog(HWND hwndParent);

static void InitCombos(HWND hdlg) {
    extern void TodoDlg_InitCombos(HWND hdlg);
    TodoDlg_InitCombos(hdlg);
}

static void ArmKeywordTimer(HWND hdlg) {
    KillTimer(hdlg, TODO_KW_TIMER);
    SetTimer(hdlg, TODO_KW_TIMER, TODO_KW_DEBOUNCE_MS, NULL);
}

static INT_PTR CALLBACK TodoListProc(HWND hdlg, UINT msg,
                                     WPARAM wp, LPARAM lp) {
    (void)lp;
    switch (msg) {
    case WM_INITDIALOG:
        Dialog_InitializeInstance(DIALOG_INSTANCE_TODO_LIST, hdlg);
        TodoFilter_InitDefault(&s_state.filter);
        InitCombos(hdlg);
        EnableWindow(GetDlgItem(hdlg, IDC_TODO_FILTER_DUE_FROM), FALSE);
        EnableWindow(GetDlgItem(hdlg, IDC_TODO_FILTER_DUE_TO), FALSE);
        RefreshList(hdlg);
        TodoUiDebug_DumpDialog(hdlg, "todo-list");
        return TRUE;
    case WM_TIMER:
        if (wp == TODO_KW_TIMER) {
            KillTimer(hdlg, TODO_KW_TIMER);
            RefreshList(hdlg);
            return TRUE;
        }
        break;
    case WM_COMMAND: {
        WORD id = LOWORD(wp);
        WORD code = HIWORD(wp);
        if (id == IDOK || id == IDCANCEL) {
            DestroyWindow(hdlg);
            return TRUE;
        }
        if (id == IDC_TODO_ADD_BUTTON) {
            TodoDlg_OnAdd(hdlg);
            return TRUE;
        }
        if (id == IDC_TODO_DONE_BUTTON) {
            TodoDlg_OnToggleDone(hdlg);
            return TRUE;
        }
        if (id == IDC_TODO_DELETE_BUTTON) {
            TodoDlg_OnDelete(hdlg);
            return TRUE;
        }
        if (id == IDC_TODO_PIN_BUTTON) {
            TodoDlg_OnPin(hdlg);
            return TRUE;
        }
        if (id == IDC_TODO_SYNC_NOW_BTN) {
            if (TodoConflict_Count() > 0)
                TodoConflict_OpenDir(hdlg);
            else {
                TodoSync_PollNow();
                RefreshList(hdlg);
            }
            return TRUE;
        }
        if (id == IDC_TODO_SETTINGS_BTN) {
            ShowTodoSettingsDialog(hdlg);
            return TRUE;
        }
        if (id == IDC_TODO_LIST_VIEW && code == LBN_SELCHANGE) {
            int sel = (int)SendDlgItemMessageW(hdlg, IDC_TODO_LIST_VIEW,
                                               LB_GETCURSEL, 0, 0);
            TodoDlg_OnSelect(hdlg, sel);
            return TRUE;
        }
        if ((id == IDC_TODO_FILTER_SOURCE || id == IDC_TODO_FILTER_IMPORTANCE ||
             id == IDC_TODO_FILTER_DUE_SCOPE) && code == CBN_SELCHANGE) {
            if (id == IDC_TODO_FILTER_DUE_SCOPE) {
                LRESULT sel = SendDlgItemMessageW(hdlg, IDC_TODO_FILTER_DUE_SCOPE,
                                                  CB_GETCURSEL, 0, 0);
                BOOL custom = (sel == 2);
                EnableWindow(GetDlgItem(hdlg, IDC_TODO_FILTER_DUE_FROM), custom);
                EnableWindow(GetDlgItem(hdlg, IDC_TODO_FILTER_DUE_TO), custom);
            }
            RefreshList(hdlg);
            return TRUE;
        }
        /* B3: date edits commit on focus-loss, keyword debounced live */
        if (id == IDC_TODO_FILTER_KEYWORD && code == EN_CHANGE) {
            ArmKeywordTimer(hdlg);
            return TRUE;
        }
        if ((id == IDC_TODO_FILTER_DUE_FROM || id == IDC_TODO_FILTER_DUE_TO) &&
            code == EN_KILLFOCUS) {
            RefreshList(hdlg);
            return TRUE;
        }
        if (id == IDC_TODO_SHOW_DONE && code == BN_CLICKED) {
            RefreshList(hdlg);
            return TRUE;
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(hdlg);
        return TRUE;
    case WM_DESTROY:
        KillTimer(hdlg, TODO_KW_TIMER);
        Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_TODO_LIST, hdlg);
        break;
    }
    return FALSE;
}

void TodoDlg_RefreshList(HWND hdlg) {
    RefreshList(hdlg);
}

void ShowTodoListDialog(HWND hwndParent) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_TODO_LIST)) {
        HWND ex = Dialog_GetInstance(DIALOG_INSTANCE_TODO_LIST);
        if (ex) SetForegroundWindow(ex);
        return;
    }
    HWND hdlg = CreateDialogW(GetModuleHandleW(NULL),
                              MAKEINTRESOURCEW(IDD_TODO_LIST_DIALOG),
                              hwndParent, TodoListProc);
    if (hdlg) ShowWindow(hdlg, SW_SHOW);
}

/* "New Task" focuses the list dialog title edit instead of cloning it. */
void ShowTodoListDialogForNew(HWND hwndParent) {
    ShowTodoListDialog(hwndParent);
    HWND ex = Dialog_GetInstance(DIALOG_INSTANCE_TODO_LIST);
    if (ex) {
        HWND edit = GetDlgItem(ex, IDC_TODO_NEW_EDIT);
        if (edit) SetFocus(edit);
    }
}
