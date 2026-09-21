/**
 * @file dialog_todo_duepick.c
 * @brief New-task due date picker read/write (SysDateTimePick32).
 *
 * The picker uses DTS_SHOWNONE: unchecked = no date. Split to respect
 * the 300-line gate.
 */
#include <windows.h>
#include <commctrl.h> /* DTM_*/GDT_* for SysDateTimePick32 */
#include <stdio.h>

#include "../../resource/resource.h"
#include "todo/todo_store.h"

void TodoDlg_GetDue(HWND hdlg, char *out, size_t cap) {
    if (out && cap) out[0] = '\0';
    if (!hdlg || !out || cap == 0) return;
    SYSTEMTIME st;
    memset(&st, 0, sizeof(st));
    if (SendDlgItemMessageW(hdlg, IDC_TODO_NEW_DUE, DTM_GETSYSTEMTIME, 0,
                            (LPARAM)&st) != GDT_VALID)
        return; /* unchecked = no date */
#if defined(_MSC_VER)
    _snprintf_s(out, cap, _TRUNCATE, "%04d-%02d-%02d", (int)st.wYear,
                (int)st.wMonth, (int)st.wDay);
#else
    snprintf(out, cap, "%04d-%02d-%02d", (int)st.wYear, (int)st.wMonth,
             (int)st.wDay);
#endif
}

void TodoDlg_SetDue(HWND hdlg, const char *dueDate) {
    if (!hdlg) return;
    if (!dueDate || !dueDate[0]) {
        SendDlgItemMessageW(hdlg, IDC_TODO_NEW_DUE, DTM_SETSYSTEMTIME,
                            GDT_NONE, 0);
        return;
    }
    int y = 0, m = 0, d = 0;
    if (sscanf_s(dueDate, "%d-%d-%d", &y, &m, &d) != 3) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    st.wYear = (WORD)y;
    st.wMonth = (WORD)m;
    st.wDay = (WORD)d;
    st.wHour = st.wMinute = st.wSecond = st.wMilliseconds = 0;
    SendDlgItemMessageW(hdlg, IDC_TODO_NEW_DUE, DTM_SETSYSTEMTIME,
                        GDT_VALID, (LPARAM)&st);
}
