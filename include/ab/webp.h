#ifndef AB_WEBP_H
#define AB_WEBP_H

#include "ab/base.h"

typedef struct {
    uint32_t width, height;
    AbSpan color, alpha;
    bool lossless, has_alpha, compressed_alpha;
    uint8_t alpha_filter, alpha_preprocessing;
} AbWebpStill;

/* Read a bounded still-image RIFF WebP container and key-frame dimensions.
 * Return borrowed VP8/VP8L color and optional ALPH data (excluding its flag byte).
 * Reject animation, unsupported frame/header versions, inconsistent dimensions,
 * invalid chunk ordering/flags and malformed sizes/padding. Unknown metadata
 * chunks are skipped. This parses metadata only: no pixel decoding, color
 * conversion or color-profile handling. Output is unchanged on failure. */
bool ab_webp_still_read(AbSpan input, AbWebpStill *output, AbError *error);

/* Decode an opaque/raw ALPH channel or the palette-transform VP8L alpha path
 * used by all seven APK WebP images. Return owned, tightly packed alpha bytes.
 * Bound all simultaneously live heap workspace/output by allocation_limit;
 * recursive image metadata has a fixed two-level bound. Other transforms,
 * preprocessing and alpha inside a full VP8L color image remain unsupported.
 * No color decoding or GPU work. Output starts empty; failure frees all partial
 * storage. The encoded image's pixel extent terminates decoding implicitly. */
bool ab_webp_alpha_decode(const AbWebpStill *image, size_t allocation_limit,
                          AbBuffer *output, AbError *error);

/* Reverse raw WebP alpha prediction in place over a selected row range. Earlier
 * rows must already be reconstructed when first_row > 0. Padding stays unchanged;
 * storage covers (height-1)*stride+width bytes. Filters are 0=none, 1=horizontal,
 * 2=vertical, 3=clamped gradient, with original boundary predictors and modulo256
 * addition. No allocation; invalid bounds/filter reject before any mutation.
 * Alpha decompression and optional preprocessing are separate boundaries. */
bool ab_webp_alpha_unfilter(uint8_t *pixels, size_t size, uint32_t width,
                            uint32_t height, size_t stride, unsigned filter,
                            uint32_t first_row, uint32_t rows, AbError *error);

#endif
