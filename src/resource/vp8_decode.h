#ifndef AB_VP8_DECODE_H
#define AB_VP8_DECODE_H

#include "ab/base.h"

typedef struct {
    uint32_t width, height;
    /* Owned tightly packed Y, U, V; chroma dimensions divide by two, rounding up. */
    AbBuffer yuv;
} AbVp8Image;

/* Decode one profile-zero displayed key frame with one token partition.
 * Bound all simultaneously live heap output/workspace by allocation_limit.
 * bypass_filter supports isolated reconstruction comparisons; normal WebP
 * decoding must enable filtering. No alpha, RGB conversion or GPU resources.
 * Output starts empty; free successful yuv with ab_buffer_free. */
bool ab_vp8_decode(AbSpan input, size_t allocation_limit, bool bypass_filter,
                   AbVp8Image *output, AbError *error);

#endif
