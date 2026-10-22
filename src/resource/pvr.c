#include "ab/image.h"

#include <stdlib.h>
#include <string.h>

typedef enum {
    PVR_RGBA4444,
    PVR_RGB565,
    PVR_RGBA8888,
    PVR_ETC1,
} PvrFormat;

typedef struct {
    uint32_t width;
    uint32_t height;
    PvrFormat format;
    AbSpan pixels;
} PvrHeader;

static bool version3(AbSpan input, PvrHeader *header, AbError *error) {
    uint32_t flags = ab_u32le(input.data + 4);
    uint64_t pixel_format =
        (uint64_t)ab_u32le(input.data + 8) | (uint64_t)ab_u32le(input.data + 12) << 32;
    uint32_t channel_type = ab_u32le(input.data + 20);
    if (flags || ab_u32le(input.data + 16) || ab_u32le(input.data + 32) != 1 ||
        ab_u32le(input.data + 36) != 1 || ab_u32le(input.data + 40) != 1 ||
        ab_u32le(input.data + 44) != 1) {
        return ab_fail(
            error,
            "Unsupported PVR flags, color space, depth, surfaces, faces, or mips");
    }
    if (pixel_format == UINT64_C(0x0404040461626772) && channel_type == 4) {
        header->format = PVR_RGBA4444;
    } else if (pixel_format == UINT64_C(0x0005060500626772) && channel_type == 4) {
        header->format = PVR_RGB565;
    } else if (pixel_format == UINT64_C(0x0808080861626772) && channel_type == 0) {
        header->format = PVR_RGBA8888;
    } else if (pixel_format == 6 && channel_type == 0) {
        header->format = PVR_ETC1;
    } else {
        return ab_fail(error, "Unsupported PVR pixel format or channel type");
    }
    header->height = ab_u32le(input.data + 24);
    header->width = ab_u32le(input.data + 28);
    size_t metadata_size = ab_u32le(input.data + 48);
    if (!ab_range(input.size, 52, metadata_size)) {
        return ab_fail(error, "PVR metadata exceeds input bounds");
    }
    /* The pinned APK's v3 files contain no metadata; do not guess orientation. */
    if (metadata_size) {
        return ab_fail(error, "PVR metadata interpretation requires recovery");
    }
    header->pixels = (AbSpan){input.data + 52, input.size - 52};
    return true;
}

static bool version2(AbSpan input, PvrHeader *header, AbError *error) {
    if (ab_u32le(input.data) != 52 ||
        ab_u32le(input.data + 44) != UINT32_C(0x21525650) ||
        ab_u32le(input.data + 12) != 0 || ab_u32le(input.data + 48) != 1 ||
        (ab_u32le(input.data + 16) & ~UINT32_C(0x8000)) != 0x10 ||
        ab_u32le(input.data + 24) != 16 ||
        ab_u32le(input.data + 20) != input.size - 52) {
        return ab_fail(error, "Unsupported legacy PVR layout, format, or flags");
    }
    const uint32_t expected[4] = {0xf000, 0x0f00, 0x00f0, 0x000f};
    bool omitted = true;
    bool matching = true;
    for (size_t channel = 0; channel < 4; ++channel) {
        uint32_t mask = ab_u32le(input.data + 28 + channel * 4);
        omitted = omitted && mask == 0;
        matching = matching && mask == expected[channel];
    }
    if (!omitted && !matching) {
        return ab_fail(error, "Legacy PVR masks disagree with its RGBA4444 format");
    }
    header->height = ab_u32le(input.data + 4);
    header->width = ab_u32le(input.data + 8);
    header->format = PVR_RGBA4444;
    header->pixels = (AbSpan){input.data + 52, input.size - 52};
    return true;
}

static uint8_t clamp_channel(int value) {
    if (value < 0) {
        return 0;
    }
    return value > 255 ? 255 : (uint8_t)value;
}

static bool etc1_block(const uint8_t bytes[8], uint8_t pixels[64], AbError *error) {
    /* Intensity magnitudes and index mapping are ETC1 format constants. */
    static const int magnitude[8][2] = {
        {2, 8}, {5, 17}, {9, 29}, {13, 42}, {18, 60}, {24, 80}, {33, 106}, {47, 183},
    };
    int base[2][3];
    bool differential = (bytes[3] & 2) != 0;
    for (size_t channel = 0; channel < 3; ++channel) {
        unsigned encoded = bytes[channel];
        if (differential) {
            int first = (int)(encoded >> 3);
            int delta = (int)(encoded & 7);
            if (delta & 4) {
                delta -= 8;
            }
            int second = first + delta;
            if (second < 0 || second > 31) {
                return ab_fail(error,
                               "ETC1 differential base overflows five-bit range");
            }
            base[0][channel] = (first << 3) | (first >> 2);
            base[1][channel] = (second << 3) | (second >> 2);
        } else {
            base[0][channel] = (int)(encoded >> 4) * 17;
            base[1][channel] = (int)(encoded & 15) * 17;
        }
    }
    unsigned table[2] = {bytes[3] >> 5, (bytes[3] >> 2) & 7};
    bool horizontal_split = (bytes[3] & 1) != 0;
    uint32_t indices = ab_u32be(bytes + 4);
    for (unsigned y = 0; y < 4; ++y) {
        for (unsigned x = 0; x < 4; ++x) {
            unsigned subblock = horizontal_split ? y / 2 : x / 2;
            unsigned bit = x * 4 + y;
            unsigned magnitude_index = (indices >> bit) & 1;
            int modifier = magnitude[table[subblock]][magnitude_index];
            if ((indices >> (bit + 16)) & 1) {
                modifier = -modifier;
            }
            uint8_t *pixel = pixels + (y * 4 + x) * 4;
            for (size_t channel = 0; channel < 3; ++channel) {
                pixel[channel] = clamp_channel(base[subblock][channel] + modifier);
            }
            pixel[3] = 255;
        }
    }
    return true;
}

