#ifndef AB_ORACLE_ANDROID_STDLIB_H
#define AB_ORACLE_ANDROID_STDLIB_H

/* First-party declarations of the platform C ABI for isolated ARM diagnostics.
 * These headers provide no C library implementation and are never used by
 * ordinary project builds. */
#include <stddef.h>

void *malloc(size_t size);
void *calloc(size_t count, size_t size);
void free(void *pointer);

#endif
