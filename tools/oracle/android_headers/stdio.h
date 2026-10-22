#ifndef AB_ORACLE_ANDROID_STDIO_H
#define AB_ORACLE_ANDROID_STDIO_H

#include <stddef.h>
#include <stdarg.h>

typedef struct AbAndroidPlatformFile FILE;

#define SEEK_SET 0
#define SEEK_END 2
#define EOF (-1)

FILE *fopen(const char *path, const char *mode);
int fclose(FILE *file);
int fseek(FILE *file, long offset, int origin);
long ftell(FILE *file);
size_t fread(void *output, size_t size, size_t count, FILE *file);
size_t fwrite(const void *input, size_t size, size_t count, FILE *file);
int fgetc(FILE *file);
int ferror(FILE *file);
int vsnprintf(char *output, size_t capacity, const char *format, va_list arguments);

#endif
