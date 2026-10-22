#ifndef AB_VP8_COEFFICIENTS_H
#define AB_VP8_COEFFICIENTS_H

#include "vp8_boolean.h"

/* Decode one VP8 coefficient block with probabilities[band][context][node].
 * first is 0 or 1, context is 0..2, and quantizers are DC/AC in 0..440.
 * Write decoded nonzero values in raster order, leaving other output values
 * untouched. The caller clears unused coefficients and may supply Y2 DC at
 * output[0] when first is 1. Values retain the native low signed 16 bits.
 * Return the next scan position after the final token, including zero runs;
 * first denotes an immediate EOB. Failure may leave a decoded prefix. */
bool ab_vp8_coefficients_read(AbVp8Boolean *reader, const uint8_t probabilities[264],
                              unsigned context, const int32_t quantizers[2],
                              unsigned first, int16_t output[16], unsigned *end,
                              AbError *error);

#endif
