/**
 * @file todo_txt_parse.c
 * @brief todo.txt single-line parser -> TodoTask.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "todo_normalize.h"
#include "todo_txt.h"

static BOOL IsDate(const char *s) {
    if (!s || strlen(s) != 10 || s[4] != '-' || s[7] != '-') return FALSE;
    for (int i = 0; s[i]; i++) {
        if (i == 4 || i == 7) continue;
        if (s[i] < '0' || s[i] > '9') return FALSE;
    }
    return TRUE;
}

static BOOL IsPrio(const char *s, TodoImportance *imp) {
    if (!s || s[0] != '(' || s[2] != ')' ||
        (s[3] != ' ' && s[3] != '\0'))
        return FALSE;
    if (s[1] == 'A') *imp = TODO_IMPORTANCE_HIGH;
    else if (s[1] == 'B') *imp = TODO_IMPORTANCE_MEDIUM;
    else if (s[1] == 'C') *imp = TODO_IMPORTANCE_LOW;
    else return FALSE;
    return TRUE;
}

BOOL TodoTxt_ParseLine(const char *line, TodoTask *out, const char *defDate) {
    if (!line || !out) return FALSE;
    char norm[512];
    TodoNormalize_Copy(line, norm, sizeof(norm));
    /* trim leading/trailing whitespace */
    char *p = norm;
    while (*p == ' ' || *p == '\t') p++;
    size_t n = strlen(p);
    while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t' || p[n - 1] == '\r' ||
                     p[n - 1] == '\n'))
        p[--n] = '\0';
    if (n == 0) return FALSE;

    memset(out, 0, sizeof(*out));
    out->source = TODO_SOURCE_LOCAL;
    out->importance = TODO_IMPORTANCE_NONE;
    if (defDate && IsDate(defDate))
        strcpy_s(out->createdAt, sizeof(out->createdAt), defDate);

    char work[512];
    strcpy_s(work, sizeof(work), p);

    /* done marker: "x " prefix */
    char *cur = work;
    if ((cur[0] == 'x' || cur[0] == 'X') && cur[1] == ' ') {
        out->done = TRUE;
        cur += 2;
        while (*cur == ' ') cur++;
        /* done date follows */
        char *sp = strchr(cur, ' ');
        if (sp && (size_t)(sp - cur) == 10) {
            char d[16] = "";
            memcpy(d, cur, 10);
            d[10] = '\0';
            if (IsDate(d)) {
                strcpy_s(out->doneAt, sizeof(out->doneAt), d);
                cur = sp + 1;
                while (*cur == ' ') cur++;
            }
        }
    }

    /* priority "(A)" */
    TodoImportance imp = TODO_IMPORTANCE_NONE;
    if (IsPrio(cur, &imp)) {
        out->importance = imp;
        cur += 3;
        while (*cur == ' ') cur++;
    }

    /* created date (open tasks only) */
    if (!out->done) {
        char *sp = strchr(cur, ' ');
        if (sp && (size_t)(sp - cur) == 10) {
            char d[16] = "";
            memcpy(d, cur, 10);
            d[10] = '\0';
            if (IsDate(d)) {
                strcpy_s(out->createdAt, sizeof(out->createdAt), d);
                cur = sp + 1;
                while (*cur == ' ') cur++;
            }
        }
    }

    /* remainder: title + trailing key:value extensions */
    char title[TODO_STORE_TITLE_LEN] = "";
    size_t tw = 0;
    char *tok = cur;
    while (*tok) {
        while (*tok == ' ') tok++;
        if (!*tok) break;
        char *end = strchr(tok, ' ');
        size_t len = end ? (size_t)(end - tok) : strlen(tok);
        char word[128] = "";
        if (len >= sizeof(word)) len = sizeof(word) - 1;
        memcpy(word, tok, len);
        word[len] = '\0';
        char *colon = strchr(word, ':');
        BOOL consumed = FALSE;
        if (colon && colon != word) {
            if (_strnicmp(word, "due:", 4) == 0 && IsDate(word + 4)) {
                strcpy_s(out->dueDate, sizeof(out->dueDate), word + 4);
                consumed = TRUE;
            } else if (_strnicmp(word, "id:", 3) == 0 && word[3]) {
                strcpy_s(out->id, sizeof(out->id), word + 3);
                consumed = TRUE;
            } else if (_strnicmp(word, "pin:", 4) == 0) {
                consumed = TRUE;
                if (word[4] == '1') out->pinned = TRUE;
            } else if (_strnicmp(word, "upd:", 4) == 0 && word[4]) {
                consumed = TRUE;
                out->updatedAt = _strtoi64(word + 4, NULL, 10);
            } else if (_strnicmp(word, "srv:", 4) == 0 && word[4]) {
                consumed = TRUE;
                strcpy_s(out->serverId, sizeof(out->serverId), word + 4);
            }
        }
        if (!consumed) {
            size_t need = strlen(word) + (tw ? 1 : 0);
            if (tw + need < sizeof(title)) {
                if (tw) title[tw++] = ' ';
                memcpy(title + tw, word, strlen(word));
                tw += strlen(word);
                title[tw] = '\0';
            }
        }
        if (!end) break;
        tok = end + 1;
    }
    if (!title[0]) return FALSE;
    strcpy_s(out->title, sizeof(out->title), title);
    return TRUE;
}
