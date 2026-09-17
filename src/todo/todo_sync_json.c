/**
 * @file todo_sync_json.c
 * @brief Minimal hand-rolled JSON extraction for the TODO sync engine.
 *
 * Backend contract:
 *   {today_open:[{id,title,status}], overdue_open:[...], someday_open:[...],
 *    pomodoros_today_min:N, server_time:T}
 * E2EE envelopes: backend sends title as-is (ciphertext or plaintext);
 * locked titles render as-is; web UI owns unlock. Client shows
 * "[locked]" placeholder when title looks like base64 envelope >64 chars
 * with empty date? No — server keeps it simple: E2EE accounts get ciphertext
 * titles; we detect non-printable/overlong single-token titles and show
 * "上锁，去网页管理". Plaintext accounts render normally.
 */
#include "todo_sync_internal.h"

/* find "key":<value> helpers; arrays of {id,title,status} only. */
static const char *FindKey(const char *json, const char *key) {
    char pat[64];
    _snprintf_s(pat, sizeof(pat), _TRUNCATE, "\"%s\"", key);
    return strstr(json, pat);
}

BOOL TodoSyncJson_ExtractStr(const char *obj, const char *key,
                             char *out, size_t outSize) {
    const char *p = FindKey(obj, key);
    if (!p) return FALSE;
    p = strchr(p, ':');
    if (!p) return FALSE;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return FALSE;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < outSize) {
        if (*p == '\\' && *(p + 1)) {
            p++;
            switch (*p) {
            case 'n': out[i++] = '\n'; break;
            case 't': out[i++] = '\t'; break;
            case 'u': /* \uXXXX → best-effort '?'; CJK titles are raw UTF-8, not \u-escaped by Go */
                out[i++] = '?';
                if (*(p+1)) p++; if (*(p+1)) p++; if (*(p+1)) p++; if (*(p+1)) p++;
                break;
            default: out[i++] = *p; break;
            }
            p++;
        } else {
            out[i++] = *p++;
        }
    }
    out[i] = '\0';
    return TRUE;
}

BOOL TodoSyncJson_ExtractInt(const char *json, const char *key, long long *out) {
    const char *p = FindKey(json, key);
    if (!p) return FALSE;
    p = strchr(p, ':');
    if (!p) return FALSE;
    *out = _strtoi64(p + 1, NULL, 10);
    return TRUE;
}

/* parse [{...},{...}] array at key into items; returns count. */
int TodoSyncJson_ParseItemArray(const char *json, const char *key,
                                TodoItem *items, int max) {
    const char *p = FindKey(json, key);
    if (!p) return 0;
    p = strchr(p, '[');
    if (!p) return 0;
    p++;
    int n = 0;
    while (*p && *p != ']' && n < max) {
        const char *o = strchr(p, '{');
        const char *end = strchr(p, ']');
        if (!o || (end && o > end)) break;
        const char *oe = strchr(o, '}');
        if (!oe) break;
        /* temporarily terminate: copy object slice */
        size_t len = (size_t)(oe - o + 1);
        char *slice = (char *)malloc(len + 1);
        if (!slice) break;
        memcpy(slice, o, len);
        slice[len] = '\0';
        memset(&items[n], 0, sizeof(items[n]));
        TodoSyncJson_ExtractStr(slice, "id", items[n].id, sizeof(items[n].id));
        TodoSyncJson_ExtractStr(slice, "title", items[n].title, sizeof(items[n].title));
        TodoSyncJson_ExtractStr(slice, "status", items[n].status, sizeof(items[n].status));
        free(slice);
        if (items[n].id[0]) n++;
        p = oe + 1;
    }
    return n;
}

BOOL TodoSyncJson_LooksLocked(const char *title) {
    /* E2EE ciphertext titles: long single token, no spaces/CJK. */
    size_t n = strlen(title);
    if (n < 48) return FALSE;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)title[i];
        if (c >= 0x80 || c == ' ') return FALSE;
    }
    return TRUE;
}
