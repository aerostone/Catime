/**
 * @file todo_normalize.h
 * @brief UTF-8 full-width punctuation normalization for todo.txt input.
 *
 * Idempotent: normalize(normalize(x)) == normalize(x).
 * Only structural characters are mapped (+ @ : ( ) space digits . / -);
 * CJK prose punctuation (，。！？、…) passes through untouched.
 */
#ifndef CATIME_TODO_NORMALIZE_H
#define CATIME_TODO_NORMALIZE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Normalize UTF-8 in place (output never longer than input). */
void TodoNormalize_InPlace(char *utf8);

/* Normalize UTF-8 src into dst (cap bytes incl. NUL). Truncates safely. */
void TodoNormalize_Copy(const char *src, char *dst, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* CATIME_TODO_NORMALIZE_H */
