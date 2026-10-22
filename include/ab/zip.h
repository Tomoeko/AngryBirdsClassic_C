#ifndef AB_ZIP_H
#define AB_ZIP_H

#include "ab/base.h"

typedef struct {
    AbSpan name;
    uint16_t method;
    uint16_t flags;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t size;
    size_t local_offset;
    size_t data_offset;
    size_t record_end;
} AbZipEntry;

typedef struct {
    /* Archive bytes remain borrowed until ab_zip_close. Entries are owned. */
    AbSpan input;
    AbZipEntry *entries;
    size_t count;
} AbZip;

bool ab_zip_open(AbSpan input, AbZip *output, AbError *error);
void ab_zip_close(AbZip *zip);
const AbZipEntry *ab_zip_find(const AbZip *zip, const char *name);
bool ab_zip_read(const AbZip *zip, size_t index, size_t limit, AbBuffer *output,
                 AbError *error);

#endif
