#include "vp8_decode.h"
#include "vp8_filter.h"
#include "vp8_predict.h"
#include "vp8_residuals.h"
#include "vp8_transform.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t *storage, *pixels;
    size_t stride, size;
} Plane;

typedef struct {
    uint8_t limit, interior, variance;
    bool internal;
} Filter;

typedef struct {
    AbVp8Header header;
    Plane planes[3];
    uint8_t *above_modes;
    AbVp8Nonzero *above_nonzero;
    AbVp8Macroblock *row_modes;
    Filter *filters;
    AbBuffer output;
} Frame;

static void release_frame(Frame *frame) {
    for (size_t plane = 0; plane < 3; ++plane) {
        free(frame->planes[plane].storage);
    }
    free(frame->above_modes);
    free(frame->above_nonzero);
    free(frame->row_modes);
    free(frame->filters);
    ab_buffer_free(&frame->output);
}

static bool add_budget(size_t *used, size_t bytes, size_t limit, AbError *error) {
    if (*used > limit || bytes > limit - *used) {
        return ab_fail(error, "VP8 reconstruction exceeds its live allocation limit");
    }
    *used += bytes;
    return true;
}

static bool allocate_frame(Frame *frame, size_t limit, AbError *error) {
    const AbVp8Header *header = &frame->header;
    size_t columns = header->macroblock_width, rows = header->macroblock_height;
    size_t used = 0;
    frame->output.size =
        (size_t)header->width * header->height +
        2 * ((header->width + 1) / 2) * (size_t)((header->height + 1) / 2);
    if (!add_budget(&used, frame->output.size, limit, error) ||
        !add_budget(&used, columns * 4, limit, error) ||
        !add_budget(&used, columns * sizeof(*frame->above_nonzero), limit, error) ||
        !add_budget(&used, columns * sizeof(*frame->row_modes), limit, error) ||
        !add_budget(&used, columns * rows * sizeof(*frame->filters), limit, error)) {
        return false;
    }
    for (size_t index = 0; index < 3; ++index) {
        Plane *plane = frame->planes + index;
        size_t block_size = index ? 8 : 16;
        plane->stride = columns * block_size + 16;
        plane->size = (rows * block_size + 2) * plane->stride;
        if (!add_budget(&used, plane->size, limit, error)) {
            return false;
        }
    }
    frame->above_modes = calloc(columns, 4);
    frame->above_nonzero = calloc(columns, sizeof(*frame->above_nonzero));
    frame->row_modes = calloc(columns, sizeof(*frame->row_modes));
    frame->filters = calloc(columns * rows, sizeof(*frame->filters));
    frame->output.data = malloc(frame->output.size);
    if (!frame->above_modes || !frame->above_nonzero || !frame->row_modes ||
        !frame->filters || !frame->output.data) {
        return ab_fail(error, "Cannot allocate VP8 reconstruction state/output");
    }
    for (size_t index = 0; index < 3; ++index) {
        Plane *plane = frame->planes + index;
        size_t height = rows * (index ? 8 : 16);
        plane->storage = calloc(1, plane->size);
        if (!plane->storage) {
            return ab_fail(error, "Cannot allocate VP8 reconstruction plane");
        }
        plane->pixels = plane->storage + plane->stride + 8;
        memset(plane->storage, 127, plane->stride);
        for (size_t row = 0; row < height; ++row) {
            (plane->pixels + row * plane->stride)[-1] = 129;
        }
    }
    return true;
}

static bool reconstruct_luma(const AbVp8Macroblock *block, const int16_t *coefficients,
                             Plane *plane, size_t column, size_t row, size_t columns,
                             AbError *error) {
    uint8_t *pixels = plane->pixels + row * 16 * plane->stride + column * 16;
    if (!block->luma4 &&
        !ab_vp8_predict_large((AbVp8Prediction)block->luma[0], 16, row != 0,
                              column != 0, pixels, plane->stride, error)) {
        return false;
    }
    uint8_t top_right[4];
    if (block->luma4) {
        const uint8_t *top = pixels - plane->stride;
        for (size_t index = 0; index < 4; ++index) {
            top_right[index] = top[column + 1 < columns ? 16 + index : 15];
        }
    }
    for (size_t y = 0; y < 4; ++y) {
        for (size_t x = 0; x < 4; ++x) {
            uint8_t *destination = pixels + y * 4 * plane->stride + x * 4;
            if (block->luma4) {
                if (x == 3) {
                    memcpy(destination - plane->stride + 4, top_right, 4);
                }
                if (!ab_vp8_predict_small((AbVp8Prediction)block->luma[y * 4 + x],
                                          destination, plane->stride, error)) {
                    return false;
                }
            }
            ab_vp8_dct_add(coefficients + (y * 4 + x) * 16, destination, plane->stride);
        }
    }
    return true;
}

static bool reconstruct_chroma(const AbVp8Macroblock *block,
                               const int16_t *coefficients, Plane *plane, size_t column,
                               size_t row, AbError *error) {
    uint8_t *pixels = plane->pixels + row * 8 * plane->stride + column * 8;
    if (!ab_vp8_predict_large((AbVp8Prediction)block->chroma, 8, row != 0, column != 0,
                              pixels, plane->stride, error)) {
        return false;
    }
    for (size_t y = 0; y < 2; ++y) {
        for (size_t x = 0; x < 2; ++x) {
            ab_vp8_dct_add(coefficients + (y * 2 + x) * 16,
                           pixels + y * 4 * plane->stride + x * 4, plane->stride);
        }
    }
    return true;
}

