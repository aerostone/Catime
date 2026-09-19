/**
 * @file todo_conflict.c
 * @brief Conflict-file helpers: count/open snapshots beside todo.txt.
 *
 * Split from todo_sync_status.c to keep files under the 300-line gate.
 * Pattern: <todoDir>/todo.txt.conflict-* (written by AppendConflict).
 */
#include <stdio.h>
#include <string.h>

#include <windows.h>

int TodoConflict_Count(void) {
    extern const char *TodoStore_TxtPath(void);
    const char *txt = TodoStore_TxtPath();
    if (!txt || !txt[0]) return 0;
    char dir[MAX_PATH] = "";
#if defined(_MSC_VER)
    strcpy_s(dir, sizeof(dir), txt);
#else
    snprintf(dir, sizeof(dir), "%s", txt);
#endif
    char *sep = strrchr(dir, '\\');
    if (!sep) sep = strrchr(dir, '/');
    if (!sep) return 0;
    char base[MAX_PATH] = "";
#if defined(_MSC_VER)
    strcpy_s(base, sizeof(base), sep + 1);
#else
    snprintf(base, sizeof(base), "%s", sep + 1);
#endif
    *sep = '\0';
    char pat[MAX_PATH];
#if defined(_MSC_VER)
    _snprintf_s(pat, sizeof(pat), _TRUNCATE, "%s\\%s.conflict-*", dir, base);
#else
    snprintf(pat, sizeof(pat), "%s\\%s.conflict-*", dir, base);
#endif
    wchar_t wp[MAX_PATH];
    if (!MultiByteToWideChar(CP_UTF8, 0, pat, -1, wp, _countof(wp)))
        return 0;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(wp, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    int n = 0;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) n++;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return n;
}

void TodoConflict_OpenDir(HWND hwnd) {
    extern const char *TodoStore_TxtPath(void);
    const char *txt = TodoStore_TxtPath();
    if (!txt || !txt[0]) return;
    char dir[MAX_PATH] = "";
#if defined(_MSC_VER)
    strcpy_s(dir, sizeof(dir), txt);
#else
    snprintf(dir, sizeof(dir), "%s", txt);
#endif
    char *sep = strrchr(dir, '\\');
    if (!sep) sep = strrchr(dir, '/');
    if (sep) *sep = '\0';
    else return;
    wchar_t w[MAX_PATH];
    if (!MultiByteToWideChar(CP_UTF8, 0, dir, -1, w, _countof(w))) return;
    ShellExecuteW(hwnd, L"open", w, NULL, NULL, SW_SHOWNORMAL);
}
