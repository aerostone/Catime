/**
 * @file todo_stickies_pin.c
 * @brief Sticky pin flags + geometry persistence (todo.ini sections).
 *
 * Sections: [Sticky<id>] Pinned=0/1 X= Y= W= H= Color=RRGGBB.
 * Geometry helpers shared with the window module.
 */
#include <stdio.h>
#include <string.h>

#include "todo_stickies.h"
#include "todo_store.h"

extern const char *TodoStore_IniPath(void);

#define STICKY_DEF_W 300
#define STICKY_DEF_H 220

static void SecFor(const char *taskId, char *out, size_t cap) {
    _snprintf_s(out, cap, _TRUNCATE, "Sticky%s", taskId ? taskId : "");
}

BOOL TodoSticky_IsPinned(const char *taskId) {
    if (!taskId || !taskId[0]) return FALSE;
    TodoFilter f;
    TodoTask buf[TODO_STORE_MAX_TASKS];
    TodoFilter_InitDefault(&f);
    f.showDone = TRUE;
    f.showLocal = TRUE;
    f.showSync = FALSE;
    int n = TodoStore_Query(&f, buf, TODO_STORE_MAX_TASKS);
    for (int i = 0; i < n; i++) {
        if (strcmp(buf[i].id, taskId) == 0) return buf[i].pinned;
    }
    return FALSE;
}

BOOL TodoSticky_SetPinned(const char *taskId, BOOL pinned) {
    if (!taskId || !taskId[0]) return FALSE;
    BOOL ok = TodoStore_SetPinned(taskId, pinned);
    if (ok && pinned) TodoStickies_Show(taskId);
    if (ok && !pinned) TodoStickies_Hide(taskId);
    return ok;
}

void TodoSticky_LoadGeom(const char *taskId, int *x, int *y, int *w, int *h) {
    int dx = 120, dy = 120, dw = STICKY_DEF_W, dh = STICKY_DEF_H;
    const char *ini = TodoStore_IniPath();
    if (taskId && taskId[0] && ini && ini[0]) {
        char sec[96];
        SecFor(taskId, sec, sizeof(sec));
        dx = GetPrivateProfileIntA(sec, "X", dx, ini);
        dy = GetPrivateProfileIntA(sec, "Y", dy, ini);
        dw = GetPrivateProfileIntA(sec, "W", dw, ini);
        dh = GetPrivateProfileIntA(sec, "H", dh, ini);
    }
    if (dw < 200) dw = 200;
    if (dh < 150) dh = 150;
    if (x) *x = dx;
    if (y) *y = dy;
    if (w) *w = dw;
    if (h) *h = dh;
}

void TodoSticky_SaveGeom(const char *taskId, int x, int y, int w, int h) {
    const char *ini = TodoStore_IniPath();
    if (!taskId || !taskId[0] || !ini || !ini[0]) return;
    char sec[96], v[16];
    SecFor(taskId, sec, sizeof(sec));
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%d", x);
    WritePrivateProfileStringA(sec, "X", v, ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%d", y);
    WritePrivateProfileStringA(sec, "Y", v, ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%d", w);
    WritePrivateProfileStringA(sec, "W", v, ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%d", h);
    WritePrivateProfileStringA(sec, "H", v, ini);
}

/* Collapsed + per-card topmost override persist in same section. */
BOOL TodoSticky_IsCollapsed(const char *taskId) {
    const char *ini = TodoStore_IniPath();
    if (!taskId || !taskId[0] || !ini || !ini[0]) return FALSE;
    char sec[96];
    SecFor(taskId, sec, sizeof(sec));
    return GetPrivateProfileIntA(sec, "Collapsed", 0, ini) != 0;
}

void TodoSticky_SetCollapsed(const char *taskId, BOOL collapsed) {
    const char *ini = TodoStore_IniPath();
    if (!taskId || !taskId[0] || !ini || !ini[0]) return;
    char sec[96];
    SecFor(taskId, sec, sizeof(sec));
    WritePrivateProfileStringA(sec, "Collapsed", collapsed ? "1" : "0", ini);
}

/* Global sticky topmost default ([Sticky] Topmost=1 default). */
BOOL TodoSticky_TopmostGlobal(void) {
    const char *ini = TodoStore_IniPath();
    if (!ini || !ini[0]) return TRUE;
    return GetPrivateProfileIntA("Sticky", "Topmost", 1, ini) != 0;
}

void TodoSticky_SetTopmostGlobal(BOOL topmost) {
    const char *ini = TodoStore_IniPath();
    if (!ini || !ini[0]) return;
    WritePrivateProfileStringA("Sticky", "Topmost", topmost ? "1" : "0",
                               ini);
}

/* Per-card override: -1 inherit, 0 normal, 1 topmost. */
int TodoSticky_TopmostOverride(const char *taskId) {
    const char *ini = TodoStore_IniPath();
    if (!taskId || !taskId[0] || !ini || !ini[0]) return -1;
    char sec[96];
    SecFor(taskId, sec, sizeof(sec));
    char v[8] = "";
    GetPrivateProfileStringA(sec, "Topmost", "", v, sizeof(v), ini);
    if (strcmp(v, "0") == 0) return 0;
    if (strcmp(v, "1") == 0) return 1;
    return -1;
}

void TodoSticky_SetTopmostOverride(const char *taskId, int mode) {
    const char *ini = TodoStore_IniPath();
    if (!taskId || !taskId[0] || !ini || !ini[0]) return;
    char sec[96];
    SecFor(taskId, sec, sizeof(sec));
    if (mode < 0)
        WritePrivateProfileStringA(sec, "Topmost", NULL, ini);
    else
        WritePrivateProfileStringA(sec, "Topmost", mode ? "1" : "0", ini);
}

/* Effective topmost for a card. */
BOOL TodoSticky_TopmostFor(const char *taskId) {
    int ov = TodoSticky_TopmostOverride(taskId);
    if (ov >= 0) return ov != 0;
    return TodoSticky_TopmostGlobal();
}
