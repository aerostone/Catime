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

#define STICKY_DEF_W 240
#define STICKY_DEF_H 180

static void SecFor(const char *taskId, char *out, size_t cap) {
    _snprintf_s(out, cap, _TRUNCATE, "Sticky%s", taskId ? taskId : "");
}

BOOL TodoSticky_IsPinned(const char *taskId) {
    const char *ini = TodoStore_IniPath();
    if (!taskId || !taskId[0] || !ini || !ini[0]) return FALSE;
    char sec[96];
    SecFor(taskId, sec, sizeof(sec));
    return GetPrivateProfileIntA(sec, "Pinned", 0, ini) != 0;
}

BOOL TodoSticky_SetPinned(const char *taskId, BOOL pinned) {
    const char *ini = TodoStore_IniPath();
    if (!taskId || !taskId[0] || !ini || !ini[0]) return FALSE;
    char sec[96];
    SecFor(taskId, sec, sizeof(sec));
    char v[4];
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%d", pinned ? 1 : 0);
    BOOL ok = WritePrivateProfileStringA(sec, "Pinned", v, ini) != 0;
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
    if (dw < 160) dw = 160;
    if (dh < 120) dh = 120;
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