static Filter filter_parameters(const AbVp8Header *header, const AbVp8Macroblock *block,
                                const int16_t coefficients[384]) {
    int level = header->filter.level;
    if (header->segments.enabled) {
        level = header->segments.filters[block->segment] +
                (header->segments.absolute ? 0 : level);
    }
    if (header->filter.use_deltas) {
        level += header->filter.reference_deltas[0];
        if (block->luma4) {
            level += header->filter.mode_deltas[0];
        }
    }
    level = level < 0 ? 0 : level > 63 ? 63 : level;
    if (!level) {
        return (Filter){0};
    }
    int interior = level;
    int sharpness = header->filter.sharpness;
    if (sharpness) {
        interior >>= sharpness > 4 ? 2 : 1;
        if (interior > 9 - sharpness) {
            interior = 9 - sharpness;
        }
    }
    if (!interior) {
        interior = 1;
    }
    bool internal = block->luma4;
    for (size_t index = 0; index < 384; ++index) {
        internal |= coefficients[index] != 0;
    }
    return (Filter){.limit = (uint8_t)(2 * level + interior),
                    .interior = (uint8_t)interior,
                    .variance = (uint8_t)(level >= 40   ? 2
                                          : level >= 15 ? 1
                                                        : 0),
                    .internal = internal};
}

static bool reconstruct_frame(Frame *frame, AbError *error) {
    AbVp8Header *header = &frame->header;
    size_t columns = header->macroblock_width;
    AbVp8Boolean tokens;
    if (!ab_vp8_boolean_init(header->partitions[0], &tokens, error)) {
        return false;
    }
    for (size_t row = 0; row < header->macroblock_height; ++row) {
        if (!ab_vp8_modes_read_row(header, frame->above_modes, columns * 4,
                                   frame->row_modes, columns, error)) {
            return false;
        }
        AbVp8Nonzero left = {0};
        for (size_t column = 0; column < columns; ++column) {
            const AbVp8Macroblock *block = frame->row_modes + column;
            int16_t coefficients[384];
            if (!ab_vp8_residuals_read(header, &tokens, block,
                                       frame->above_nonzero + column, &left,
                                       coefficients, error) ||
                !reconstruct_luma(block, coefficients, frame->planes, column, row,
                                  columns, error) ||
                !reconstruct_chroma(block, coefficients + 256, frame->planes + 1,
                                    column, row, error) ||
                !reconstruct_chroma(block, coefficients + 320, frame->planes + 2,
                                    column, row, error)) {
                return false;
            }
            frame->filters[row * columns + column] =
                filter_parameters(header, block, coefficients);
        }
    }
    return true;
}

static void filter_macroblock(Plane *plane, size_t column, size_t row,
                              size_t block_size, Filter filter, bool simple) {
    uint8_t *pixels =
        plane->pixels + row * block_size * plane->stride + column * block_size;
    for (size_t direction = 0; direction < 2; ++direction) {
        size_t normal = direction ? plane->stride : 1;
        size_t along = direction ? 1 : plane->stride;
        if (direction ? row != 0 : column != 0) {
            ab_vp8_filter_edge(pixels, normal, along, block_size,
                               (uint8_t)(filter.limit + 4), filter.interior,
                               filter.variance, true, simple);
        }
        if (filter.internal) {
            for (size_t offset = 4; offset < block_size; offset += 4) {
                ab_vp8_filter_edge(pixels + offset * normal, normal, along, block_size,
                                   filter.limit, filter.interior, filter.variance,
                                   false, simple);
            }
        }
    }
}

static void filter_frame(Frame *frame) {
    size_t columns = frame->header.macroblock_width;
    bool simple = frame->header.filter.simple;
    for (size_t row = 0; row < frame->header.macroblock_height; ++row) {
        for (size_t column = 0; column < columns; ++column) {
            Filter filter = frame->filters[row * columns + column];
            if (!filter.limit) {
                continue;
            }
            filter_macroblock(frame->planes, column, row, 16, filter, simple);
            if (!simple) {
                filter_macroblock(frame->planes + 1, column, row, 8, filter, false);
                filter_macroblock(frame->planes + 2, column, row, 8, filter, false);
            }
        }
    }
}

static void copy_output(Frame *frame) {
    size_t offset = 0;
    for (size_t index = 0; index < 3; ++index) {
        const Plane *plane = frame->planes + index;
        size_t width = index ? (frame->header.width + 1) / 2 : frame->header.width;
        size_t height = index ? (frame->header.height + 1) / 2 : frame->header.height;
        for (size_t row = 0; row < height; ++row) {
            memcpy(frame->output.data + offset, plane->pixels + row * plane->stride,
                   width);
            offset += width;
        }
    }
}

bool ab_vp8_decode(AbSpan input, size_t allocation_limit, bool bypass_filter,
                   AbVp8Image *output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing VP8 decoded-plane output");
    }
    *output = (AbVp8Image){0};
    Frame frame = {0};
    bool okay = false;
    if (!ab_vp8_header_read(input, &frame.header, error)) {
        goto release;
    }
    if (frame.header.profile != 0 || frame.header.partition_count != 1) {
        ab_fail(error, "Unsupported VP8 pixel profile/token partition layout");
        goto release;
    }
    if (!allocate_frame(&frame, allocation_limit, error) ||
        !reconstruct_frame(&frame, error)) {
        goto release;
    }
    if (!bypass_filter) {
        filter_frame(&frame);
    }
    copy_output(&frame);
    *output = (AbVp8Image){frame.header.width, frame.header.height, frame.output};
    frame.output = (AbBuffer){0};
    okay = true;
release:
    release_frame(&frame);
    return okay;
}
