#ifndef AB_VP8_PREDICT_H
#define AB_VP8_PREDICT_H

#include "vp8_modes.h"

/* Kernels write one predictor into padded reconstruction storage. The caller
 * supplies top/left/top-left neighbors and four rows (small) or size rows
 * (large), with stride >= size. Small prediction also reads eight top pixels.
 * Small stride is at least 8. Large size is 8 or 16; above/left availability
 * only changes DC averaging.
 * Other modes consume the caller's initialized border pixels. */
bool ab_vp8_predict_large(AbVp8Prediction mode, size_t size, bool above_available,
                          bool left_available, uint8_t *pixels, size_t stride,
                          AbError *error);
bool ab_vp8_predict_small(AbVp8Prediction mode, uint8_t *pixels, size_t stride,
                          AbError *error);

#endif