static bool decompress_etc1(const PvrHeader *header, uint8_t *pixels, AbError *error) {
    size_t blocks_x = ((size_t)header->width + 3) / 4;
    size_t blocks_y = ((size_t)header->height + 3) / 4;
    for (size_t block_y = 0; block_y < blocks_y; ++block_y) {
        for (size_t block_x = 0; block_x < blocks_x; ++block_x) {
            const uint8_t *bytes =
                header->pixels.data + (block_y * blocks_x + block_x) * 8;
            uint8_t block[64];
            if (!etc1_block(bytes, block, error)) {
                return false;
            }
            for (size_t y = 0; y < 4 && block_y * 4 + y < header->height; ++y) {
                for (size_t x = 0; x < 4 && block_x * 4 + x < header->width; ++x) {
                    size_t destination =
                        ((block_y * 4 + y) * header->width + block_x * 4 + x) * 4;
                    memcpy(pixels + destination, block + (y * 4 + x) * 4, 4);
                }
            }
        }
    }
    return true;
}

static void unpack_pixels(const PvrHeader *header, uint8_t *pixels) {
    size_t count = (size_t)header->width * header->height;
    if (header->format == PVR_RGBA8888) {
        memcpy(pixels, header->pixels.data, count * 4);
        return;
    }
    for (size_t index = 0; index < count; ++index) {
        uint16_t packed = ab_u16le(header->pixels.data + index * 2);
        uint8_t *pixel = pixels + index * 4;
        if (header->format == PVR_RGBA4444) {
            for (unsigned channel = 0; channel < 4; ++channel) {
                pixel[channel] = (uint8_t)(((packed >> ((3 - channel) * 4)) & 15) * 17);
            }
        } else {
            unsigned red = packed >> 11;
            unsigned green = (packed >> 5) & 63;
            unsigned blue = packed & 31;
            pixel[0] = (uint8_t)((red * 255 + 15) / 31);
            pixel[1] = (uint8_t)((green * 255 + 31) / 63);
            pixel[2] = (uint8_t)((blue * 255 + 15) / 31);
            pixel[3] = 255;
        }
    }
}

static bool read_header(AbSpan input, size_t decoded_limit, PvrHeader *header,
                        AbError *error) {
    if (!input.data || input.size < 52) {
        return ab_fail(error, "Truncated PVR header");
    }
    bool parsed = ab_u32le(input.data) == UINT32_C(0x03525650)
                      ? version3(input, header, error)
                      : version2(input, header, error);
    if (!parsed) {
        return false;
    }
    size_t width = header->width;
    size_t height = header->height;
    if (!width || !height || width > 16384 || height > 16384 || width > SIZE_MAX / 4 ||
        height > decoded_limit / (width * 4)) {
        return ab_fail(error, "PVR dimensions or decoded pixels exceed size limit");
    }
    size_t pixel_size = width * height * 4;
    size_t encoded_size = header->format == PVR_RGBA8888 ? pixel_size : pixel_size / 2;
    if (header->format == PVR_ETC1) {
        encoded_size = ((width + 3) / 4) * ((height + 3) / 4) * 8;
    }
    if (header->pixels.size != encoded_size) {
        return ab_fail(error, "PVR pixel payload size disagrees with dimensions");
    }
    return true;
}

bool ab_pvr_has_alpha(AbSpan input, bool *output, AbError *error) {
    PvrHeader header = {0};
    if (!output) {
        return ab_fail(error, "Missing PVR alpha metadata output");
    }
    if (!read_header(input, SIZE_MAX, &header, error)) {
        return false;
    }
    *output = header.format == PVR_RGBA4444 || header.format == PVR_RGBA8888;
    return true;
}

bool ab_pvr_decode(AbSpan input, size_t decoded_limit, AbImage *output,
                   AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing PVR image output");
    }
    *output = (AbImage){0};
    PvrHeader header = {0};
    if (!read_header(input, decoded_limit, &header, error)) {
        return false;
    }
    size_t pixel_size = (size_t)header.width * header.height * 4;
    uint8_t *pixels = malloc(pixel_size);
    if (!pixels) {
        return ab_fail(error, "Cannot allocate PVR image pixels");
    }
    if (header.format == PVR_ETC1) {
        if (!decompress_etc1(&header, pixels, error)) {
            free(pixels);
            return false;
        }
    } else {
        unpack_pixels(&header, pixels);
    }
    *output = (AbImage){header.width, header.height, {pixels, pixel_size}};
    return true;
}
