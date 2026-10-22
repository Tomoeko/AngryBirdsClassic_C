#ifndef AB_VP8_TRANSFORM_H
#define AB_VP8_TRANSFORM_H

#include <stdint.h>
#include <stddef.h>

/* Invert one dequantized Y2 block into the sixteen luma block DC values.
 * Caller owns both sixteen-element arrays; input/output may alias. */
void ab_vp8_walsh_inverse(const int16_t input[16], int16_t output[16]);

/* Invert a 4x4 coefficient block and add/clamp to an existing predictor.
 * The caller supplies four writable rows, each with at least four bytes,
 * separated by stride bytes (stride >= 4). No other pixels are changed. */
void ab_vp8_dct_add(const int16_t input[16], uint8_t *pixels, size_t stride);

#endif
