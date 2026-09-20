/**
 * @file todo_date_range.c
 * @brief Week/Month window computation for filters (local time).
 *
 * Week is Monday..Sunday, Month is 1st..last-day, both relative to the
 * current date. FILETIME arithmetic is done at noon to stay clear of
 * DST edges.
 */
#include <stdio.h>
#include <windows.h>

#include "todo_date_range.h"

static void Fmt(char *out, size_t cap, const SYSTEMTIME *st) {
#if defined(_MSC_VER)
    _snprintf_s(out, cap, _TRUNCATE, "%04d-%02d-%02d",
                (int)st->wYear, (int)st->wMonth, (int)st->wDay);
#else
    snprintf(out, cap, "%04d-%02d-%02d",
             (int)st->wYear, (int)st->wMonth, (int)st->wDay);
#endif
}

static int LastDayOfMonth(int year, int month) {
    switch (month) {
    case 1: case 3: case 5: case 7: case 8: case 10: case 12: return 31;
    case 4: case 6: case 9: case 11: return 30;
    default: {
        BOOL leap = ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0);
        return leap ? 29 : 28;
    }
    }
}

void TodoDateRange_CurrentMonth(char *from, size_t fromCap,
                                char *to, size_t toCap) {
    SYSTEMTIME st;
    GetLocalTime(&st);
#if defined(_MSC_VER)
    _snprintf_s(from, fromCap, _TRUNCATE, "%04d-%02d-01",
                (int)st.wYear, (int)st.wMonth);
#else
    snprintf(from, fromCap, "%04d-%02d-01", (int)st.wYear, (int)st.wMonth);
#endif
#if defined(_MSC_VER)
    _snprintf_s(to, toCap, _TRUNCATE, "%04d-%02d-%02d", (int)st.wYear,
                (int)st.wMonth, LastDayOfMonth((int)st.wYear, (int)st.wMonth));
#else
    snprintf(to, toCap, "%04d-%02d-%02d", (int)st.wYear, (int)st.wMonth,
             LastDayOfMonth((int)st.wYear, (int)st.wMonth));
#endif
}

void TodoDateRange_CurrentWeek(char *from, size_t fromCap,
                               char *to, size_t toCap) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    int dow = (int)st.wDayOfWeek; /* 0 = Sunday */
    int back = (dow == 0) ? 6 : dow - 1;
    int fwd = (dow == 0) ? 0 : 7 - dow;
    SYSTEMTIME base = st;
    base.wHour = 12;
    base.wMinute = 0;
    base.wSecond = 0;
    base.wMilliseconds = 0;
    FILETIME ft;
    if (!SystemTimeToFileTime(&base, &ft)) {
        /* fall back to today..today rather than emit a broken range */
        Fmt(from, fromCap, &st);
        Fmt(to, toCap, &st);
        return;
    }
    ULONGLONG day = 24ULL * 3600ULL * 10000000ULL;
    ULONGLONG q = ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    q -= (ULONGLONG)back * day;
    FILETIME f0;
    f0.dwLowDateTime = (DWORD)(q & 0xFFFFFFFFULL);
    f0.dwHighDateTime = (DWORD)(q >> 32);
    q += (ULONGLONG)(back + fwd) * day;
    FILETIME f1;
    f1.dwLowDateTime = (DWORD)(q & 0xFFFFFFFFULL);
    f1.dwHighDateTime = (DWORD)(q >> 32);
    SYSTEMTIME s0, s1;
    if (!FileTimeToSystemTime(&f0, &s0) || !FileTimeToSystemTime(&f1, &s1)) {
        Fmt(from, fromCap, &st);
        Fmt(to, toCap, &st);
        return;
    }
    Fmt(from, fromCap, &s0);
    Fmt(to, toCap, &s1);
}
