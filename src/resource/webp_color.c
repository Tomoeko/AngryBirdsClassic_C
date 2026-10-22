#include "ab/webp_color.h"

static bool readable(AbSpan span, size_t count) {
    return span.data && span.size >= count;
}

static bool writable(AbBuffer buffer, size_t count) {
    return buffer.data && buffer.size >= count;
}

static uint8_t component(int32_t value) {
    if (value <= 0) {
        return 0;
    }
    if (value >= 255 * 16384) {
        return 255;
    }
    return (uint8_t)((uint32_t)value >> 14);
}

static void pixel_rgba(uint8_t y, unsigned u, unsigned v, uint8_t output[4]) {
    int32_t luma = 19077 * (int32_t)y;
    output[0] = component(luma + 26149 * (int32_t)v - 3644112);
    output[1] = component(luma - 6419 * (int32_t)u - 13320 * (int32_t)v + 2229552);
    output[2] = component(luma + 33050 * (int32_t)u - 4527440);
    output[3] = 255;
}

static unsigned chroma(const uint8_t *primary, const uint8_t *secondary, uint32_t x,
                       uint32_t width) {
    uint32_t index = x / 2;
    uint32_t count = (width + 1) / 2;
    if (!x || ((x & 1u) && index + 1 == count)) {
        return (3u * primary[index] + secondary[index] + 2u) / 4;
    }
    uint32_t neighbor = (x & 1u) ? index + 1 : index - 1;
    return (9u * primary[index] + 3u * primary[neighbor] + 3u * secondary[index] +
            secondary[neighbor] + 8u) /
           16;
}

static void convert_pair(const AbWebpYuvPair *pair, uint8_t *top, uint8_t *bottom) {
    for (uint32_t x = 0; x < pair->width; ++x) {
        unsigned u = chroma(pair->top_u.data, pair->bottom_u.data, x, pair->width);
        unsigned v = chroma(pair->top_v.data, pair->bottom_v.data, x, pair->width);
        pixel_rgba(pair->top_y.data[x], u, v, top + (size_t)x * 4);
        if (pair->bottom_y.size) {
            u = chroma(pair->bottom_u.data, pair->top_u.data, x, pair->width);
            v = chroma(pair->bottom_v.data, pair->top_v.data, x, pair->width);
            pixel_rgba(pair->bottom_y.data[x], u, v, bottom + (size_t)x * 4);
        }
    }
}

bool ab_webp_upsample_rgba(const AbWebpYuvPair *pair, AbBuffer top_rgba,
                           AbBuffer bottom_rgba, AbError *error) {
    if (!pair || !pair->width || pair->width > 16384) {
        return ab_fail(error, "Invalid WebP upsample line-pair width");
    }
    size_t chroma_count = (pair->width + 1u) / 2;
    size_t bytes = (size_t)pair->width * 4;
    if (!readable(pair->top_y, pair->width) || !readable(pair->top_u, chroma_count) ||
        !readable(pair->top_v, chroma_count) ||
        !readable(pair->bottom_u, chroma_count) ||
        !readable(pair->bottom_v, chroma_count) || !writable(top_rgba, bytes) ||
        (pair->bottom_y.size &&
         (!readable(pair->bottom_y, pair->width) || !writable(bottom_rgba, bytes)))) {
        return ab_fail(error, "Invalid WebP upsample plane/output bounds");
    }
    convert_pair(pair, top_rgba.data, bottom_rgba.data);
    return true;
}

static AbWebpYuvPair image_pair(const AbWebpYuv420 *image, uint32_t top_row,
                                uint32_t bottom_row, uint32_t upper_chroma,
                                uint32_t lower_chroma) {
    size_t width = image->width, chroma_width = (image->width + 1u) / 2;
    return (AbWebpYuvPair){
        .top_y = {image->y.data + (size_t)top_row * width, width},
        .bottom_y = bottom_row < image->height
                        ? (AbSpan){image->y.data + (size_t)bottom_row * width, width}
                        : (AbSpan){0},
        .top_u = {image->u.data + (size_t)upper_chroma * chroma_width, chroma_width},
        .top_v = {image->v.data + (size_t)upper_chroma * chroma_width, chroma_width},
        .bottom_u = {image->u.data + (size_t)lower_chroma * chroma_width, chroma_width},
        .bottom_v = {image->v.data + (size_t)lower_chroma * chroma_width, chroma_width},
        .width = image->width};
}

bool ab_webp_yuv420_rgba(const AbWebpYuv420 *image, AbSpan alpha, AbBuffer rgba,
                         AbError *error) {
    if (!image || !image->width || !image->height || image->width > 16384 ||
        image->height > 16384) {
        return ab_fail(error, "Invalid WebP YUV image dimensions");
    }
    size_t width = image->width, height = image->height;
    size_t count = width * height;
    size_t chroma_width = (width + 1) / 2, chroma_height = (height + 1) / 2;
    size_t chroma_count = chroma_width * chroma_height;
    if (count > SIZE_MAX / 4 || !readable(image->y, count) ||
        !readable(image->u, chroma_count) || !readable(image->v, chroma_count) ||
        !writable(rgba, count * 4) || (alpha.size && !readable(alpha, count))) {
        return ab_fail(error, "Invalid WebP YUV/alpha/output plane bounds");
    }
    AbWebpYuvPair first = image_pair(image, 0, image->height, 0, 0);
    convert_pair(&first, rgba.data, NULL);
    for (uint32_t row = 1; row < image->height; row += 2) {
        uint32_t upper = (row - 1) / 2;
        uint32_t lower = (row + 1) / 2;
        if (lower >= chroma_height) {
            lower = (uint32_t)chroma_height - 1;
        }
        AbWebpYuvPair pair = image_pair(image, row, row + 1, upper, lower);
        uint8_t *top = rgba.data + (size_t)row * width * 4;
        uint8_t *bottom = row + 1 < image->height ? top + width * 4 : NULL;
        convert_pair(&pair, top, bottom);
    }
    if (alpha.size) {
        for (size_t index = 0; index < count; ++index) {
            rgba.data[index * 4 + 3] = alpha.data[index];
        }
    }
    return true;
}
