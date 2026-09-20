/**
 * @file dialog_todo_combos.c
 * @brief TODO list combo/cue/button label init (localized).
 *
 * Split from dialog_todo_list.c to respect the 300-line gate.
 */
#include <windows.h>
#include <stdio.h>

#include "../../resource/resource.h"
#include "language.h"
#include "dialog/dialog_todo_parts.h"

#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER (WM_USER + 1)
#endif
#ifndef EM_SETCUEBANNER_W
#define EM_SETCUEBANNER_W EM_SETCUEBANNER
#endif

void TodoDlg_InitCombos(HWND hdlg) {

    HWND scope = GetDlgItem(hdlg, IDC_TODO_FILTER_DUE_SCOPE);
    SendMessageW(scope, CB_ADDSTRING, 0,
        (LPARAM)GetLocalizedString(L"\u5468", L"Week"));
    SendMessageW(scope, CB_ADDSTRING, 0,
        (LPARAM)GetLocalizedString(L"\u6708", L"Month"));
    SendMessageW(scope, CB_ADDSTRING, 0,
        (LPARAM)GetLocalizedString(L"\u81ea\u5b9a\u4e49", L"Custom"));
    SendMessageW(scope, CB_ADDSTRING, 0,
        (LPARAM)GetLocalizedString(L"\u5168\u90e8", L"All"));
    SendMessageW(scope, CB_SETCURSEL, 0, 0);
    HWND src = GetDlgItem(hdlg, IDC_TODO_FILTER_SOURCE);
    SendMessageW(src, CB_ADDSTRING, 0,
        (LPARAM)GetLocalizedString(L"\u5168\u90e8", L"All"));
    SendMessageW(src, CB_ADDSTRING, 0,
        (LPARAM)GetLocalizedString(L"\u672c\u5730", L"Local"));
    SendMessageW(src, CB_ADDSTRING, 0,
        (LPARAM)GetLocalizedString(L"\u540c\u6b65", L"Sync"));
    SendMessageW(src, CB_SETCURSEL, 0, 0);
    HWND imp = GetDlgItem(hdlg, IDC_TODO_FILTER_IMPORTANCE);
    SendMessageW(imp, CB_ADDSTRING, 0,
        (LPARAM)GetLocalizedString(L"\u4efb\u610f", L"Any"));
    SendMessageW(imp, CB_ADDSTRING, 0, (LPARAM)GetLocalizedString(L"\u4f4e+", L"Low+"));
    SendMessageW(imp, CB_ADDSTRING, 0, (LPARAM)GetLocalizedString(L"\u4e2d+", L"Medium+"));
    SendMessageW(imp, CB_ADDSTRING, 0,
        (LPARAM)GetLocalizedString(L"\u9ad8", L"High"));
    SendMessageW(imp, CB_SETCURSEL, 0, 0);
    HWND ni = GetDlgItem(hdlg, IDC_TODO_NEW_IMPORTANCE);
    SendMessageW(ni, CB_ADDSTRING, 0,
        (LPARAM)GetLocalizedString(L"\u65e0", L"None"));
    SendMessageW(ni, CB_ADDSTRING, 0,
        (LPARAM)GetLocalizedString(L"\u4f4e", L"Low"));
    SendMessageW(ni, CB_ADDSTRING, 0,
        (LPARAM)GetLocalizedString(L"\u4e2d", L"Medium"));
    SendMessageW(ni, CB_ADDSTRING, 0,
        (LPARAM)GetLocalizedString(L"\u9ad8", L"High"));
    SendMessageW(ni, CB_SETCURSEL, 0, 0);
    SendDlgItemMessageW(hdlg, IDC_TODO_FILTER_KEYWORD, EM_SETCUEBANNER_W,
                        TRUE, (LPARAM)GetLocalizedString(
                            L"\u641c\u7d22\u6807\u9898\u2026", L"Search title..."));
    SendDlgItemMessageW(hdlg, IDC_TODO_NEW_EDIT, EM_SETCUEBANNER_W,
                        TRUE, (LPARAM)GetLocalizedString(
                            L"\u65b0\u4efb\u52a1\u6807\u9898", L"New task title"));
    SendDlgItemMessageW(hdlg, IDC_TODO_NEW_DUE, EM_SETCUEBANNER_W,
                        TRUE, (LPARAM)L"YYYY-MM-DD");
    SetDlgItemTextW(hdlg, IDC_TODO_FILTER_DUE_FROM, L"");
    SetDlgItemTextW(hdlg, IDC_TODO_FILTER_DUE_TO, L"");
    SetWindowTextW(GetDlgItem(hdlg, IDC_TODO_ADD_BUTTON),
                   GetLocalizedString(L"\u6dfb\u52a0", L"Add"));
    SetWindowTextW(GetDlgItem(hdlg, IDC_TODO_DONE_BUTTON),
                   GetLocalizedString(L"\u5b8c\u6210", L"Done"));
    SetWindowTextW(GetDlgItem(hdlg, IDC_TODO_BOARD_SHOW),
                   GetLocalizedString(L"\u663e\u793a\u4fbf\u7b7e",
                                      L"Show sticky"));
    SetWindowTextW(GetDlgItem(hdlg, IDC_TODO_DELETE_BUTTON),
                   GetLocalizedString(L"\u5220\u9664", L"Delete"));
    SetWindowTextW(GetDlgItem(hdlg, IDCANCEL),
                   GetLocalizedString(L"\u5173\u95ed", L"Close"));
}

