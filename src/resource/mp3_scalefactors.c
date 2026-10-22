#include "ab/mp3.h"

/* Format partition endpoints, in the decoder's long, short and mixed order.
 * Endpoints keep the cumulative band layout explicit; widths derive below from
 * the transmitted compression value. */
static const uint8_t partition_ends[3][6][4] = {
    {{6, 11, 16, 21},
     {6, 11, 18, 21},
     {11, 21, 21, 21},
     {7, 14, 21, 21},
     {6, 12, 18, 21},
     {8, 16, 21, 21}},
    {{9, 18, 27, 36},
     {9, 18, 30, 36},
     {18, 36, 36, 36},
     {12, 24, 36, 36},
     {12, 21, 30, 36},
     {15, 27, 36, 36}},
    {{6, 15, 24, 33},
     {6, 15, 27, 33},
     {15, 33, 33, 33},
     {6, 21, 33, 33},
     {6, 18, 27, 33},
     {6, 24, 33, 33}},
};

static size_t compression_widths(uint16_t encoded, bool intensity, uint8_t width[4],
                                 bool *preflag) {
    unsigned value = encoded;
    *preflag = false;
    if (intensity) {
        value /= 2;
        if (value < 180) {
            width[0] = (uint8_t)(value / 36);
            width[1] = (uint8_t)(value % 36 / 6);
            width[2] = (uint8_t)(value % 6);
            return 3;
        }
        if (value < 244) {
            value -= 180;
            width[0] = (uint8_t)(value / 16);
            width[1] = (uint8_t)(value % 16 / 4);
            width[2] = (uint8_t)(value % 4);
            return 4;
        }
        value -= 244;
        width[0] = (uint8_t)(value / 3);
        width[1] = (uint8_t)(value % 3);
        return 5;
    }
    if (value < 400) {
        width[0] = (uint8_t)(value / 80);
        width[1] = (uint8_t)(value % 80 / 16);
        width[2] = (uint8_t)(value % 16 / 4);
        width[3] = (uint8_t)(value % 4);
        return 0;
    }
    if (value < 500) {
        value -= 400;
        width[0] = (uint8_t)(value / 20);
        width[1] = (uint8_t)(value % 20 / 4);
        width[2] = (uint8_t)(value % 4);
        return 1;
    }
    value -= 500;
    width[0] = (uint8_t)(value / 3);
    width[1] = (uint8_t)(value % 3);
    *preflag = true;
    return 2;
}

bool ab_mp3_scalefactors(const AbMp3Granule *granule, bool intensity, AbSpan main_data,
                         size_t bit_offset, AbMp3Scalefactors *output, AbError *error) {
    if (!granule || !output || main_data.size > 1024 ||
        (main_data.size && !main_data.data) || bit_offset > main_data.size * 8 ||
        granule->scalefactor_compress > 511 || granule->block_type > 3 ||
        granule->part23_length > 4095) {
        return ab_fail(error, "Invalid MPEG-2 scalefactor input");
    }
    AbMp3Scalefactors scales = {0};
    uint8_t width[4] = {0};
    size_t pattern = compression_widths(granule->scalefactor_compress, intensity, width,
                                        &scales.preflag);
    size_t layout = granule->block_type != 2 ? 0u : granule->mixed_block ? 2u : 1u;
    const uint8_t *ends = partition_ends[layout][pattern];
    size_t bits = 0, start = 0;
    for (size_t group = 0; group < 4; ++group) {
        bits += (size_t)width[group] * (ends[group] - start);
        start = ends[group];
    }
    if (bits > granule->part23_length || bits > main_data.size * 8 - bit_offset) {
        return ab_fail(error, "Truncated MPEG-2 scalefactors or part23 length");
    }
    scales.bit_count = (uint16_t)bits;
    scales.data_count = ends[3];
    scales.count = layout == 0 ? 22 : layout == 1 ? 39 : 38;
    start = 0;
    for (size_t group = 0; group < 4; ++group) {
        for (size_t index = start; index < ends[group]; ++index) {
            uint32_t value = 0;
            for (unsigned bit = 0; bit < width[group]; ++bit) {
                unsigned shift = 7u - (unsigned)(bit_offset % 8);
                value = value * 2 +
                        ((uint32_t)main_data.data[bit_offset / 8] >> shift & 1u);
                ++bit_offset;
            }
            scales.values[index] = (uint8_t)value;
        }
        start = ends[group];
    }
    *output = scales;
    return true;
}
