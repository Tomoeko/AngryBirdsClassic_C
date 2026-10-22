#include "vp8_predict.h"

static uint8_t clip(int value) {
    return (uint8_t)(value < 0 ? 0 : value > 255 ? 255 : value);
}

static uint8_t average_pair(int first, int second) {
    return (uint8_t)((first + second + 1) / 2);
}

static uint8_t average_three(int first, int middle, int last) {
    return (uint8_t)((first + 2 * middle + last + 2) / 4);
}

bool ab_vp8_predict_large(AbVp8Prediction mode, size_t size, bool above_available,
                          bool left_available, uint8_t *pixels, size_t stride,
                          AbError *error) {
    if ((unsigned)mode > AB_VP8_PRED_HORIZONTAL || (size != 8 && size != 16) ||
        !pixels || stride < size) {
        return ab_fail(error, "Invalid VP8 large predictor mode/size/stride");
    }
    const uint8_t *top = pixels - stride;
    int corner = top[-1];
    unsigned sum = 0, count = 0;
    if (mode == AB_VP8_PRED_DC) {
        for (size_t index = 0; index < size; ++index) {
            if (above_available) {
                sum += top[index];
                ++count;
            }
            if (left_available) {
                sum += (pixels + index * stride)[-1];
                ++count;
            }
        }
    }
    uint8_t dc = count ? (uint8_t)((sum + count / 2) / count) : 128;
    for (size_t row = 0; row < size; ++row) {
        int left = (pixels + row * stride)[-1];
        for (size_t column = 0; column < size; ++column) {
            uint8_t value = dc;
            if (mode == AB_VP8_PRED_TM) {
                value = clip(top[column] + left - corner);
            } else if (mode == AB_VP8_PRED_VERTICAL) {
                value = top[column];
            } else if (mode == AB_VP8_PRED_HORIZONTAL) {
                value = (uint8_t)left;
            }
            pixels[row * stride + column] = value;
        }
    }
    return true;
}

static int edge_sample(const uint8_t top[8], const uint8_t left[4], int corner,
                       int position) {
    if (position == 0) {
        return corner;
    }
    return position > 0 ? top[position - 1] : left[-position - 1];
}

static uint8_t predict_vertical_right(const uint8_t top[8], const uint8_t left[4],
                                      int corner, int column, int row) {
    int position = column * 2 - row;
    if (position >= 0 && position % 2 == 0) {
        int first = edge_sample(top, left, corner, position / 2);
        int second = edge_sample(top, left, corner, position / 2 + 1);
        return average_pair(first, second);
    }
    int first = position < 0 ? position : position / 2;
    return average_three(edge_sample(top, left, corner, first),
                         edge_sample(top, left, corner, first + 1),
                         edge_sample(top, left, corner, first + 2));
}

static uint8_t predict_vertical_left(const uint8_t *top, size_t count, int column,
                                     int row) {
    size_t first = (size_t)(column + row / 2);
    size_t second = first + 1, third = first + 2;
    if (first >= count) {
        first = count - 1;
    }
    if (second >= count) {
        second = count - 1;
    }
    if (third >= count) {
        third = count - 1;
    }
    return row % 2 ? average_three(top[first], top[second], top[third])
                   : average_pair(top[first], top[second]);
}

static uint8_t small_pixel(AbVp8Prediction mode, const uint8_t top[8],
                           const uint8_t left[4], int corner, int column, int row,
                           uint8_t dc) {
    switch (mode) {
        case AB_VP8_PRED_DC:
            return dc;
        case AB_VP8_PRED_TM:
            return clip(top[column] + left[row] - corner);
        case AB_VP8_PRED_VERTICAL:
            return average_three(column ? top[column - 1] : corner, top[column],
                                 top[column + 1]);
        case AB_VP8_PRED_HORIZONTAL:
            return average_three(row ? left[row - 1] : corner, left[row],
                                 left[row < 3 ? row + 1 : 3]);
        case AB_VP8_PRED_DOWN_LEFT: {
            int first = row + column;
            return average_three(top[first], top[first + 1],
                                 top[first < 6 ? first + 2 : 7]);
        }
        case AB_VP8_PRED_DOWN_RIGHT: {
            int position = column - row;
            return average_three(edge_sample(top, left, corner, position - 1),
                                 edge_sample(top, left, corner, position),
                                 edge_sample(top, left, corner, position + 1));
        }
        case AB_VP8_PRED_VERTICAL_RIGHT:
            return predict_vertical_right(top, left, corner, column, row);
        case AB_VP8_PRED_VERTICAL_LEFT:
            /* The two lower-right VL samples extend the filtered top-right
             * sequence instead of repeating the earlier half-pixel phase. */
            if (column == 3 && row >= 2) {
                return average_three(top[row + 2], top[row + 3], top[row + 4]);
            }
            return predict_vertical_left(top, 8, column, row);
        case AB_VP8_PRED_HORIZONTAL_DOWN: {
            uint8_t transposed_top[8] = {left[0], left[1], left[2], left[3]};
            return predict_vertical_right(transposed_top, top, corner, row, column);
        }
        case AB_VP8_PRED_HORIZONTAL_UP:
            return predict_vertical_left(left, 4, row, column);
    }
    return 0;
}

bool ab_vp8_predict_small(AbVp8Prediction mode, uint8_t *pixels, size_t stride,
                          AbError *error) {
    if ((unsigned)mode > AB_VP8_PRED_HORIZONTAL_UP || !pixels || stride < 8) {
        return ab_fail(error, "Invalid VP8 small predictor mode/stride");
    }
    uint8_t top[8], left[4];
    int corner = pixels[-(ptrdiff_t)stride - 1];
    unsigned sum = 4;
    for (size_t index = 0; index < 8; ++index) {
        top[index] = pixels[(ptrdiff_t)index - (ptrdiff_t)stride];
        if (index < 4) {
            left[index] = (pixels + index * stride)[-1];
            sum += top[index] + left[index];
        }
    }
    uint8_t dc = (uint8_t)(sum / 8);
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            pixels[(size_t)row * stride + (size_t)column] =
                small_pixel(mode, top, left, corner, column, row, dc);
        }
    }
    return true;
}
