#include "ab/zip.h"

#include "ab/hash.h"
#include "ab/inflate.h"

#include <stdlib.h>
#include <string.h>

static bool find_end(AbSpan input, size_t *offset, AbError *error) {
    if (!input.data || input.size < 22) {
        return ab_fail(error, "ZIP end record is missing");
    }
    size_t minimum = input.size > 22 + 65535 ? input.size - 22 - 65535 : 0;
    size_t candidate = input.size - 22;
    for (;;) {
        const uint8_t *bytes = input.data + candidate;
        if (ab_u32le(bytes) == UINT32_C(0x06054b50) &&
            ab_u16le(bytes + 20) == input.size - candidate - 22) {
            *offset = candidate;
            return true;
        }
        if (candidate == minimum) {
            break;
        }
        --candidate;
    }
    return ab_fail(error, "ZIP end record is missing or has trailing data");
}

static bool validate_local(AbSpan input, AbZipEntry *entry, size_t central_start,
                           AbError *error) {
    if (!ab_range(central_start, entry->local_offset, 30)) {
        return ab_fail(error, "ZIP local header exceeds payload region");
    }
    const uint8_t *local = input.data + entry->local_offset;
    if (ab_u32le(local) != UINT32_C(0x04034b50) ||
        ab_u16le(local + 6) != entry->flags || ab_u16le(local + 8) != entry->method) {
        return ab_fail(error, "ZIP local header disagrees with directory");
    }
    size_t name_length = ab_u16le(local + 26);
    size_t extra_length = ab_u16le(local + 28);
    size_t variable_start = entry->local_offset + 30;
    if (!ab_range(central_start, variable_start, name_length + extra_length) ||
        name_length != entry->name.size ||
        memcmp(local + 30, entry->name.data, name_length) != 0) {
        return ab_fail(error, "ZIP local filename or extra data is invalid");
    }
    if (!(entry->flags & 8) && (ab_u32le(local + 14) != entry->crc32 ||
                                ab_u32le(local + 18) != entry->compressed_size ||
                                ab_u32le(local + 22) != entry->size)) {
        return ab_fail(error, "ZIP local sizes or CRC disagree with directory");
    }
    entry->data_offset = variable_start + name_length + extra_length;
    if (!ab_range(central_start, entry->data_offset, entry->compressed_size)) {
        return ab_fail(error, "ZIP compressed payload exceeds archive bounds");
    }
    entry->record_end = entry->data_offset + entry->compressed_size;
    if (entry->flags & 8) {
        size_t descriptor = entry->record_end;
        if (!ab_range(central_start, descriptor, 12)) {
            return ab_fail(error, "ZIP data descriptor is truncated");
        }
        if (ab_u32le(input.data + descriptor) == UINT32_C(0x08074b50)) {
            descriptor += 4;
        }
        if (!ab_range(central_start, descriptor, 12) ||
            ab_u32le(input.data + descriptor) != entry->crc32 ||
            ab_u32le(input.data + descriptor + 4) != entry->compressed_size ||
            ab_u32le(input.data + descriptor + 8) != entry->size) {
            return ab_fail(error, "ZIP data descriptor disagrees with directory");
        }
        entry->record_end = descriptor + 12;
    }
    return true;
}

static bool read_entry(AbSpan input, size_t *offset, size_t central_end,
                       size_t central_start, AbZipEntry *entry, AbError *error) {
    if (!ab_range(central_end, *offset, 46)) {
        return ab_fail(error, "ZIP directory entry is truncated");
    }
    const uint8_t *bytes = input.data + *offset;
    if (ab_u32le(bytes) != UINT32_C(0x02014b50)) {
        return ab_fail(error, "Invalid ZIP directory signature");
    }
    entry->flags = ab_u16le(bytes + 8);
    entry->method = ab_u16le(bytes + 10);
    entry->crc32 = ab_u32le(bytes + 16);
    entry->compressed_size = ab_u32le(bytes + 20);
    entry->size = ab_u32le(bytes + 24);
    size_t name_size = ab_u16le(bytes + 28);
    size_t variable_size = name_size + ab_u16le(bytes + 30) + ab_u16le(bytes + 32);
    entry->local_offset = ab_u32le(bytes + 42);
    if (ab_u16le(bytes + 34) != 0 || entry->compressed_size == UINT32_MAX ||
        entry->size == UINT32_MAX || entry->local_offset == UINT32_MAX) {
        return ab_fail(error, "Split archives and ZIP64 are unsupported");
    }
    if ((entry->flags & ~UINT16_C(0x080e)) ||
        (entry->method != 0 && entry->method != 8) ||
        (entry->method == 0 && (entry->flags & 6))) {
        return ab_fail(error, "Unsupported ZIP flags or compression method");
    }
    if (entry->method == 0 && entry->compressed_size != entry->size) {
        return ab_fail(error, "Stored ZIP entry sizes disagree");
    }
    if (!name_size || !ab_range(central_end, *offset + 46, variable_size)) {
        return ab_fail(error, "ZIP filename or extra data exceeds directory");
    }
    entry->name = (AbSpan){bytes + 46, name_size};
    if (memchr(entry->name.data, 0, name_size)) {
        return ab_fail(error, "ZIP filename contains a NUL byte");
    }
    *offset += 46 + variable_size;
    return validate_local(input, entry, central_start, error);
}

