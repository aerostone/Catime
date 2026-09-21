/**
 * @file todo_sticky_rowmenu.c
 * @brief Card + task-row context menus for sticky boards.
 *
 * Card menu: new task / open list / week-all / collapse / topmost / hide.
 * Row menu : done toggle / pomodoro / due / importance / move / delete.
 * All mutations go through the store (id-keyed) so the next paint and the
 * next sync round see the same state.
 */
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "dialog/dialog_todo.h"
#include "language.h"
#include "todo_board.h"
#include "todo_stickies.h"
#include "todo_store.h"
#include "todo_sync.h"
#include "todo_sticky_pomo.h"

#include "todo_stickies_slot.h"

#define ROW_MENU_BOARD_BASE 9400

static void Repaint(StickyWin *sw) { TodoSticky_Repaint(sw); }

static HWND MainHwnd(void) { return TodoSync_MainHwnd(); }

/* ---- card menu ----------------------------------------------------- */
void TodoSticky_ShowCardMenu(HWND hwnd, StickyWin *sw) {
    if (!sw) return;
    BOOL week = TodoBoard_Scope(sw->board) != TODO_DUE_SCOPE_ALL;
    HMENU m = CreatePopupMenu();
    if (!m) return;
    AppendMenuW(m, MF_STRING, STICKY_CMD_NEW_TASK,
                GetLocalizedString(L"新建任务...", L"New task..."));
    AppendMenuW(m, MF_STRING, STICKY_CMD_OPEN_LIST,
                GetLocalizedString(L"打开任务列表...", L"Open task list..."));
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING | (week ? MF_CHECKED : 0), STICKY_CMD_SCOPE,
                week ? GetLocalizedString(L"只看本周", L"This week only")
                     : GetLocalizedString(L"显示全部", L"Show all"));
    AppendMenuW(m, MF_STRING, STICKY_CMD_FOLD,
                GetLocalizedString(L"折叠 / 展开", L"Collapse / expand"));
    AppendMenuW(m, MF_STRING | (TodoBoard_TopmostFor(sw->board) ? MF_CHECKED : 0),
                STICKY_CMD_TOPMOST,
                GetLocalizedString(L"置顶显示", L"Always on top"));
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, STICKY_CMD_HIDE,
                GetLocalizedString(L"隐藏此便签（任务保留）",
                                   L"Hide this board (tasks kept)"));
    POINT pt;
    GetCursorPos(&pt);
    int cmd = (int)TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x,
                                  pt.y, 0, hwnd, NULL);
    DestroyMenu(m);
    if (!cmd) return;
    switch (cmd) {
    case STICKY_CMD_NEW_TASK:
        ShowTodoListDialogForBoard(MainHwnd(), sw->board, TRUE);
        break;
    case STICKY_CMD_OPEN_LIST:
        ShowTodoListDialogForBoard(MainHwnd(), sw->board, FALSE);
        break;
    case STICKY_CMD_SCOPE:
        TodoBoard_SetScope(sw->board, week ? TODO_DUE_SCOPE_ALL
                                           : TODO_DUE_SCOPE_WEEK);
        Repaint(sw);
        break;
    case STICKY_CMD_FOLD:
        TodoSticky_SetCollapsedUI(hwnd, sw, !sw->collapsed);
        break;
    case STICKY_CMD_TOPMOST: {
        int ov = TodoBoard_TopmostOverride(sw->board);
        TodoBoard_SetTopmostOverride(sw->board, ov == 1 ? 0 : 1);
        TodoSticky_RetopAll();
        break;
    }
    case STICKY_CMD_HIDE:
        TodoStickies_HideBoard(sw->board);
        break;
    default:
        break;
    }
}

/* ---- row menu ------------------------------------------------------ */
static BOOL LoadTask(const char *id, TodoTask *out) {
    memset(out, 0, sizeof(*out));
    return TodoStore_FindById(id, out);
}

static void AddEditItem(HMENU m) {
    /* N3: single-editor rule - the list owns date/importance pickers. */
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, STICKY_CMD_ROW_EDIT,
                GetLocalizedString(L"在列表中编辑...", L"Edit in task list..."));
}

