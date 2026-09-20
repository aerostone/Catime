/**
 * @file todo_board_internal.h
 * @brief Shared ini accessors for the board modules (internal header).
 *
 * The board list lives in todo_board_list.c; filter/geometry/settings
 * accessors in todo_board.c read the same ini through these helpers so
 * both TUs agree on section names and defaults.
 */
#ifndef CATIME_TODO_BOARD_INTERNAL_H
#define CATIME_TODO_BOARD_INTERNAL_H

#include <stddef.h>
#include <windows.h>

const char *TodoBoard_Ini(void); /* NULL when no config path is known */
void TodoBoard_Sec(const char *name, char *out, size_t cap);
void TodoBoard_GetStr(const char *sec, const char *key, const char *def,
                      char *out, size_t cap);
void TodoBoard_PutStr(const char *sec, const char *key, const char *val);
void TodoBoard_PutInt(const char *sec, const char *key, int v);

#endif /* CATIME_TODO_BOARD_INTERNAL_H */