bool ab_zip_open(AbSpan input, AbZip *output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing ZIP output");
    }
    *output = (AbZip){0};
    size_t end_offset = 0;
    if (!find_end(input, &end_offset, error)) {
        return false;
    }
    const uint8_t *end = input.data + end_offset;
    size_t count = ab_u16le(end + 10);
    size_t central_size = ab_u32le(end + 12);
    size_t central_start = ab_u32le(end + 16);
    if (ab_u16le(end + 4) || ab_u16le(end + 6) || ab_u16le(end + 8) != count ||
        count == 65535 || central_size == UINT32_MAX || central_start == UINT32_MAX) {
        return ab_fail(error, "Split archives and ZIP64 are unsupported");
    }
    if (!ab_range(end_offset, central_start, central_size) ||
        central_size != end_offset - central_start || count > central_size / 46) {
        return ab_fail(error, "ZIP directory bounds or entry count are invalid");
    }
    AbZipEntry *entries = count ? calloc(count, sizeof(*entries)) : NULL;
    if (count && !entries) {
        return ab_fail(error, "Cannot allocate ZIP directory");
    }
    size_t offset = central_start;
    for (size_t index = 0; index < count; ++index) {
        if (!read_entry(input, &offset, end_offset, central_start, &entries[index],
                        error)) {
            free(entries);
            return false;
        }
        const AbZipEntry *current = entries + index;
        for (size_t previous = 0; previous < index; ++previous) {
            const AbZipEntry *other = entries + previous;
            if (current->name.size == other->name.size &&
                memcmp(current->name.data, other->name.data, current->name.size) == 0) {
                free(entries);
                return ab_fail(error, "ZIP contains duplicate filenames");
            }
            if (current->local_offset < other->record_end &&
                other->local_offset < current->record_end) {
                free(entries);
                return ab_fail(error, "ZIP local records overlap");
            }
        }
    }
    if (offset != end_offset) {
        free(entries);
        return ab_fail(error, "ZIP directory contains unparsed bytes");
    }
    *output = (AbZip){input, entries, count};
    return true;
}

void ab_zip_close(AbZip *zip) {
    if (zip) {
        free(zip->entries);
        *zip = (AbZip){0};
    }
}

const AbZipEntry *ab_zip_find(const AbZip *zip, const char *name) {
    if (!zip || !name) {
        return NULL;
    }
    size_t size = strlen(name);
    for (size_t index = 0; index < zip->count; ++index) {
        const AbZipEntry *entry = zip->entries + index;
        if (entry->name.size == size && memcmp(entry->name.data, name, size) == 0) {
            return entry;
        }
    }
    return NULL;
}

bool ab_zip_read(const AbZip *zip, size_t index, size_t limit, AbBuffer *output,
                 AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing ZIP extraction output");
    }
    *output = (AbBuffer){0};
    if (!zip || index >= zip->count) {
        return ab_fail(error, "ZIP entry index is out of range");
    }
    const AbZipEntry *entry = zip->entries + index;
    if (entry->size > limit) {
        return ab_fail(error, "ZIP entry exceeds extraction size limit");
    }
    uint8_t *bytes = malloc(entry->size ? entry->size : 1);
    if (!bytes) {
        return ab_fail(error, "Cannot allocate ZIP extraction buffer");
    }
    AbSpan compressed = {zip->input.data + entry->data_offset, entry->compressed_size};
    if (entry->method == 8) {
        if (!ab_inflate_raw(compressed, bytes, entry->size, error)) {
            free(bytes);
            return false;
        }
    } else if (entry->size) {
        memcpy(bytes, compressed.data, entry->size);
    }
    AbSpan decoded = {bytes, entry->size};
    if (ab_crc32(decoded) != entry->crc32) {
        free(bytes);
        return ab_fail(error, "ZIP entry CRC-32 mismatch");
    }
    *output = (AbBuffer){bytes, entry->size};
    return true;
}
