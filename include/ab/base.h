#ifndef AB_BASE_H
#define AB_BASE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const uint8_t *data;
    size_t size;
} AbSpan;

typedef struct {
    uint8_t *data;
    size_t size;
} AbBuffer;

typedef struct {
    char message[256];
} AbError;

bool ab_fail(AbError *error, const char *format, ...);
bool ab_range(size_t size, size_t offset, size_t length);
bool ab_slice(AbSpan input, size_t offset, size_t length, AbSpan *output);
uint16_t ab_u16le(const uint8_t *bytes);
uint32_t ab_u32le(const uint8_t *bytes);
uint32_t ab_u32be(const uint8_t *bytes);

/* Outputs start empty. The caller owns successful buffers and frees them here. */
bool ab_file_read(const char *path, size_t limit, AbBuffer *output, AbError *error);
bool ab_file_write(const char *path, AbSpan input, AbError *error);
void ab_buffer_free(AbBuffer *buffer);

#endif
