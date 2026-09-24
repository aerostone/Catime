/**
 * @file todo_store_util.h
 * @brief Tiny date helpers shared inside the store (internal header).
 */
#ifndef CATIME_TODO_STORE_UTIL_H
#define CATIME_TODO_STORE_UTIL_H

#include <windows.h>

void TodoStore_TodayStr(char *out, size_t cap);
BOOL TodoStore_ValidDate(const char *d);
long long TodoStore_StampNext(long long prev);

#endif /* CATIME_TODO_STORE_UTIL_H */
