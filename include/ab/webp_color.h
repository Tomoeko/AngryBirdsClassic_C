#ifndef AB_WEBP_COLOR_H
#define AB_WEBP_COLOR_H

#include "ab/base.h"

typedef struct {
    AbSpan top_y, bottom_y;
    AbSpan top_u, top_v, bottom_u, bottom_v;
    uint32_t width;
} AbWebpYuvPair;

/* Native RGBA line-pair reconstruction: interpolate chroma at quarter-pixel
 * positions, round once, then apply the target's fixed-point YUV conversion.
 * Top luma is required; empty bottom luma omits that output row. All four chroma
 * spans need ceil(width/2) bytes even without bottom luma. Width is 1..16384.
 * Output buffers are borrowed writable views, independent of inputs/each other;
 * write width*4 bytes per present row and preserve any trailing storage. No
 * allocation, plane decoding, alpha application or GPU work. Validate every
 * span/capacity before writes; invalid input leaves both outputs unchanged. */
bool ab_webp_upsample_rgba(const AbWebpYuvPair *pair, AbBuffer top_rgba,
                           AbBuffer bottom_rgba, AbError *error);

typedef struct {
    uint32_t width, height;
    AbSpan y, u, v;
} AbWebpYuv420;

/* Convert tightly packed Y/U/V planes into a borrowed tightly packed RGBA view.
 * Chroma dimensions are ceil(width/2),ceil(height/2). Clamp the first/last chroma
 * rows as the native line-pair caller does. Optional alpha is empty for opaque,
 * otherwise contains width*height bytes. Dimensions are 1..16384. Input/output
 * views must be disjoint and serialized. No allocation, compressed VP8 decoding,
 * premultiplication or GPU work. Bounds failures precede all writes. */
bool ab_webp_yuv420_rgba(const AbWebpYuv420 *image, AbSpan alpha, AbBuffer rgba,
                         AbError *error);

#endif
