#include "vp8_residuals.h"
#include "vp8_coefficients.h"
#include "vp8_transform.h"

#include <string.h>

static bool valid_context(const AbVp8Nonzero *context) {
    for (size_t index = 0; index < 4; ++index) {
        if (context->y[index] > 1) {
            return false;
        }
    }
    for (size_t index = 0; index < 2; ++index) {
        if (context->u[index] > 1 || context->v[index] > 1) {
            return false;
        }
    }
    return context->y2 <= 1;
}

static bool read_plane(AbVp8Boolean *reader, const uint8_t *probabilities,
                       unsigned first, const int32_t quantizers[2], size_t width,
                       uint8_t *above, uint8_t *left, int16_t *output, AbError *error) {
    for (size_t row = 0; row < width; ++row) {
        for (size_t column = 0; column < width; ++column) {
            unsigned end;
            if (!ab_vp8_coefficients_read(
                    reader, probabilities, above[column] + left[row], quantizers, first,
                    output + (row * width + column) * 16, &end, error)) {
                return false;
            }
            above[column] = left[row] = end > first;
        }
    }
    return true;
}

static void clear_context(AbVp8Nonzero *context, bool y2) {
    uint8_t retained_y2 = y2 ? 0 : context->y2;
    *context = (AbVp8Nonzero){.y2 = retained_y2};
}

bool ab_vp8_residuals_read(const AbVp8Header *header, AbVp8Boolean *reader,
                           const AbVp8Macroblock *block, AbVp8Nonzero *above,
                           AbVp8Nonzero *left, int16_t output[384], AbError *error) {
    if (!header || !reader || !block || block->segment > 3 || !above || !left ||
        !output || !valid_context(above) || !valid_context(left)) {
        return ab_fail(error, "Invalid VP8 macroblock residual input/context");
    }
    memset(output, 0, 384 * sizeof(*output));
    if (block->skip) {
        clear_context(above, !block->luma4);
        clear_context(left, !block->luma4);
        return true;
    }
    const int32_t *quantizers = header->quantizers[block->segment];
    if (!block->luma4) {
        int16_t y2[16] = {0}, dc[16];
        unsigned end;
        if (!ab_vp8_coefficients_read(reader, header->coefficient_probabilities + 264,
                                      above->y2 + left->y2, quantizers + AB_VP8_Y2_DC,
                                      0, y2, &end, error)) {
            return false;
        }
        above->y2 = left->y2 = end > 0;
        ab_vp8_walsh_inverse(y2, dc);
        for (size_t index = 0; index < 16; ++index) {
            output[index * 16] = dc[index];
        }
    }
    const uint8_t *probabilities = header->coefficient_probabilities;
    unsigned first = block->luma4 ? 0 : 1;
    if (!read_plane(reader, probabilities + (block->luma4 ? 3 * 264 : 0), first,
                    quantizers + AB_VP8_Y1_DC, 4, above->y, left->y, output, error) ||
        !read_plane(reader, probabilities + 2 * 264, 0, quantizers + AB_VP8_UV_DC, 2,
                    above->u, left->u, output + 256, error) ||
        !read_plane(reader, probabilities + 2 * 264, 0, quantizers + AB_VP8_UV_DC, 2,
                    above->v, left->v, output + 320, error)) {
        return false;
    }
    return true;
}
