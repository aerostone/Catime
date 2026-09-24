/**
 * @file todo_store_util.c
 * @brief Date helpers for the store (split to respect the 300-line gate).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "todo_store_util.h"

void TodoStore_TodayStr(char *out, size_t cap) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    _snprintf_s(out, cap, _TRUNCATE, "%04d-%02d-%02d",
                (int)st.wYear, (int)st.wMonth, (int)st.wDay);
}

BOOL TodoStore_ValidDate(const char *d) {
    if (!d || !d[0]) return TRUE; /* empty = no date */
    if (strlen(d) != 10 || d[4] != '-' || d[7] != '-') return FALSE;
    for (int i = 0; d[i]; i++) {
        if (i == 4 || i == 7) continue;
        if (d[i] < '0' || d[i] > '9') return FALSE;
    }
    return TRUE;
}

/* Monotonic edit stamp: always strictly newer than the previous value of
 * the same row. For a pulled row the previous value is the server stamp,
 * so a local edit is guaranteed to win the server's newer-wins compare
 * even when the local clock lags the server's. */
long long TodoStore_StampNext(long long prev) {
    long long now = (long long)time(NULL);
    return now > prev ? now : prev + 1;
}
