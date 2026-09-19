/**
 * @file todo_store.h
 * @brief Local TODO task store: CRUD + persistence + merged view.
 *
 * Local tasks persist to todo.ini next to config.ini (UTF-8).
 * Merged view = local tasks + live sync snapshot, filtered + sorted:
 * overdue first, then by due date, then importance desc, then title.
 */
#ifndef CATIME_TODO_STORE_H
#define CATIME_TODO_STORE_H

#include <windows.h>

#include "todo_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Lifecycle: iniPath = config.ini path; todo.ini derived beside it. */
BOOL TodoStore_Init(const wchar_t *configIniPath);
void TodoStore_Shutdown(void);

/* CRUD on local tasks. Returns FALSE on full/invalid input. */
BOOL TodoStore_Add(const char *title, TodoImportance imp, const char *dueDate);
BOOL TodoStore_SetDone(const char *id, BOOL done);
BOOL TodoStore_Remove(const char *id);
BOOL TodoStore_SetImportance(const char *id, TodoImportance imp);
BOOL TodoStore_SetDueDate(const char *id, const char *dueDate);
BOOL TodoStore_SetPinned(const char *id, BOOL pinned);

/* Count of local tasks (open + done). */
int TodoStore_LocalCount(void);

/* Merged view: local + sync snapshot matching filter, sorted.
 * out capacity = TODO_STORE_MAX_TASKS. Returns items written. */
int TodoStore_Query(const TodoFilter *filter, TodoTask *out, int outCap);

/* Open (not done) merged count for overlay header badge. */
int TodoStore_OpenCount(void);

/* Force re-read of todo.ini from disk. */
void TodoStore_Reload(void);

/* todo.ini path (UTF-8). Empty when store not initialized. */
const char *TodoStore_IniPath(void);
/* todo.txt path (UTF-8). Empty when store not initialized. */
const char *TodoStore_TxtPath(void);
/* Find local task by id (copy out). */
BOOL TodoStore_FindById(const char *id, TodoTask *out);
/* Upsert rows pulled from server (normalized, saved to txt). */
BOOL TodoStore_UpdateFromSync(const char *id, const char *title,
                              const char *dueDate, TodoImportance imp,
                              BOOL done);
BOOL TodoStore_UpdateFromSyncStamp(const char *id, const char *title,
                                   const char *dueDate, TodoImportance imp,
                                   BOOL done, long long srvStamp,
                                   const char *srvId);
BOOL TodoStore_InsertSynced(const char *id, const char *title,
                            const char *dueDate, TodoImportance imp,
                            BOOL done);
BOOL TodoStore_InsertSyncedStamp(const char *id, const char *title,
                                 const char *dueDate, TodoImportance imp,
                                 BOOL done, long long srvStamp,
                                 const char *srvId);
void TodoStore_RecordDeleted(const char *id);
int TodoStore_DeletedIds(char out[][TODO_STORE_ID_LEN], int cap);
void TodoStore_ClearDeleted(void);
void TodoStore_StripDeleted(const char *id);

/* Snapshot local tasks (for merged query). Returns items written. */
int TodoStore_SnapshotLocal(TodoTask *out, int outCap);
/* Lock/row accessors for the sync-upsert TU (caller must lock). */
void TodoStore_Lock(void);
void TodoStore_Unlock(void);
int TodoStore_FindIndex(const char *id);
int TodoStore_Count(void);
TodoTask *TodoStore_RowAt(int index);
void TodoStore_Save(void);

#ifdef __cplusplus
}
#endif

#endif /* CATIME_TODO_STORE_H */
