#include "ab/webp.h"

bool ab_webp_alpha_unfilter(uint8_t *pixels, size_t size, uint32_t width,
                            uint32_t height, size_t stride, unsigned filter,
                            uint32_t first_row, uint32_t rows, AbError *error) {
    if (!pixels || !width || !height || stride < width || filter > 3 ||
        first_row > height || rows > height - first_row ||
        (size_t)(height - 1) > SIZE_MAX / stride ||
        !ab_range(size, (size_t)(height - 1) * stride, width)) {
        return ab_fail(error, "Invalid WebP alpha reconstruction bounds/filter");
    }
    if (!filter) {
        return true;
    }
    for (uint32_t y = first_row; y < first_row + rows; ++y) {
        uint8_t *row = pixels + (size_t)y * stride;
        const uint8_t *above = y ? row - stride : NULL;
        for (uint32_t x = 0; x < width; ++x) {
            int prediction;
            if (!y) {
                prediction = x ? row[x - 1] : 0;
            } else if (!x) {
                prediction = above[0];
            } else if (filter == 1) {
                prediction = row[x - 1];
            } else if (filter == 2) {
                prediction = above[x];
            } else {
                prediction = row[x - 1] + above[x] - above[x - 1];
                if (prediction < 0) {
                    prediction = 0;
                } else if (prediction > 255) {
                    prediction = 255;
                }
            }
            row[x] = (uint8_t)((unsigned)row[x] + (unsigned)prediction);
        }
    }
    return true;
}
