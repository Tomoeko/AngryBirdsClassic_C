#ifndef AB_DAT_H
#define AB_DAT_H

#include "ab/base.h"

typedef struct {
    AbSpan bytes;
    size_t offset;
} AbDatReader;

typedef struct {
    AbSpan tag;
    AbSpan payload;
} AbDatChunk;

/* All spans borrow input. Container sizes and numeric fields are big endian. */
bool ab_dat_open(AbSpan input, const char tag[4], AbDatReader *reader, AbError *error);
bool ab_dat_next(AbDatReader *reader, AbDatChunk *chunk, AbError *error);
bool ab_dat_bytes(AbDatReader *reader, size_t count, AbSpan *output, AbError *error);
bool ab_dat_u16(AbDatReader *reader, uint16_t *output, AbError *error);
bool ab_dat_u32(AbDatReader *reader, uint32_t *output, AbError *error);
bool ab_dat_i16(AbDatReader *reader, int16_t *output, AbError *error);
/* Retains encoded bytes; does not transcode the native readUTF string. */
bool ab_dat_string(AbDatReader *reader, AbSpan *output, AbError *error);

#endif
