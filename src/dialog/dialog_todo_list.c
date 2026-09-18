/**
 * @file dialog_todo_list.c
 * @brief TODO task list dialog: filter + list + add/done/delete/pin.
 *
 * Filters: source combo (all/local/sync), importance combo (any/low+
 * med+/high), due range (YYYY-MM-DD), done range, keyword, show-done.
 * List rows: "[!] 2026-09-20 title" / "[x]". Selection maps to the
 * last query snapshot. New task: title + optional due + importance.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dialog/dialog_todo.h"
#include "../../resource/resource.h"
#include "dialog/dialog_registry.h"
#include "todo/todo_store.h"
#include "todo/todo_stickies.h"
#include "todo/todo_sync.h"

#include "dialog/dialog_todo_list_state.h"

static TodoDlgState s_state;

TodoDlgState *TodoDlg_State(void) {
    return &s_state;
}

static void ApplyScopeRange(TodoFilter *f) {
    if (f->dueScope == TODO_DUE_SCOPE_CUSTOM) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    if (f->dueScope == TODO_DUE_SCOPE_MONTH) {
        _snprintf_s(f->fromDate, sizeof(f->fromDate), _TRUNCATE,
                    "%04d-%02d-01", st.wYear, st.wMonth);
        int last = 28;
        switch (st.wMonth) {
        case 1: case 3: case 5: case 7: case 8: case 10: case 12:
            last = 31;
            break;
        case 4: case 6: case 9: case 11:
            last = 30;
            break;
        default: {
            BOOL leap = ((st.wYear % 4 == 0 && st.wYear % 100 != 0) ||
                         st.wYear % 400 == 0);
            last = leap ? 29 : 28;
            break;
        }
        }
        _snprintf_s(f->toDate, sizeof(f->toDate), _TRUNCATE,
                    "%04d-%02d-%02d", st.wYear, st.wMonth, last);
        return;
    }
    /* week: Monday..Sunday of the current week */
    int dow = (int)st.wDayOfWeek; /* 0=Sunday */
    int back = (dow == 0) ? 6 : dow - 1;
    int fwd = (dow == 0) ? 0 : 7 - dow;
    FILETIME ft, ft0, ft1;
    SYSTEMTIME base = st;
    base.wHour = 12;
    base.wMinute = 0;
    base.wSecond = 0;
    base.wMilliseconds = 0;
    SystemTimeToFileTime(&base, &ft);
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    ULONGLONG day = 24ULL * 3600ULL * 10000000ULL;
    u.QuadPart -= (ULONGLONG)back * day;
    ft0.dwLowDateTime = u.LowPart;
    ft0.dwHighDateTime = u.HighPart;
    u.QuadPart += (ULONGLONG)(back + fwd) * day;
    ft1.dwLowDateTime = u.LowPart;
    ft1.dwHighDateTime = u.HighPart;
    SYSTEMTIME s0, s1;
    FileTimeToSystemTime(&ft0, &s0);
    FileTimeToSystemTime(&ft1, &s1);
    _snprintf_s(f->fromDate, sizeof(f->fromDate), _TRUNCATE,
                "%04d-%02d-%02d", s0.wYear, s0.wMonth, s0.wDay);
    _snprintf_s(f->toDate, sizeof(f->toDate), _TRUNCATE,
                "%04d-%02d-%02d", s1.wYear, s1.wMonth, s1.wDay);
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
    GetDlgItemTextW(hdlg, IDC_TODO_FILTER_DONE_FROM, w, _countof(w));
    if (w[0]) {
        WideCharToMultiByte(CP_UTF8, 0, w, -1, u, sizeof(u), NULL, NULL);
        strcpy_s(f->doneFrom, sizeof(f->doneFrom), u);
    }
    GetDlgItemTextW(hdlg, IDC_TODO_FILTER_DONE_TO, w, _countof(w));
    if (w[0]) {
        WideCharToMultiByte(CP_UTF8, 0, w, -1, u, sizeof(u), NULL, NULL);
        strcpy_s(f->doneTo, sizeof(f->doneTo), u);
    }
    wchar_t kw[TODO_STORE_TITLE_LEN];
    GetDlgItemTextW(hdlg, IDC_TODO_FILTER_KEYWORD, kw, _countof(kw));
    if (kw[0])
        WideCharToMultiByte(CP_UTF8, 0, kw, -1, f->keyword,
                            sizeof(f->keyword), NULL, NULL);
    f->showDone = IsDlgButtonChecked(hdlg, IDC_TODO_SHOW_DONE) == BST_CHECKED;
}

void TodoDlg_RefreshList(HWND hdlg);

