#ifndef AB_ORACLE_ANDROID_STRING_H
#define AB_ORACLE_ANDROID_STRING_H

#include <stddef.h>

void *memcpy(void *destination, const void *source, size_t size);
void *memset(void *destination, int value, size_t size);
int memcmp(const void *first, const void *second, size_t size);
char *strchr(const char *text, int character);
size_t strlen(const char *text);

#endif
