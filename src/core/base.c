#include "ab/base.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

bool ab_fail(AbError *error, const char *format, ...) {
    if (error) {
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(error->message, sizeof(error->message), format, arguments);
        va_end(arguments);
    }
    return false;
}

bool ab_range(size_t size, size_t offset, size_t length) {
    return offset <= size && length <= size - offset;
}

bool ab_slice(AbSpan input, size_t offset, size_t length, AbSpan *output) {
    if (!output || !ab_range(input.size, offset, length) ||
        (!input.data && input.size)) {
        return false;
    }
    output->data = input.data ? input.data + offset : NULL;
    output->size = length;
    return true;
}

uint16_t ab_u16le(const uint8_t *bytes) {
    return (uint16_t)((uint16_t)bytes[0] | (uint16_t)((uint16_t)bytes[1] << 8));
}

uint32_t ab_u32le(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
           (uint32_t)bytes[3] << 24;
}

uint32_t ab_u32be(const uint8_t *bytes) {
    return (uint32_t)bytes[0] << 24 | (uint32_t)bytes[1] << 16 |
           (uint32_t)bytes[2] << 8 | (uint32_t)bytes[3];
}

bool ab_file_read(const char *path, size_t limit, AbBuffer *output, AbError *error) {
    FILE *file = NULL;
    uint8_t *bytes = NULL;
    long length = 0;
    bool okay = false;

    if (!output) {
        return ab_fail(error, "Missing file output");
    }
    *output = (AbBuffer){0};
    if (!path) {
        return ab_fail(error, "Missing input path");
    }
    file = fopen(path, "rb");
    if (!file) {
        return ab_fail(error, "Cannot open input file");
    }
    if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 ||
        (uintmax_t)length > (uintmax_t)limit || fseek(file, 0, SEEK_SET) != 0) {
        ab_fail(error, "Cannot determine input size or input exceeds size limit");
        goto release_file;
    }
    bytes = malloc(length ? (size_t)length : 1);
    if (!bytes) {
        ab_fail(error, "Cannot allocate input buffer");
        goto release_file;
    }
    if (fread(bytes, 1, (size_t)length, file) != (size_t)length || fgetc(file) != EOF ||
        ferror(file)) {
        ab_fail(error, "Input changed or could not be read completely");
        goto release_file;
    }
    if (fclose(file) != 0) {
        file = NULL;
        ab_fail(error, "Cannot close input file");
        goto release_file;
    }
    file = NULL;
    *output = (AbBuffer){bytes, (size_t)length};
    bytes = NULL;
    okay = true;

release_file:
    free(bytes);
    if (file) {
        fclose(file);
    }
    return okay;
}

bool ab_file_write(const char *path, AbSpan input, AbError *error) {
    if (!path || (!input.data && input.size)) {
        return ab_fail(error, "Invalid output file arguments");
    }
    FILE *file = fopen(path, "wbx");
    if (!file) {
        return ab_fail(error,
                       "Cannot create output file; existing files are preserved");
    }
    bool okay = !input.size || fwrite(input.data, 1, input.size, file) == input.size;
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay || ab_fail(error, "Cannot write output file completely");
}

void ab_buffer_free(AbBuffer *buffer) {
    if (buffer) {
        free(buffer->data);
        *buffer = (AbBuffer){0};
    }
}
