#ifndef AB_VP8_HEADER_H
#define AB_VP8_HEADER_H

#include "vp8_boolean.h"
#include "vp8_tables.h"

typedef struct {
    bool enabled, update_map, absolute;
    int8_t quantizers[4], filters[4];
    uint8_t probabilities[3];
} AbVp8Segments;

typedef struct {
    bool simple, use_deltas;
    uint8_t level, sharpness;
    int32_t reference_deltas[4], mode_deltas[4];
} AbVp8Filter;

enum {
    AB_VP8_Y1_DC,
    AB_VP8_Y1_AC,
    AB_VP8_Y2_DC,
    AB_VP8_Y2_AC,
    AB_VP8_UV_DC,
    AB_VP8_UV_AC,
    AB_VP8_UV_QUANTIZER,
    AB_VP8_QUANTIZER_VALUES
};

typedef struct {
    AbSpan frame;
    AbVp8Boolean control;
    uint32_t width, height, macroblock_width, macroblock_height;
    uint32_t first_partition_size, partition_count;
    uint8_t profile, x_scale, y_scale, base_quantizer;
    bool colorspace, clamp, refresh_entropy, use_skip_probability;
    uint8_t skip_probability;
    int8_t quantizer_deltas[5];
    AbVp8Segments segments;
    AbVp8Filter filter;
    int32_t quantizers[4][AB_VP8_QUANTIZER_VALUES];
    uint8_t coefficient_probabilities[AB_VP8_COEFFICIENT_PROBABILITIES];
    AbSpan partitions[8];
} AbVp8Header;

/* Parse one displayed VP8 key frame through coefficient/skip probabilities.
 * Borrow all encoded bytes until decoding is finished. Preserve the arithmetic
 * control cursor for subsequent macroblock modes and expose bounded token
 * partition slices. Start fresh single-image segmentation/filter state; no
 * inter-frame references, pixel reconstruction, allocation or native runtime.
 * Output is unchanged on malformed/truncated input. Arithmetic padding/EOF is
 * limited to the checked reader's supplied-input domain. */
bool ab_vp8_header_read(AbSpan frame, AbVp8Header *output, AbError *error);

#endif
