/**
 * @file todo_types.h
 * @brief Shared TODO task model: local + merged sync view.
 *
 * Local tasks live in %LOCALAPPDATA%\Catime\todo.ini (UTF-8).
 * Sync tasks come from TodoSync cache via TodoSync_GetLines-equivalent
 * fields; the store module merges both into one filtered view.
 * Pure C, no Win32 dependency (persistence layer injects INI path).
 */
#ifndef CATIME_TODO_TYPES_H
#define CATIME_TODO_TYPES_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TODO_STORE_MAX_TASKS 200
#define TODO_STORE_TITLE_LEN 128
#define TODO_STORE_ID_LEN 64
#define TODO_STORE_DATE_LEN 16 /* YYYY-MM-DD, empty = no date */

/* Importance: 0 none, 1 low, 2 medium, 3 high. */
typedef enum {
    TODO_IMPORTANCE_NONE = 0,
    TODO_IMPORTANCE_LOW = 1,
    TODO_IMPORTANCE_MEDIUM = 2,
    TODO_IMPORTANCE_HIGH = 3
} TodoImportance;

/* Source of a task in the merged view. */
typedef enum {
    TODO_SOURCE_LOCAL = 0,
    TODO_SOURCE_SYNC = 1
} TodoSource;

/* One task, local or merged. doneAt empty unless completed. */
typedef struct {
    char id[TODO_STORE_ID_LEN];
    char title[TODO_STORE_TITLE_LEN];
    TodoSource source;
    BOOL done;
    TodoImportance importance;
    char dueDate[TODO_STORE_DATE_LEN];  /* deadline, YYYY-MM-DD or "" */
    char doneAt[TODO_STORE_DATE_LEN];   /* completion date or "" */
    char createdAt[TODO_STORE_DATE_LEN];/* creation date or "" */
} TodoTask;

/* Filter for list dialog: each field empty/negative = no constraint. */
typedef struct {
    BOOL showDone;          /* FALSE hides completed tasks */
    BOOL showLocal;         /* include local tasks */
    BOOL showSync;          /* include sync tasks */
    int minImportance;      /* -1 = any, else >= value */
    char fromDate[TODO_STORE_DATE_LEN]; /* due >= from, "" = any */
    char toDate[TODO_STORE_DATE_LEN];   /* due <= to, "" = any */
    char doneFrom[TODO_STORE_DATE_LEN]; /* doneAt >= from, "" = any */
    char doneTo[TODO_STORE_DATE_LEN];   /* doneAt <= to, "" = any */
    char keyword[TODO_STORE_TITLE_LEN]; /* substring in title, "" = any */
} TodoFilter;

void TodoFilter_InitDefault(TodoFilter *f);
BOOL TodoTask_MatchesFilter(const TodoTask *t, const TodoFilter *f);

#ifdef __cplusplus
}
#endif

#endif /* CATIME_TODO_TYPES_H */
