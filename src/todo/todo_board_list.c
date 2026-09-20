/**
 * @file todo_board_list.c
 * @brief Board list + ini access primitives (shared with todo_board.c).
 *
 * Sections:
 *   [Boards] Count=N Item0=NAME Item1=NAME ...
 * The sync board (TODO_BOARD_SYNC) is implicit: always present, never
 * removable, and its tasks arrive by pull only.
 */
#include <stdio.h>
#include <string.h>

#include "todo_board.h"
#include "todo_store.h"

#include "todo_board_internal.h"

static char s_names[TODO_BOARD_MAX][TODO_STORE_BOARD_LEN];
static int s_count = 0;

const char *TodoBoard_Ini(void) {
    const char *p = TodoStore_IniPath();
    return (p && p[0]) ? p : NULL;
}

void TodoBoard_Sec(const char *name, char *out, size_t cap) {
    _snprintf_s(out, cap, _TRUNCATE, "Board %s", name ? name : "");
}

void TodoBoard_GetStr(const char *sec, const char *key, const char *def,
                      char *out, size_t cap) {
    out[0] = '\0';
    const char *ini = TodoBoard_Ini();
    if (!ini) {
        if (def) strcpy_s(out, cap, def);
        return;
    }
    GetPrivateProfileStringA(sec, key, def ? def : "", out, (DWORD)cap, ini);
}

void TodoBoard_PutStr(const char *sec, const char *key, const char *val) {
    const char *ini = TodoBoard_Ini();
    if (ini) WritePrivateProfileStringA(sec, key, val, ini);
}

void TodoBoard_PutInt(const char *sec, const char *key, int v) {
    char b[16];
    _snprintf_s(b, sizeof(b), _TRUNCATE, "%d", v);
    TodoBoard_PutStr(sec, key, b);
}

static void SaveList(void) {
    const char *ini = TodoBoard_Ini();
    if (!ini) return;
    TodoBoard_PutInt("Boards", "Count", s_count);
    for (int i = 0; i < TODO_BOARD_MAX; i++) {
        char key[16];
        _snprintf_s(key, sizeof(key), _TRUNCATE, "Item%d", i);
        WritePrivateProfileStringA("Boards", key,
                                   i < s_count ? s_names[i] : NULL, ini);
    }
}

static int Find(const char *name) {
    if (!name || !name[0]) return -1;
    for (int i = 0; i < s_count; i++) {
        if (_stricmp(s_names[i], name) == 0) return i;
    }
    return -1;
}

static BOOL Push(const char *name) {
    if (s_count >= TODO_BOARD_MAX) return FALSE;
    strcpy_s(s_names[s_count], TODO_STORE_BOARD_LEN, name);
    s_count++;
    return TRUE;
}

void TodoBoard_Init(void) {
    s_count = 0;
    const char *ini = TodoBoard_Ini();
    int n = ini ? GetPrivateProfileIntA("Boards", "Count", 0, ini) : 0;
    if (n < 0) n = 0;
    if (n > TODO_BOARD_MAX) n = TODO_BOARD_MAX;
    for (int i = 0; i < n; i++) {
        char key[16], val[TODO_STORE_BOARD_LEN];
        _snprintf_s(key, sizeof(key), _TRUNCATE, "Item%d", i);
        TodoBoard_GetStr("Boards", key, "", val, sizeof(val));
        if (val[0] && strcmp(val, TODO_BOARD_SYNC) != 0) Push(val);
    }
    if (s_count == 0) Push(TODO_BOARD_DEFAULT);
    if (Find(TODO_BOARD_SYNC) < 0) Push(TODO_BOARD_SYNC);
    SaveList();
}

void TodoBoard_Shutdown(void) { s_count = 0; }

int TodoBoard_Count(void) { return s_count; }

const char *TodoBoard_NameAt(int index) {
    if (index < 0 || index >= s_count) return "";
    return s_names[index];
}

int TodoBoard_IndexByName(const char *name) { return Find(name); }

BOOL TodoBoard_IsSync(int index) {
    if (index < 0 || index >= s_count) return FALSE;
    return strcmp(s_names[index], TODO_BOARD_SYNC) == 0;
}

BOOL TodoBoard_Add(const char *name) {
    if (!name || !name[0]) return FALSE;
    if (strlen(name) >= TODO_STORE_BOARD_LEN) return FALSE;
    if (strcmp(name, TODO_BOARD_SYNC) == 0) return FALSE;
    if (Find(name) >= 0) return FALSE;
    if (!Push(name)) return FALSE;
    SaveList();
    return TRUE;
}

BOOL TodoBoard_Rename(int index, const char *newName) {
    if (index < 0 || index >= s_count) return FALSE;
    if (!newName || !newName[0]) return FALSE;
    if (strlen(newName) >= TODO_STORE_BOARD_LEN) return FALSE;
    if (TodoBoard_IsSync(index)) return FALSE;
    int dup = Find(newName);
    if (dup >= 0 && dup != index) return FALSE;
    char oldName[TODO_STORE_BOARD_LEN];
    strcpy_s(oldName, sizeof(oldName), s_names[index]);
    strcpy_s(s_names[index], TODO_STORE_BOARD_LEN, newName);
    SaveList();
    TodoStore_ReassignBoard(oldName, newName);
    return TRUE;
}

BOOL TodoBoard_Remove(int index) {
    if (index < 0 || index >= s_count) return FALSE;
    if (TodoBoard_IsSync(index)) return FALSE;
    if (s_count <= 2) return FALSE; /* keep one local board + sync board */
    char name[TODO_STORE_BOARD_LEN];
    strcpy_s(name, sizeof(name), s_names[index]);
    for (int i = index; i + 1 < s_count; i++)
        strcpy_s(s_names[i], TODO_STORE_BOARD_LEN, s_names[i + 1]);
    s_count--;
    SaveList();
    TodoStore_ReassignBoard(name, s_names[0]); /* orphan -> first board */
    return TRUE;
}
