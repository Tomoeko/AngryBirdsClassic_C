#include "vp8_modes.h"

#include <string.h>

static bool read_segment(const AbVp8Segments *segments, AbVp8Boolean *reader,
                         uint8_t *output, AbError *error) {
    *output = 0;
    if (!segments->enabled || !segments->update_map) {
        return true;
    }
    uint32_t side, low;
    if (!ab_vp8_boolean_bit(reader, segments->probabilities[0], &side, error) ||
        !ab_vp8_boolean_bit(reader, segments->probabilities[1 + side], &low, error)) {
        return false;
    }
    *output = (uint8_t)(side * 2 + low);
    return true;
}

static bool read_large_mode(AbVp8Boolean *reader, bool chroma, uint8_t *output,
                            AbError *error) {
    uint32_t branch, leaf;
    if (!ab_vp8_boolean_bit(reader, chroma ? 142 : 156, &branch, error)) {
        return false;
    }
    if (chroma && !branch) {
        *output = AB_VP8_PRED_DC;
        return true;
    }
    uint8_t probability = chroma ? 114 : (branch ? 128 : 163);
    if (!ab_vp8_boolean_bit(reader, probability, &leaf, error)) {
        return false;
    }
    if (chroma && leaf) {
        if (!ab_vp8_boolean_bit(reader, 183, &leaf, error)) {
            return false;
        }
        *output = leaf ? AB_VP8_PRED_TM : AB_VP8_PRED_HORIZONTAL;
    } else if (chroma) {
        *output = AB_VP8_PRED_VERTICAL;
    } else if (branch) {
        *output = leaf ? AB_VP8_PRED_TM : AB_VP8_PRED_HORIZONTAL;
    } else {
        *output = leaf ? AB_VP8_PRED_VERTICAL : AB_VP8_PRED_DC;
    }
    return true;
}

static bool read_small_mode(AbVp8Boolean *reader, uint8_t above, uint8_t left,
                            uint8_t *output, AbError *error) {
    /* The format tree's leaves are negated prediction modes; positive entries
     * select another pair of branches. Zero is the DC leaf. */
    static const int8_t branches[18] = {0,  2,  -1, 4,  -2, 6,  8,  12, -3,
                                        10, -5, -6, -4, 14, -7, 16, -8, -9};
    const uint8_t *probabilities = ab_vp8_intra4_probabilities[above][left];
    unsigned node = 0;
    for (;;) {
        uint32_t bit;
        if (!ab_vp8_boolean_bit(reader, probabilities[node / 2], &bit, error)) {
            return false;
        }
        int next = branches[node + bit];
        if (next <= 0) {
            *output = (uint8_t)-next;
            return true;
        }
        node = (unsigned)next;
    }
}

static bool read_luma(AbVp8Boolean *reader, uint8_t above[4], uint8_t left[4],
                      AbVp8Macroblock *block, AbError *error) {
    uint32_t large;
    if (!ab_vp8_boolean_bit(reader, 145, &large, error)) {
        return false;
    }
    block->luma4 = !large;
    if (large) {
        if (!read_large_mode(reader, false, block->luma, error)) {
            return false;
        }
        memset(above, block->luma[0], 4);
        memset(left, block->luma[0], 4);
        return true;
    }
    for (size_t row = 0; row < 4; ++row) {
        for (size_t column = 0; column < 4; ++column) {
            uint8_t mode;
            if (!read_small_mode(reader, above[column], left[row], &mode, error)) {
                return false;
            }
            block->luma[row * 4 + column] = mode;
            above[column] = left[row] = mode;
        }
    }
    return true;
}

bool ab_vp8_modes_read_row(AbVp8Header *header, uint8_t *above, size_t above_count,
                           AbVp8Macroblock *output, size_t capacity, AbError *error) {
    if (!header || !header->macroblock_width || header->macroblock_width > 1024 ||
        !above || above_count < header->macroblock_width * 4 || !output ||
        capacity < header->macroblock_width) {
        return ab_fail(error, "Invalid VP8 prediction row/context capacity");
    }
    for (size_t index = 0; index < header->macroblock_width * 4; ++index) {
        if (above[index] > AB_VP8_PRED_HORIZONTAL_UP) {
            return ab_fail(error, "Invalid VP8 above prediction context");
        }
    }
    uint8_t left[4] = {0};
    for (size_t column = 0; column < header->macroblock_width; ++column) {
        AbVp8Macroblock block = {0};
        uint32_t skip = 0;
        if (!read_segment(&header->segments, &header->control, &block.segment, error) ||
            (header->use_skip_probability &&
             !ab_vp8_boolean_bit(&header->control, header->skip_probability, &skip,
                                 error))) {
            return false;
        }
        block.skip = skip != 0;
        if (!read_luma(&header->control, above + column * 4, left, &block, error) ||
            !read_large_mode(&header->control, true, &block.chroma, error)) {
            return false;
        }
        output[column] = block;
    }
    return true;
}
