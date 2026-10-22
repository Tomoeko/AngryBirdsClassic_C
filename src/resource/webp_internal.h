#ifndef AB_WEBP_INTERNAL_H
#define AB_WEBP_INTERNAL_H

#include "ab/base.h"

typedef struct {
    AbSpan bytes;
    size_t offset;
    unsigned bit;
} AbWebpBits;

/* VP8L fields are consumed least-significant bit first. Borrow bytes and keep
 * offset/bit private to this reader. Counts 0..24 match the target field reader.
 * Empty input permits only zero-bit reads. No allocation or padding/fallback;
 * invalid/truncated reads preserve both state and output. */
bool ab_webp_bits_init(AbSpan bytes, AbWebpBits *output, AbError *error);
bool ab_webp_bits_read(AbWebpBits *reader, unsigned count, uint32_t *output,
                       AbError *error);

#endif