static void RefreshList(HWND hdlg) {
    ReadFilterFromUI(hdlg, &s_state.filter);
    s_state.rowCount = TodoStore_Query(&s_state.filter, s_state.rows,
                                      TODO_DLG_MAX_ROWS);
    s_state.selected = -1;
    HWND list = GetDlgItem(hdlg, IDC_TODO_LIST_VIEW);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < s_state.rowCount; i++) {
        TodoTask *t = &s_state.rows[i];
        char row[256];
        const char *mark = t->done ? "[x]" :
            (t->importance == TODO_IMPORTANCE_HIGH ? "[!]" : "[ ]");
        if (t->dueDate[0])
            _snprintf_s(row, sizeof(row), _TRUNCATE, "%s %s %s%s",
                        mark, t->dueDate, t->title,
                        t->source == TODO_SOURCE_SYNC ? " (sync)" : "");
        else
            _snprintf_s(row, sizeof(row), _TRUNCATE, "%s %s%s",
                        mark, t->title,
                        t->source == TODO_SOURCE_SYNC ? " (sync)" : "");
        wchar_t wr[256];
        if (MultiByteToWideChar(CP_UTF8, 0, row, -1, wr, _countof(wr)))
            SendMessageW(list, LB_ADDSTRING, 0, (LPARAM)wr);
    }
}

void TodoDlg_OnAdd(HWND hdlg);
void TodoDlg_OnToggleDone(HWND hdlg);
void TodoDlg_OnDelete(HWND hdlg);
void TodoDlg_OnPin(HWND hdlg);
void TodoDlg_OnSelect(HWND hdlg, int listIndex);
void ShowTodoSettingsDialog(HWND hwndParent);

static void InitCombos(HWND hdlg) {
    HWND scope = GetDlgItem(hdlg, IDC_TODO_FILTER_DUE_SCOPE);
    SendMessageW(scope, CB_ADDSTRING, 0, (LPARAM)L"Week");
    SendMessageW(scope, CB_ADDSTRING, 0, (LPARAM)L"Month");
    SendMessageW(scope, CB_ADDSTRING, 0, (LPARAM)L"Custom");
    SendMessageW(scope, CB_SETCURSEL, 0, 0);
    HWND src = GetDlgItem(hdlg, IDC_TODO_FILTER_SOURCE);
    SendMessageW(src, CB_ADDSTRING, 0, (LPARAM)L"All");
    SendMessageW(src, CB_ADDSTRING, 0, (LPARAM)L"Local");
    SendMessageW(src, CB_ADDSTRING, 0, (LPARAM)L"Sync");
    SendMessageW(src, CB_SETCURSEL, 0, 0);
    HWND imp = GetDlgItem(hdlg, IDC_TODO_FILTER_IMPORTANCE);
    SendMessageW(imp, CB_ADDSTRING, 0, (LPARAM)L"Any");
    SendMessageW(imp, CB_ADDSTRING, 0, (LPARAM)L"Low+");
    SendMessageW(imp, CB_ADDSTRING, 0, (LPARAM)L"Medium+");
    SendMessageW(imp, CB_ADDSTRING, 0, (LPARAM)L"High");
    SendMessageW(imp, CB_SETCURSEL, 0, 0);
    HWND ni = GetDlgItem(hdlg, IDC_TODO_NEW_IMPORTANCE);
    SendMessageW(ni, CB_ADDSTRING, 0, (LPARAM)L"None");
    SendMessageW(ni, CB_ADDSTRING, 0, (LPARAM)L"Low");
    SendMessageW(ni, CB_ADDSTRING, 0, (LPARAM)L"Medium");
    SendMessageW(ni, CB_ADDSTRING, 0, (LPARAM)L"High");
    SendMessageW(ni, CB_SETCURSEL, 0, 0);
    SetDlgItemTextW(hdlg, IDC_TODO_FILTER_DUE_FROM, L"");
    SetDlgItemTextW(hdlg, IDC_TODO_FILTER_DUE_TO, L"");
    SetDlgItemTextW(hdlg, IDC_TODO_FILTER_DONE_FROM, L"");
    SetDlgItemTextW(hdlg, IDC_TODO_FILTER_DONE_TO, L"");
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
        return TRUE;
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
            TodoSync_PollNow();
            RefreshList(hdlg);
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
        if ((id == IDC_TODO_FILTER_DUE_FROM || id == IDC_TODO_FILTER_DUE_TO ||
             id == IDC_TODO_FILTER_DONE_FROM || id == IDC_TODO_FILTER_DONE_TO ||
             id == IDC_TODO_FILTER_KEYWORD) && code == EN_CHANGE) {
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
