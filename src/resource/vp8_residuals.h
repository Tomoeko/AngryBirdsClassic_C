#ifndef AB_VP8_RESIDUALS_H
#define AB_VP8_RESIDUALS_H

#include "vp8_modes.h"

typedef struct {
    uint8_t y[4], u[2], v[2], y2;
} AbVp8Nonzero;

/* Decode one raster macroblock into 24 consecutive 4x4 coefficient blocks:
 * sixteen Y, four U and four V. Apply inverse Walsh to Y2 when present.
 * Above state persists for each macroblock column; reset left state before
 * every row and all above state before the frame. Retain Y2 context through
 * small-luma blocks. Reader/context/output may contain a prefix on failure. */
bool ab_vp8_residuals_read(const AbVp8Header *header, AbVp8Boolean *reader,
                           const AbVp8Macroblock *block, AbVp8Nonzero *above,
                           AbVp8Nonzero *left, int16_t output[384], AbError *error);

#endif
