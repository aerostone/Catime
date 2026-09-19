/**
 * @file todo_sticky_rowmenu.c
 * @brief Sticky right-click menu: id-keyed row actions.
 *
 * Menu is built from the bound task id (never row position):
 * - start pomo on this task, toggle done, expand/collapse,
 *   per-card topmost, rename, unpin, delete.
 * SetCollapsedUI resizes window + persists flag. ApplyTopmost flips
 * z-order per override. FindCatimeMainWindow locates the main HWND
 * for launching the shared pomodoro timer.
 */
#include <stdio.h>
#include <string.h>

#include "todo_stickies.h"
#include "todo_store.h"
#include "todo_sticky_pomo.h"

#include "todo_stickies_slot.h"
#include "language.h"

#define STICKY_TIMER_POMO 9201
void TodoStickyEdit_Open(HWND parent, const char *taskId, HWND *editOut);

static void Rowmenu_OpenEdit(StickyWin *sw) {
    if (!sw || sw->edit) return;
    TodoStickyEdit_Open(sw->hwnd, sw->taskId, &sw->edit);
}



#define STICKY_BAR_H 26
#define STICKY_MENU_DONE 9101
#define STICKY_MENU_UNPIN 9102
#define STICKY_MENU_DELETE 9103
#define STICKY_MENU_EXPAND 9104
#define STICKY_MENU_TOPMOST 9105
#define STICKY_MENU_POMO 9106
#define STICKY_MENU_RENAME 9107

void TodoSticky_SetCollapsedUI(HWND hwnd, StickyWin *sw, BOOL collapsed) {
    if (!hwnd || !sw) return;
    sw->collapsed = collapsed ? TRUE : FALSE;
    TodoSticky_SetCollapsed(sw->taskId, sw->collapsed);
    RECT wr;
    GetWindowRect(hwnd, &wr);
    int w = wr.right - wr.left;
    if (sw->collapsed) {
        sw->expandH = wr.bottom - wr.top;
        SetWindowPos(hwnd, NULL, 0, 0, w, STICKY_BAR_H + 2,
                     SWP_NOMOVE | SWP_NOZORDER);
    } else {
        int h = sw->expandH >= 120 ? sw->expandH : 180;
        SetWindowPos(hwnd, NULL, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);
    }
    InvalidateRect(hwnd, NULL, TRUE);
}

void TodoSticky_ApplyTopmost(HWND hwnd, StickyWin *sw) {
    if (!hwnd || !sw) return;
    BOOL top = TodoSticky_TopmostFor(sw->taskId);
    SetWindowPos(hwnd, top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE);
}

HWND FindCatimeMainWindow(void) {
    extern HWND g_todoSyncHwnd;
    return g_todoSyncHwnd;
}

void TodoSticky_ShowRowMenu(HWND hwnd, StickyWin *sw) {
    if (!hwnd || !sw) return;
    TodoTask t;
    memset(&t, 0, sizeof(t));
    TodoFilter f;
    TodoFilter_InitDefault(&f);
    f.showDone = TRUE;
    f.showSync = FALSE;
    TodoTask buf[TODO_STORE_MAX_TASKS];
    int n = TodoStore_Query(&f, buf, TODO_STORE_MAX_TASKS);
    BOOL found = FALSE;
    for (int i = 0; i < n; i++) {
        if (strcmp(buf[i].id, sw->taskId) == 0) {
            t = buf[i];
            found = TRUE;
            break;
        }
    }
    HMENU m = CreatePopupMenu();
    if (!m) return;
    /* D5+D7: gray header anchors the task; labels localized. */
    wchar_t wt[TODO_STORE_TITLE_LEN];
    MultiByteToWideChar(CP_UTF8, 0, found ? t.title : sw->taskId, -1, wt,
                        _countof(wt));
    wchar_t head[TODO_STORE_TITLE_LEN + 4];
    wcsncpy_s(head, _countof(head), wt, 20);
    head[20] = L'\0';
    if (found && wcslen(wt) > 20) wcscat_s(head, _countof(head), L"\u2026");
    AppendMenuW(m, MF_STRING | MF_DISABLED | MF_GRAYED, 0, head);
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    wchar_t pomo[48];
    _snwprintf_s(pomo, _countof(pomo), _TRUNCATE, L"\u25b6 %s",
                 GetLocalizedString(L"\u5f00\u59cb\u756a\u8304", L"Start pomodoro"));
    AppendMenuW(m, MF_STRING, STICKY_MENU_POMO, pomo);
    AppendMenuW(m, MF_STRING, STICKY_MENU_DONE,
                (found && t.done) ? GetLocalizedString(L"\u6807\u4e3a\u672a\u5b8c\u6210", L"Mark open")
                                    : GetLocalizedString(L"\u6807\u4e3a\u5b8c\u6210", L"Mark done"));
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, STICKY_MENU_EXPAND,
                sw->collapsed ? GetLocalizedString(L"\u5c55\u5f00", L"Expand")
                                : GetLocalizedString(L"\u6536\u8d77", L"Collapse"));
    AppendMenuW(m, MF_STRING | (TodoSticky_TopmostFor(sw->taskId) ? MF_CHECKED : 0),
                STICKY_MENU_TOPMOST,
                GetLocalizedString(L"\u7f6e\u9876\u6b64\u5361", L"Pin on top"));
    AppendMenuW(m, MF_STRING, STICKY_MENU_RENAME,
                GetLocalizedString(L"\u91cd\u547d\u540d\u2026", L"Rename..."));
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, STICKY_MENU_UNPIN,
                GetLocalizedString(L"\u53d6\u6d88\u7f6e\u9876", L"Unpin"));
    /* D3: delete sits last + confirms (see RunMenuById). */
    AppendMenuW(m, MF_STRING, STICKY_MENU_DELETE,
                GetLocalizedString(L"\u5220\u9664\u4efb\u52a1\u2026", L"Delete task..."));
    POINT pt;
    GetCursorPos(&pt);
    UINT cmd = TrackPopupMenu(m, TPM_RETURNCMD, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(m);
    TodoSticky_RunMenuById(hwnd, sw, cmd);
}

