/**
 * @file todo_store_util.c
 * @brief Date helpers for the store (split to respect the 300-line gate).
 */
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
