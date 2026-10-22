#ifndef AB_INFLATE_H
#define AB_INFLATE_H

#include "ab/base.h"

/* RFC 1951 raw stream. The caller owns the exact-size destination buffer. */
bool ab_inflate_raw(AbSpan input, uint8_t *output, size_t output_size, AbError *error);

#endif
