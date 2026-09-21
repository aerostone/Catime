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
#include "todo/todo_board.h"
#include "todo/todo_store.h"
#include "todo/todo_stickies.h"
#include "todo/todo_sync.h"
#include "dialog/dialog_todo_parts.h"
#include "todo/todo_rowmark.h"
#include "todo/todo_sync_status.h"
#include "todo/todo_conflict.h"
#include "todo/todo_ui_debug.h"


#define TODO_KW_TIMER 7701
#define TODO_KW_DEBOUNCE_MS 200

static TodoDlgState s_state;
static char s_preselect[TODO_STORE_ID_LEN] = "";
static char s_board[TODO_STORE_BOARD_LEN] = ""; /* board the dialog edits */

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


void TodoDlg_ReadFilterImpl(HWND hdlg, TodoFilter *f) {
    TodoFilter_InitDefault(f);
    LRESULT scope = SendDlgItemMessageW(hdlg, IDC_TODO_FILTER_DUE_SCOPE,
                                        CB_GETCURSEL, 0, 0);
    if (scope == 1) f->dueScope = TODO_DUE_SCOPE_MONTH;
    else if (scope == 2) f->dueScope = TODO_DUE_SCOPE_CUSTOM;
    else if (scope == 3) f->dueScope = TODO_DUE_SCOPE_ALL;
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
        TodoDlg_ApplyScopeRange(f);
    }
    wchar_t kw[TODO_STORE_TITLE_LEN];
    GetDlgItemTextW(hdlg, IDC_TODO_FILTER_KEYWORD, kw, _countof(kw));
    if (kw[0])
        WideCharToMultiByte(CP_UTF8, 0, kw, -1, f->keyword,
                            sizeof(f->keyword), NULL, NULL);
    f->showDone = IsDlgButtonChecked(hdlg, IDC_TODO_SHOW_DONE) == BST_CHECKED;
    /* board selection wins over the source combo (mutually exclusive) */
    TodoDlg_ApplyBoardFilter(hdlg, f);
}

/* Exported for dialog_todo_list_cmd.c: current UI filter snapshot. */
void TodoDlg_ReadFilter(HWND hdlg, TodoFilter *f) {
    TodoDlg_ReadFilterImpl(hdlg, f);
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
        TodoDlg_InitCombos(hdlg);
        {
            int bi = TodoBoard_IndexByName(s_board);
            TodoDlg_InitBoardCombo(hdlg);
            if (bi >= 0) {
                HWND cb = GetDlgItem(hdlg, IDC_TODO_BOARD_SEL);
                if (cb)
                    SendMessageW(cb, CB_SETCURSEL, (WPARAM)bi, 0);
            }
            TodoDlg_SyncStickyButton(hdlg);
            TodoDlg_SyncScopeCombo(hdlg);
            TodoDlg_RefreshAddGate(hdlg);
            s_board[0] = '\0';
        }
        EnableWindow(GetDlgItem(hdlg, IDC_TODO_FILTER_DUE_FROM), FALSE);
        EnableWindow(GetDlgItem(hdlg, IDC_TODO_FILTER_DUE_TO), FALSE);
        TodoDlg_RefreshInto(hdlg);
        TodoUiDebug_DumpDialog(hdlg, "todo-list");
        return TRUE;
    case WM_TIMER:
        if (wp == TODO_KW_TIMER) {
            KillTimer(hdlg, TODO_KW_TIMER);
            TodoDlg_RefreshInto(hdlg);
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
        if (TodoDlg_BoardCommand(hdlg, id, code)) return TRUE;
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
        if (id == IDC_TODO_SAVE_BUTTON) {
            TodoDlg_OnSaveEdit(hdlg); /* RD9 */
            return TRUE;
        }
        if (id == IDC_TODO_SYNC_NOW_BTN) {
            TodoSync_PollNow();
            TodoDlg_RefreshInto(hdlg);
            return TRUE;
        }
        if (id == IDC_TODO_CONFLICT_BTN) {
            TodoConflict_OpenDir(hdlg);
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
                /* the board owns its scope: keep card and list in sync */
                TodoDlg_PersistScope(hdlg, (TodoDueScope)(
                    sel == 1 ? TODO_DUE_SCOPE_MONTH :
                    sel == 2 ? TODO_DUE_SCOPE_CUSTOM :
                    sel == 3 ? TODO_DUE_SCOPE_ALL : TODO_DUE_SCOPE_WEEK));
            }
            TodoDlg_RefreshInto(hdlg);
            return TRUE;
        }
        /* B3: date edits commit on focus-loss, keyword debounced live */
        if (id == IDC_TODO_FILTER_KEYWORD && code == EN_CHANGE) {
            ArmKeywordTimer(hdlg);
            return TRUE;
        }
        if ((id == IDC_TODO_FILTER_DUE_FROM || id == IDC_TODO_FILTER_DUE_TO) &&
            code == EN_KILLFOCUS) {
            TodoDlg_RefreshInto(hdlg);
            return TRUE;
        }
        if (id == IDC_TODO_SHOW_DONE && code == BN_CLICKED) {
            TodoDlg_RefreshInto(hdlg);
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
    TodoDlg_RefreshInto(hdlg);
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
void ShowTodoListDialogForBoard(HWND hwndParent, const char *board,
                                BOOL addNew) {
    if (board && board[0])
        strcpy_s(s_board, sizeof(s_board), board);
    ShowTodoListDialog(hwndParent);
    HWND ex = Dialog_GetInstance(DIALOG_INSTANCE_TODO_LIST);
    if (ex && addNew) {
        HWND edit = GetDlgItem(ex, IDC_TODO_NEW_EDIT);
        if (edit) SetFocus(edit);
    }
}

void ShowTodoListDialogForNew(HWND hwndParent) {
    ShowTodoListDialogForBoard(hwndParent, NULL, TRUE);
}
