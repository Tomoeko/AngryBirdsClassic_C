#include "ab/image.h"
#include "ab/webp.h"
#include "ab/webp_color.h"
#include "vp8_decode.h"

#include <stdlib.h>

bool ab_webp_decode(AbSpan input, size_t allocation_limit, AbImage *output,
                    AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing WebP decoded-image output");
    }
    *output = (AbImage){0};
    AbWebpStill image;
    AbVp8Image planes = {0};
    AbBuffer alpha = {0}, rgba = {0};
    bool okay = false;
    if (!ab_webp_still_read(input, &image, error)) {
        goto release;
    }
    if (image.lossless || image.alpha_preprocessing) {
        ab_fail(error, "Unsupported WebP color/alpha preprocessing mode");
        goto release;
    }
    if (!ab_vp8_decode(image.color, allocation_limit, false, &planes, error)) {
        goto release;
    }
    if (planes.width != image.width || planes.height != image.height) {
        ab_fail(error, "Inconsistent WebP decoded-plane dimensions");
        goto release;
    }
    size_t used = planes.yuv.size;
    if (image.has_alpha &&
        !ab_webp_alpha_decode(&image, allocation_limit - used, &alpha, error)) {
        goto release;
    }
    used += alpha.size;
    size_t count = (size_t)image.width * image.height;
    if (used > allocation_limit || count > (allocation_limit - used) / 4) {
        ab_fail(error, "WebP RGBA exceeds its live allocation limit");
        goto release;
    }
    rgba.size = count * 4;
    rgba.data = malloc(rgba.size);
    if (!rgba.data) {
        ab_fail(error, "Cannot allocate WebP RGBA output");
        goto release;
    }
    size_t chroma_size = (size_t)((image.width + 1) / 2) * ((image.height + 1) / 2);
    AbWebpYuv420 color = {.width = image.width,
                          .height = image.height,
                          .y = {planes.yuv.data, count},
                          .u = {planes.yuv.data + count, chroma_size},
                          .v = {planes.yuv.data + count + chroma_size, chroma_size}};
    if (!ab_webp_yuv420_rgba(&color, (AbSpan){alpha.data, alpha.size}, rgba, error)) {
        goto release;
    }
    *output = (AbImage){image.width, image.height, rgba};
    rgba = (AbBuffer){0};
    okay = true;
release:
    ab_buffer_free(&planes.yuv);
    ab_buffer_free(&alpha);
    ab_buffer_free(&rgba);
    return okay;
}
