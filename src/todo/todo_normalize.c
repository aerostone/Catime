/**
 * @file todo_normalize.c
 * @brief Full-width to half-width normalization for todo.txt input.
 *
 * Mapping (UTF-8 aware, explicit structural allowlist):
 * - （）:;＋＠．／－ ０-９ Ａ-Ｃ/ａ-ｃ ｘ(id/due/pin letters) -> ASCII
 * - U+3000 ideographic space -> 0x20
 * All other codepoints (CJK prose punctuation, hanzi) copy through.
 */
#include <string.h>

#include "todo_normalize.h"

static int DecodeUtf8(const char *s, unsigned *cp) {
    unsigned char c0 = (unsigned char)s[0];
    if (c0 < 0x80) {
        *cp = c0;
        return 1;
    }
    if ((c0 & 0xE0) == 0xC0) {
        unsigned char c1 = (unsigned char)s[1];
        if ((c1 & 0xC0) != 0x80 || c0 < 0xC2) return -1;
        *cp = ((unsigned)(c0 & 0x1F) << 6) | (c1 & 0x3F);
        return 2;
    }
    if ((c0 & 0xF0) == 0xE0) {
        unsigned char c1 = (unsigned char)s[1];
        unsigned char c2 = (unsigned char)s[2];
        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80) return -1;
        *cp = ((unsigned)(c0 & 0x0F) << 12) | ((unsigned)(c1 & 0x3F) << 6) |
              (c2 & 0x3F);
        if (*cp < 0x800) return -1; /* overlong */
        return 3;
    }
    return -1; /* 4-byte (emoji) copies through below */
}

/* Structural fullwidth chars only; CJK prose （，。！？、…） untouched. */
static int MapCodepoint(unsigned cp, char *out) {
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp == 0x3000) {
        out[0] = ' ';
        return 1;
    }
    switch (cp) {
    case 0xFF08: out[0] = '('; return 1; /* （ */
    case 0xFF09: out[0] = ')'; return 1; /* ） */
    case 0xFF1A: out[0] = ':'; return 1; /* ： */
    case 0xFF0B: out[0] = '+'; return 1; /* ＋ */
    case 0xFF20: out[0] = '@'; return 1; /* ＠ */
    case 0xFF0E: out[0] = '.'; return 1; /* ． */
    case 0xFF0F: out[0] = '/'; return 1; /* ／ */
    case 0xFF0D: out[0] = '-'; return 1; /* － */
    case 0xFF10: case 0xFF11: case 0xFF12: case 0xFF13: case 0xFF14:
    case 0xFF15: case 0xFF16: case 0xFF17: case 0xFF18: case 0xFF19:
        out[0] = (char)('0' + (cp - 0xFF10)); /* ０-９ */
        return 1;
    case 0xFF21: case 0xFF22: case 0xFF23: /* ＡＢＣ priority letters */
        out[0] = (char)('A' + (cp - 0xFF21));
        return 1;
    case 0xFF41: case 0xFF42: case 0xFF43: /* ａｂｃ */
        out[0] = (char)('A' + (cp - 0xFF41));
        return 1;
    case 0xFF58: out[0] = 'x'; return 1; /* ｘ done marker */
    case 0xFF49: out[0] = 'i'; return 1; /* ｉ in id: */
    case 0xFF44: out[0] = 'd'; return 1; /* ｄ in due:/id: */
    case 0xFF55: out[0] = 'u'; return 1; /* ｕ */
    case 0xFF45: out[0] = 'e'; return 1; /* ｅ */
    case 0xFF50: out[0] = 'p'; return 1; /* ｐ in pin: */
    case 0xFF4E: out[0] = 'n'; return 1; /* ｎ */
    default: break;
    }
    return 0; /* copy original bytes */
}

void TodoNormalize_Copy(const char *src, char *dst, size_t cap) {
    if (!dst || cap == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    size_t w = 0;
    size_t i = 0;
    while (src[i] && w + 1 < cap) {
        unsigned char c = (unsigned char)src[i];
        if (c < 0x80) {
            dst[w++] = src[i++];
            continue;
        }
        unsigned cp = 0;
        int len = DecodeUtf8(src + i, &cp);
        if (len < 0) {
            /* invalid byte or 4-byte: copy raw byte, stay in sync */
            dst[w++] = src[i++];
            continue;
        }
        char mapped = 0;
        if (MapCodepoint(cp, &mapped)) {
            if (w + 1 >= cap) break;
            dst[w++] = mapped;
            i += (size_t)len;
        } else {
            if (w + (size_t)len >= cap) break;
            memcpy(dst + w, src + i, (size_t)len);
            w += (size_t)len;
            i += (size_t)len;
        }
    }
    dst[w] = '\0';
}

void TodoNormalize_InPlace(char *utf8) {
    if (!utf8) return;
    /* output <= input length: normalize via temp on stack-safe chunks */
    char tmp[512];
    size_t n = strlen(utf8);
    if (n < sizeof(tmp)) {
        TodoNormalize_Copy(utf8, tmp, sizeof(tmp));
        memcpy(utf8, tmp, strlen(tmp) + 1);
        return;
    }
    /* long line: process in place with read/write cursors (write <= read) */
    size_t r = 0, w = 0;
    while (utf8[r]) {
        unsigned char c = (unsigned char)utf8[r];
        if (c < 0x80) {
            utf8[w++] = utf8[r++];
            continue;
        }
        unsigned cp = 0;
        int len = DecodeUtf8(utf8 + r, &cp);
        if (len < 0) {
            utf8[w++] = utf8[r++];
            continue;
        }
        char mapped = 0;
        if (MapCodepoint(cp, &mapped)) {
            utf8[w++] = mapped;
            r += (size_t)len;
        } else {
            memmove(utf8 + w, utf8 + r, (size_t)len);
            w += (size_t)len;
            r += (size_t)len;
        }
    }
    utf8[w] = '\0';
}
