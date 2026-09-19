/**
 * @file dialog_todo_scope.c
 * @brief Week/Month scope range computation for the TODO list dialog.
 *
 * Split from dialog_todo_list.c to respect the 300-line gate.
 * Monday..Sunday for Week, 1st..last-day for Month, Custom untouched.
 */
#include <windows.h>
#include <stdio.h>

#include "todo/todo_store.h"

void TodoDlg_ApplyScopeRange(TodoFilter *f) {
    if (!f || f->dueScope == TODO_DUE_SCOPE_CUSTOM) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    if (f->dueScope == TODO_DUE_SCOPE_MONTH) {
#if defined(_MSC_VER)
        _snprintf_s(f->fromDate, sizeof(f->fromDate), _TRUNCATE,
                    "%04d-%02d-01", st.wYear, st.wMonth);
#else
        snprintf(f->fromDate, sizeof(f->fromDate),
                 "%04d-%02d-01", st.wYear, st.wMonth);
#endif
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
#if defined(_MSC_VER)
        _snprintf_s(f->toDate, sizeof(f->toDate), _TRUNCATE,
                    "%04d-%02d-%02d", st.wYear, st.wMonth, last);
#else
        snprintf(f->toDate, sizeof(f->toDate),
                 "%04d-%02d-%02d", st.wYear, st.wMonth, last);
#endif
        return;
    }
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
#if defined(_MSC_VER)
    _snprintf_s(f->fromDate, sizeof(f->fromDate), _TRUNCATE,
                "%04d-%02d-%02d", s0.wYear, s0.wMonth, s0.wDay);
    _snprintf_s(f->toDate, sizeof(f->toDate), _TRUNCATE,
                "%04d-%02d-%02d", s1.wYear, s1.wMonth, s1.wDay);
#else
    snprintf(f->fromDate, sizeof(f->fromDate),
             "%04d-%02d-%02d", s0.wYear, s0.wMonth, s0.wDay);
    snprintf(f->toDate, sizeof(f->toDate),
             "%04d-%02d-%02d", s1.wYear, s1.wMonth, s1.wDay);
#endif
}
