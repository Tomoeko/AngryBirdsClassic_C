#ifndef AB_VP8_MODES_H
#define AB_VP8_MODES_H

#include "vp8_header.h"

/* VP8 format mode numbering. The original decoder uses a different ordering
 * for three diagonal modes; convert at evidence boundaries only. */
typedef enum {
    AB_VP8_PRED_DC,
    AB_VP8_PRED_TM,
    AB_VP8_PRED_VERTICAL,
    AB_VP8_PRED_HORIZONTAL,
    AB_VP8_PRED_DOWN_LEFT,
    AB_VP8_PRED_DOWN_RIGHT,
    AB_VP8_PRED_VERTICAL_RIGHT,
    AB_VP8_PRED_VERTICAL_LEFT,
    AB_VP8_PRED_HORIZONTAL_DOWN,
    AB_VP8_PRED_HORIZONTAL_UP
} AbVp8Prediction;

typedef struct {
    uint8_t segment, chroma;
    bool skip, luma4;
    uint8_t luma[16];
} AbVp8Macroblock;

/* Decode a complete raster row from the header's continuing control reader.
 * The caller owns width*4 above modes, initialized to DC before the first row,
 * and width output records. Above modes persist across rows; left modes reset
 * to DC for each row. No allocations. A malformed row may leave a decoded
 * prefix in reader/context/output; discard that frame after failure. */
bool ab_vp8_modes_read_row(AbVp8Header *header, uint8_t *above, size_t above_count,
                           AbVp8Macroblock *output, size_t capacity, AbError *error);

#endif
