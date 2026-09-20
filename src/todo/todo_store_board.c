/**
 * @file todo_store_board.c
 * @brief Board membership mutations for the local task store.
 *
 * Split out of todo_store_core.c to respect the 300-line source gate.
 * Both functions take the store lock and go through the shared
 * RowAt/Save accessors instead of touching core statics.
 */
#include <string.h>
#include <time.h>

#include "todo_store.h"

static long long StampNow(void) {
    return (long long)time(NULL);
}

BOOL TodoStore_SetBoard(const char *id, const char *board) {
    if (!id || !id[0]) return FALSE;
    if (board && strlen(board) >= TODO_STORE_BOARD_LEN) return FALSE;
    TodoStore_Lock();
    int i = TodoStore_FindIndex(id);
    BOOL ok = FALSE;
    if (i >= 0) {
        TodoTask *t = TodoStore_RowAt(i);
        if (t) {
            if (board)
                strcpy_s(t->board, sizeof(t->board), board);
            else
                t->board[0] = '\0';
            t->updatedAt = StampNow();
            TodoStore_Save();
            ok = TRUE;
        }
    }
    TodoStore_Unlock();
    return ok;
}

/* Board renamed/removed: move every member task onto the new name. */
void TodoStore_ReassignBoard(const char *oldName, const char *newName) {
    if (!oldName || !oldName[0] || !newName || !newName[0]) return;
    TodoStore_Lock();
    BOOL touched = FALSE;
    int n = TodoStore_Count();
    for (int i = 0; i < n; i++) {
        TodoTask *t = TodoStore_RowAt(i);
        if (!t || strcmp(t->board, oldName) != 0) continue;
        strcpy_s(t->board, sizeof(t->board), newName);
        t->updatedAt = StampNow();
        touched = TRUE;
    }
    if (touched) TodoStore_Save();
    TodoStore_Unlock();
}
