/**
 * @file todo_store_access.c
 * @brief Read-only snapshot helpers over the locked store.
 *
 * Split out of todo_store_core.c to respect the 300-line source gate.
 * TodoStore_Count and TodoStore_RowAt live in the core next to the
 * static task array and assume the caller already holds the lock.
 */
#include "todo_store.h"

int TodoStore_LocalCount(void) {
    TodoStore_Lock();
    int n = TodoStore_Count();
    TodoStore_Unlock();
    return n;
}

int TodoStore_SnapshotLocal(TodoTask *out, int outCap) {
    if (!out || outCap <= 0) return 0;
    TodoStore_Lock();
    int n = TodoStore_Count();
    int w = n < outCap ? n : outCap;
    for (int i = 0; i < w; i++) {
        TodoTask *t = TodoStore_RowAt(i);
        if (t) out[i] = *t;
    }
    TodoStore_Unlock();
    return w;
}
