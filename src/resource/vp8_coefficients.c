#include "vp8_coefficients.h"
#include "vp8_math.h"

static bool read_magnitude(AbVp8Boolean *reader, const uint8_t probabilities[11],
                           uint32_t *output, AbError *error) {
    /* Nodes 2..10 describe positive magnitudes after the zero/EOB decisions. */
    static const int8_t branches[18] = {-1, 2,  4,  8,  -2, 6,  -3, -4, 10,
                                        12, -5, -6, 14, 16, -7, -8, -9, -10};
    static const uint16_t bases[6] = {5, 7, 11, 19, 35, 67};
    static const uint8_t extra[6][11] = {
        {159},
        {165, 145},
        {173, 148, 140},
        {176, 155, 140, 135},
        {180, 157, 141, 134, 130},
        {254, 254, 243, 230, 196, 177, 153, 140, 133, 130, 129}};
    unsigned node = 0;
    int token;
    for (;;) {
        uint32_t bit;
        if (!ab_vp8_boolean_bit(reader, probabilities[2 + node / 2], &bit, error)) {
            return false;
        }
        token = -branches[node + bit];
        if (token > 0) {
            break;
        }
        node = (unsigned)-token;
    }
    if (token <= 4) {
        *output = (uint32_t)token;
        return true;
    }
    unsigned category = (unsigned)token - 5;
    uint32_t offset = 0;
    unsigned count = category == 5 ? 11 : category + 1;
    for (unsigned index = 0; index < count; ++index) {
        uint32_t bit;
        if (!ab_vp8_boolean_bit(reader, extra[category][index], &bit, error)) {
            return false;
        }
        offset = offset * 2 + bit;
    }
    *output = bases[category] + offset;
    return true;
}

bool ab_vp8_coefficients_read(AbVp8Boolean *reader, const uint8_t probabilities[264],
                              unsigned context, const int32_t quantizers[2],
                              unsigned first, int16_t output[16], unsigned *end,
                              AbError *error) {
    static const uint8_t bands[16] = {0, 1, 2, 3, 6, 4, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7};
    static const uint8_t raster[16] = {0, 1,  4,  8,  5, 2,  3,  6,
                                       9, 12, 13, 10, 7, 11, 14, 15};
    if (!reader || !probabilities || !quantizers || !output || !end || context > 2 ||
        first > 1 || quantizers[0] < 0 || quantizers[0] > 440 || quantizers[1] < 0 ||
        quantizers[1] > 440) {
        return ab_fail(error, "Invalid VP8 coefficient block input");
    }
    bool allow_end = true;
    unsigned position = first;
    while (position < 16) {
        const uint8_t *tree = probabilities + (bands[position] * 3 + context) * 11;
        uint32_t bit;
        if (allow_end) {
            if (!ab_vp8_boolean_bit(reader, tree[0], &bit, error)) {
                return false;
            }
            if (!bit) {
                break;
            }
        }
        if (!ab_vp8_boolean_bit(reader, tree[1], &bit, error)) {
            return false;
        }
        if (!bit) {
            context = 0;
            allow_end = false;
            ++position;
            continue;
        }
        uint32_t magnitude, negative;
        if (!read_magnitude(reader, tree, &magnitude, error) ||
            !ab_vp8_boolean_bit(reader, 128, &negative, error)) {
            return false;
        }
        int32_t value = (int32_t)magnitude * quantizers[position != 0];
        output[raster[position]] = ab_vp8_narrow(negative ? -value : value);
        context = magnitude == 1 ? 1 : 2;
        allow_end = true;
        ++position;
    }
    *end = position;
    return true;
}