static void AddBoardItems(HMENU m, const char *board) {
    HMENU sub = CreatePopupMenu();
    if (!sub) return;
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    int n = TodoBoard_Count();
    for (int i = 0; i < n; i++) {
        const char *name = TodoBoard_NameAt(i);
        if (!name[0] || strcmp(name, TODO_BOARD_SYNC) == 0) continue;
        if (strcmp(name, board) == 0) continue;
        wchar_t wname[TODO_STORE_BOARD_LEN];
        MultiByteToWideChar(CP_UTF8, 0, name, -1, wname,
                            TODO_STORE_BOARD_LEN);
        AppendMenuW(sub, MF_STRING, ROW_MENU_BOARD_BASE + i, wname);
    }
    if (GetMenuItemCount(sub) > 0)
        AppendMenuW(m, MF_STRING | MF_POPUP, (UINT_PTR)sub,
                    GetLocalizedString(L"移动到便签", L"Move to board"));
    else
        DestroyMenu(sub);
}

void TodoSticky_ShowRowMenu(HWND hwnd, StickyWin *sw, int row) {
    if (!sw || row < 0) {
        TodoSticky_ShowCardMenu(hwnd, sw);
        return;
    }
    char id[TODO_STORE_ID_LEN] = "";
    if (!TodoSticky_RowIdAt(sw->board, row, id, sizeof(id))) {
        TodoSticky_ShowCardMenu(hwnd, sw);
        return;
    }
    TodoTask t;
    if (!LoadTask(id, &t)) return;
    strcpy_s(sw->menuTaskId, sizeof(sw->menuTaskId), id);
    HMENU m = CreatePopupMenu();
    if (!m) return;
    AppendMenuW(m, MF_STRING, STICKY_CMD_ROW_DONE,
                t.done ? GetLocalizedString(L"标记未完成", L"Mark not done")
                       : GetLocalizedString(L"标记完成", L"Mark done"));
    AppendMenuW(m, MF_STRING, STICKY_CMD_ROW_POMO,
                GetLocalizedString(L"开始番茄钟", L"Start pomodoro"));
    AddEditItem(m);
    if (t.source != TODO_SOURCE_SYNC) AddBoardItems(m, sw->board);
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, STICKY_CMD_ROW_DELETE,
                GetLocalizedString(L"删除任务", L"Delete task"));
    POINT pt;
    GetCursorPos(&pt);
    int cmd = (int)TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x,
                                  pt.y, 0, hwnd, NULL);
    DestroyMenu(m);
    if (!cmd) return;
    if (cmd >= ROW_MENU_BOARD_BASE) {
        const char *dest = TodoBoard_NameAt(cmd - ROW_MENU_BOARD_BASE);
        if (dest[0]) TodoBoard_AssignTask(id, dest);
        Repaint(sw);
        return;
    }
    TodoSticky_RowMenuCommand(hwnd, sw, (UINT)cmd);
}

/* Due/importance edits reuse the list dialog, which owns the date and
 * priority pickers; the row is preselected there. */
static void EditInList(StickyWin *sw) {
    TodoDlg_Preselect(sw->menuTaskId);
    ShowTodoListDialogForBoard(MainHwnd(), sw->board, FALSE);
}

void TodoSticky_RowMenuCommand(HWND hwnd, StickyWin *sw, UINT cmd) {
    if (!sw || !sw->menuTaskId[0]) return;
    TodoTask t;
    if (!LoadTask(sw->menuTaskId, &t)) {
        sw->menuTaskId[0] = '\0';
        Repaint(sw);
        return;
    }
    switch (cmd) {
    case STICKY_CMD_ROW_DONE:
        TodoStore_SetDone(t.id, !t.done);
        TodoSync_PollNow();
        break;
    case STICKY_CMD_ROW_POMO:
        TodoStickyPomo_Start(MainHwnd(), t.id);
        break;
    case STICKY_CMD_ROW_EDIT:
        EditInList(sw);
        return;
    case STICKY_CMD_ROW_DELETE: {
        wchar_t wt[TODO_STORE_TITLE_LEN], msg[320];
        wchar_t fmt[192];
        MultiByteToWideChar(CP_UTF8, 0, t.title, -1, wt, TODO_STORE_TITLE_LEN);
        wcsncpy_s(fmt, 192,
                  GetLocalizedString(
                      L"删除“%s”？服务端任务也会被删除。",
                      L"Delete \"%s\"? The server copy is deleted too."),
                  _TRUNCATE);
        _snwprintf_s(msg, 320, _TRUNCATE, fmt, wt);
        if (MessageBoxW(hwnd, msg, L"TODO",
                        MB_OKCANCEL | MB_ICONWARNING) != IDOK)
            return;
        TodoStore_Remove(t.id);
        TodoSync_PollNow();
        break;
    }
    default:
        break;
    }
    Repaint(sw);
}