void TodoSticky_RunMenuById(HWND hwnd, StickyWin *sw, unsigned cmd) {
    if (!hwnd || !sw) return;
    const char *taskId = sw->taskId;
    TodoTask tk;
    BOOL done = FALSE;
    {
        TodoFilter fr;
        TodoFilter_InitDefault(&fr);
        fr.showDone = TRUE;
        fr.showSync = FALSE;
        TodoTask bq[TODO_STORE_MAX_TASKS];
        int nq = TodoStore_Query(&fr, bq, TODO_STORE_MAX_TASKS);
        for (int i = 0; i < nq; i++) {
            if (strcmp(bq[i].id, taskId) == 0) {
                tk = bq[i];
                done = tk.done;
                break;
            }
        }
    }
    if (cmd == STICKY_MENU_DONE) {
        TodoStore_SetDone(taskId, !done);
        InvalidateRect(hwnd, NULL, TRUE);
    } else if (cmd == STICKY_MENU_UNPIN) {
        TodoSticky_SetPinned(taskId, FALSE);
    } else if (cmd == STICKY_MENU_DELETE) {
        int rc = MessageBoxW(hwnd,
            GetLocalizedString(
                L"\u5220\u9664\u540e\u5c06\u540c\u6b65\u5220\u9664\u670d\u52a1\u7aef\u4efb\u52a1\uff0c\u786e\u8ba4\u5220\u9664\uff1f",
                L"Delete will also remove the server task on next sync. Delete?"),
            GetLocalizedString(L"TODO", L"TODO"),
            MB_OKCANCEL | MB_ICONWARNING);
        if (rc != IDOK) return;
        char id[TODO_STORE_ID_LEN];
        strcpy_s(id, sizeof(id), taskId);
        TodoStickies_Forget(id);
        TodoStore_Remove(id);
    } else if (cmd == STICKY_MENU_EXPAND) {
        if (sw->collapsed) TodoSticky_SetCollapsedUI(hwnd, sw, FALSE);
        else TodoSticky_SetCollapsedUI(hwnd, sw, TRUE);
        TodoSticky_UpdateTips(hwnd, sw->collapsed);
    } else if (cmd == STICKY_MENU_TOPMOST) {
        int ov = TodoSticky_TopmostOverride(taskId);
        int next = (ov == 1) ? 0 : 1;
        TodoSticky_SetTopmostOverride(taskId, next);
        TodoSticky_ApplyTopmost(hwnd, sw);
    } else if (cmd == STICKY_MENU_POMO) {
        HWND main = FindCatimeMainWindow();
        TodoStickyPomo_Start(main, taskId);
        sw->collapsed = FALSE;
        SetTimer(hwnd, STICKY_TIMER_POMO, 1000, NULL);
        InvalidateRect(hwnd, NULL, TRUE);
    } else if (cmd == STICKY_MENU_RENAME) {
        Rowmenu_OpenEdit(sw);
    }
}

