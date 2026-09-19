/**
 * @file tray_menu_todo.h
 * @brief TODO submenu builder for the right-click tray menu.
 */
#ifndef CATIME_TRAY_MENU_TODO_H
#define CATIME_TRAY_MENU_TODO_H

#include <windows.h>

void BuildTodoMenu(HMENU hMenu);
BOOL HandleTodoMenuConflict(HWND hwnd, UINT cmd, int index);
UINT TodoMenu_ConflictId(void);

#endif /* CATIME_TRAY_MENU_TODO_H */
