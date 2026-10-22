#include "ab/webp.h"

#include <string.h>

static uint32_t u24(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16);
}

static bool color_header(AbSpan bytes, bool lossless, uint32_t *width, uint32_t *height,
                         bool *alpha, AbError *error) {
    if (lossless) {
        if (bytes.size < 5 || bytes.data[0] != 0x2f) {
            return ab_fail(error, "Invalid lossless WebP image header");
        }
        uint32_t fields = ab_u32le(bytes.data + 1);
        if (fields >> 29) {
            return ab_fail(error, "Unsupported lossless WebP version");
        }
        *width = 1 + (fields & 16383);
        *height = 1 + ((fields >> 14) & 16383);
        *alpha = (fields & (1u << 28)) != 0;
        return true;
    }
    if (bytes.size < 10 || memcmp(bytes.data + 3, "\x9d\x01\x2a", 3)) {
        return ab_fail(error, "Invalid lossy WebP key-frame header");
    }
    uint32_t tag = u24(bytes.data);
    uint16_t x = ab_u16le(bytes.data + 6), y = ab_u16le(bytes.data + 8);
    if ((tag & 1) || ((tag >> 1) & 7) > 3 || !(tag & 16) ||
        (tag >> 5) > bytes.size - 10 || (x >> 14) || (y >> 14)) {
        return ab_fail(error, "Unsupported/malformed WebP key frame");
    }
    *width = x & 16383;
    *height = y & 16383;
    *alpha = false;
    return (*width && *height) || ab_fail(error, "Empty WebP key-frame dimensions");
}

bool ab_webp_still_read(AbSpan input, AbWebpStill *output, AbError *error) {
    if (!output || !input.data || input.size < 12 || memcmp(input.data, "RIFF", 4) ||
        memcmp(input.data + 8, "WEBP", 4) ||
        input.size - 8 != ab_u32le(input.data + 4)) {
        return ab_fail(error, "Invalid WebP RIFF container");
    }
    AbWebpStill result = {0};
    bool extended = false, alpha = false, color = false, encoded_alpha = false;
    uint32_t canvas_width = 0, canvas_height = 0;
    uint8_t flags = 0;
    for (size_t offset = 12; offset < input.size;) {
        if (!ab_range(input.size, offset, 8)) {
            return ab_fail(error, "Truncated WebP chunk header");
        }
        const uint8_t *tag = input.data + offset;
        size_t size = ab_u32le(tag + 4);
        offset += 8;
        if (!ab_range(input.size, offset, size) ||
            ((size & 1) &&
             (!ab_range(input.size, offset + size, 1) || input.data[offset + size]))) {
            return ab_fail(error, "Invalid WebP chunk payload/padding");
        }
        AbSpan bytes = {input.data + offset, size};
        if (!memcmp(tag, "VP8X", 4)) {
            if (offset != 20 || extended || size != 10 || (bytes.data[0] & 0xc3) ||
                bytes.data[1] || bytes.data[2] || bytes.data[3]) {
                return ab_fail(error, "Unsupported extended/animated WebP header");
            }
            extended = true;
            flags = bytes.data[0];
            canvas_width = u24(bytes.data + 4) + 1;
            canvas_height = u24(bytes.data + 7) + 1;
        } else if (!memcmp(tag, "ALPH", 4)) {
            if (!extended || alpha || color || !size || (bytes.data[0] & 0xc0) ||
                (bytes.data[0] & 3) > 1 || ((bytes.data[0] >> 4) & 3) > 1) {
                return ab_fail(error, "Invalid WebP alpha chunk/order");
            }
            alpha = true;
            result.compressed_alpha = (bytes.data[0] & 3) != 0;
            result.alpha_filter = (bytes.data[0] >> 2) & 3;
            result.alpha_preprocessing = (bytes.data[0] >> 4) & 3;
            result.alpha = (AbSpan){bytes.data + 1, size - 1};
        } else if (!memcmp(tag, "VP8 ", 4) || !memcmp(tag, "VP8L", 4)) {
            result.lossless = tag[3] == 'L';
            if (color || (result.lossless && alpha) ||
                !color_header(bytes, result.lossless, &result.width, &result.height,
                              &encoded_alpha, error)) {
                return ab_fail(error, "Invalid/duplicate WebP color chunk");
            }
            color = true;
            result.color = bytes;
        } else if (!memcmp(tag, "ANIM", 4) || !memcmp(tag, "ANMF", 4)) {
            return ab_fail(error, "Animated WebP requires separate recovery");
        }
        offset += size + (size & 1);
    }
    result.has_alpha = alpha || encoded_alpha;
    if (!color ||
        (extended && (result.width != canvas_width || result.height != canvas_height ||
                      result.has_alpha != ((flags & 16) != 0))) ||
        (alpha && !result.compressed_alpha &&
         (result.width > SIZE_MAX / result.height ||
          result.alpha.size != (size_t)result.width * result.height))) {
        return ab_fail(error, "Inconsistent WebP dimensions/alpha payload");
    }
    *output = result;
    return true;
}
