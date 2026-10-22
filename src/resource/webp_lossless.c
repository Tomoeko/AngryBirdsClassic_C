#include "ab/webp.h"
#include "webp_internal.h"

#include <stdlib.h>
#include <string.h>

enum { PREFIX_BITS = 15, PREFIX_SYMBOLS = 2328 };

typedef struct {
    size_t used, limit;
} Budget;

typedef struct {
    uint16_t counts[16], first[16], base[16];
    uint16_t *symbols;
    size_t count;
} Prefix;

typedef struct {
    Prefix codes[5];
} PrefixGroup;

static void *allocate(Budget *budget, size_t count, size_t stride, AbError *error) {
    if (!count || count > (budget->limit - budget->used) / stride) {
        ab_fail(error, "Lossless WebP workspace exceeds allocation limit");
        return NULL;
    }
    void *result = calloc(count, stride);
    if (!result) {
        ab_fail(error, "Cannot allocate lossless WebP workspace");
        return NULL;
    }
    budget->used += count * stride;
    return result;
}

static void discard(Budget *budget, void *pointer, size_t count, size_t stride) {
    if (pointer) {
        budget->used -= count * stride;
        free(pointer);
    }
}

static void close_prefix(Budget *budget, Prefix *prefix) {
    discard(budget, prefix->symbols, prefix->count, sizeof(*prefix->symbols));
    *prefix = (Prefix){0};
}

static bool build_prefix(const uint8_t *lengths, size_t alphabet, Budget *budget,
                         Prefix *prefix, AbError *error) {
    size_t used = 0;
    for (size_t symbol = 0; symbol < alphabet; ++symbol) {
        if (lengths[symbol] > PREFIX_BITS) {
            return ab_fail(error, "Invalid lossless WebP prefix length");
        }
        if (lengths[symbol]) {
            ++prefix->counts[lengths[symbol]];
            ++used;
        }
    }
    if (!used) {
        return ab_fail(error, "Empty lossless WebP prefix code");
    }
    int space = 1;
    unsigned code = 0, index = 0;
    for (unsigned length = 1; length <= PREFIX_BITS; ++length) {
        space = space * 2 - prefix->counts[length];
        if (space < 0) {
            return ab_fail(error, "Oversubscribed lossless WebP prefix code");
        }
        code = (code + prefix->counts[length - 1]) * 2;
        prefix->first[length] = (uint16_t)code;
        prefix->base[length] = (uint16_t)index;
        index += prefix->counts[length];
    }
    if (space && !(used == 1 && prefix->counts[1] == 1)) {
        return ab_fail(error, "Incomplete lossless WebP prefix code");
    }
    prefix->symbols = allocate(budget, used, sizeof(*prefix->symbols), error);
    if (!prefix->symbols) {
        return false;
    }
    prefix->count = used;
    uint16_t next[16];
    memcpy(next, prefix->base, sizeof(next));
    for (size_t symbol = 0; symbol < alphabet; ++symbol) {
        if (lengths[symbol]) {
            prefix->symbols[next[lengths[symbol]]++] = (uint16_t)symbol;
        }
    }
    return true;
}

static bool prefix_symbol(AbWebpBits *bits, const Prefix *prefix, uint32_t *output,
                          AbError *error) {
    if (prefix->count == 1) {
        *output = prefix->symbols[0];
        return true;
    }
    uint32_t code = 0;
    for (unsigned length = 1; length <= PREFIX_BITS; ++length) {
        uint32_t bit;
        if (!ab_webp_bits_read(bits, 1, &bit, error)) {
            return false;
        }
        code = (code << 1) | bit;
        uint32_t first = prefix->first[length];
        if (code >= first && code - first < prefix->counts[length]) {
            *output = prefix->symbols[prefix->base[length] + code - first];
            return true;
        }
    }
    ab_fail(error, "Invalid lossless WebP prefix symbol");
    return false;
}

static bool read_lengths(AbWebpBits *bits, size_t alphabet, Budget *budget,
                         uint8_t lengths[PREFIX_SYMBOLS], AbError *error) {
    static const uint8_t order[19] = {17, 18, 0, 1,  2,  3,  4,  5,  16, 6,
                                      7,  8,  9, 10, 11, 12, 13, 14, 15};
    uint32_t number, value;
    if (!ab_webp_bits_read(bits, 4, &number, error)) {
        return false;
    }
    number += 4;
    uint8_t coded[19] = {0};
    for (uint32_t index = 0; index < number; ++index) {
        if (!ab_webp_bits_read(bits, 3, &value, error)) {
            return false;
        }
        coded[order[index]] = (uint8_t)value;
    }
    Prefix prefix = {0};
    bool okay = false;
    if (!build_prefix(coded, 19, budget, &prefix, error) ||
        !ab_webp_bits_read(bits, 1, &value, error)) {
        goto release_prefix;
    }
    size_t limit = alphabet;
    if (value) {
        if (!ab_webp_bits_read(bits, 3, &value, error) ||
            !ab_webp_bits_read(bits, 2 + value * 2, &value, error)) {
            goto release_prefix;
        }
        limit = 2 + value;
        if (limit > alphabet) {
            ab_fail(error, "WebP code-length symbol limit exceeds alphabet");
            goto release_prefix;
        }
    }
    size_t position = 0;
    uint8_t previous = 8;
    /* The optional limit counts encoded length symbols, including repetition
     * symbols, rather than the number of expanded destination lengths. */
    while (position < alphabet && limit) {
        --limit;
        uint32_t symbol;
        if (!prefix_symbol(bits, &prefix, &symbol, error)) {
            goto release_prefix;
        }
        if (symbol < 16) {
            lengths[position++] = (uint8_t)symbol;
            if (symbol) {
                previous = (uint8_t)symbol;
            }
        } else {
            unsigned extra = symbol == 16 ? 2 : symbol == 17 ? 3 : 7;
            if (symbol > 18 || !ab_webp_bits_read(bits, extra, &value, error)) {
                goto release_prefix;
            }
            size_t repeat = value + (symbol == 18 ? 11u : 3u);
            if (repeat > alphabet - position) {
                ab_fail(error, "WebP code-length repetition exceeds alphabet");
                goto release_prefix;
            }
            memset(lengths + position, symbol == 16 ? previous : 0, repeat);
            position += repeat;
        }
    }
    okay = true;

release_prefix:
    close_prefix(budget, &prefix);
    return okay;
}

static bool read_prefix(AbWebpBits *bits, size_t alphabet, Budget *budget,
                        Prefix *prefix, AbError *error) {
    uint8_t lengths[PREFIX_SYMBOLS] = {0};
    uint32_t simple;
    if (alphabet > PREFIX_SYMBOLS || !ab_webp_bits_read(bits, 1, &simple, error)) {
        return false;
    }
    if (simple) {
        uint32_t two, wide, first;
        if (!ab_webp_bits_read(bits, 1, &two, error) ||
            !ab_webp_bits_read(bits, 1, &wide, error) ||
            !ab_webp_bits_read(bits, 1 + 7 * wide, &first, error) ||
            first >= alphabet) {
            return ab_fail(error, "Invalid simple WebP prefix code");
        }
        lengths[first] = 1;
        if (two) {
            uint32_t second;
            if (!ab_webp_bits_read(bits, 8, &second, error) || second >= alphabet) {
                return ab_fail(error, "Invalid second simple WebP prefix symbol");
            }
            lengths[second] = 1;
        }
    } else if (!read_lengths(bits, alphabet, budget, lengths, error)) {
        return false;
    }
    return build_prefix(lengths, alphabet, budget, prefix, error);
}

static bool prefix_value(AbWebpBits *bits, uint32_t code, uint32_t *output,
                         AbError *error) {
    if (code < 4) {
        *output = code + 1;
        return true;
    }
    if (code > 39) {
        ab_fail(error, "Invalid lossless WebP length/distance prefix");
        return false;
    }
    unsigned extra = (code - 2) / 2;
    uint32_t value;
    if (!ab_webp_bits_read(bits, extra, &value, error)) {
        return false;
    }
    *output = ((2 + (code & 1)) << extra) + value + 1;
    return true;
}

static uint32_t spatial_distance(uint32_t code, uint32_t width) {
    /* Normative WebP distance-code coordinates, independent of implementation. */
    static const int8_t coordinates[120][2] = {
        {0, 1},  {1, 0},  {1, 1},  {-1, 1}, {0, 2},  {2, 0},  {1, 2},  {-1, 2}, {2, 1},
        {-2, 1}, {2, 2},  {-2, 2}, {0, 3},  {3, 0},  {1, 3},  {-1, 3}, {3, 1},  {-3, 1},
        {2, 3},  {-2, 3}, {3, 2},  {-3, 2}, {0, 4},  {4, 0},  {1, 4},  {-1, 4}, {4, 1},
        {-4, 1}, {3, 3},  {-3, 3}, {2, 4},  {-2, 4}, {4, 2},  {-4, 2}, {0, 5},  {3, 4},
        {-3, 4}, {4, 3},  {-4, 3}, {5, 0},  {1, 5},  {-1, 5}, {5, 1},  {-5, 1}, {2, 5},
        {-2, 5}, {5, 2},  {-5, 2}, {4, 4},  {-4, 4}, {3, 5},  {-3, 5}, {5, 3},  {-5, 3},
        {0, 6},  {6, 0},  {1, 6},  {-1, 6}, {6, 1},  {-6, 1}, {2, 6},  {-2, 6}, {6, 2},
        {-6, 2}, {4, 5},  {-4, 5}, {5, 4},  {-5, 4}, {3, 6},  {-3, 6}, {6, 3},  {-6, 3},
        {0, 7},  {7, 0},  {1, 7},  {-1, 7}, {5, 5},  {-5, 5}, {7, 1},  {-7, 1}, {4, 6},
        {-4, 6}, {6, 4},  {-6, 4}, {2, 7},  {-2, 7}, {7, 2},  {-7, 2}, {3, 7},  {-3, 7},
        {7, 3},  {-7, 3}, {5, 6},  {-5, 6}, {6, 5},  {-6, 5}, {8, 0},  {4, 7},  {-4, 7},
        {7, 4},  {-7, 4}, {8, 1},  {8, 2},  {6, 6},  {-6, 6}, {8, 3},  {5, 7},  {-5, 7},
        {7, 5},  {-7, 5}, {8, 4},  {6, 7},  {-6, 7}, {7, 6},  {-7, 6}, {8, 5},  {7, 7},
        {-7, 7}, {8, 6},  {8, 7}};
    if (code > 120) {
        return code - 120;
    }
    int32_t distance =
        coordinates[code - 1][0] + coordinates[code - 1][1] * (int32_t)width;
    return distance < 1 ? 1 : (uint32_t)distance;
}

static void cache_pixel(uint32_t *cache, unsigned bits, uint32_t pixel) {
    if (bits) {
        uint32_t key = pixel * UINT32_C(0x1e35a7bd);
        cache[key >> (32 - bits)] = pixel;
    }
}

static bool decode_pixels(AbWebpBits *bits, uint32_t width, uint32_t height,
                          const PrefixGroup *groups, const uint32_t *selectors,
                          uint32_t block_width, unsigned block_bits, uint32_t *cache,
                          unsigned cache_bits, uint32_t *pixels, AbError *error) {
    size_t count = (size_t)width * height, position = 0;
    while (position < count) {
        size_t group = 0;
        if (selectors) {
            size_t x = position % width, y = position / width;
            group =
                (selectors[(y >> block_bits) * block_width + (x >> block_bits)] >> 8) &
                65535;
        }
        const Prefix *codes = groups[group].codes;
        uint32_t green;
        if (!prefix_symbol(bits, codes, &green, error)) {
            return false;
        }
        if (green < 256) {
            uint32_t red, blue, alpha;
            if (!prefix_symbol(bits, codes + 1, &red, error) ||
                !prefix_symbol(bits, codes + 2, &blue, error) ||
                !prefix_symbol(bits, codes + 3, &alpha, error)) {
                return false;
            }
            uint32_t pixel = (alpha << 24) | (red << 16) | (green << 8) | blue;
            pixels[position++] = pixel;
            cache_pixel(cache, cache_bits, pixel);
        } else if (green < 280) {
            uint32_t length, code, distance;
            if (!prefix_value(bits, green - 256, &length, error) ||
                !prefix_symbol(bits, codes + 4, &code, error) ||
                !prefix_value(bits, code, &distance, error)) {
                return false;
            }
            distance = spatial_distance(distance, width);
            if (distance > position || length > count - position) {
                return ab_fail(error, "Invalid lossless WebP backward reference");
            }
            for (uint32_t index = 0; index < length; ++index) {
                uint32_t pixel = pixels[position - distance];
                pixels[position++] = pixel;
                cache_pixel(cache, cache_bits, pixel);
            }
        } else {
            if (!cache_bits || green - 280 >= (1u << cache_bits)) {
                return ab_fail(error, "Invalid lossless WebP color-cache index");
            }
            uint32_t pixel = cache[green - 280];
            pixels[position++] = pixel;
            cache_pixel(cache, cache_bits, pixel);
        }
    }
    return true;
}

static bool entropy_image(AbWebpBits *bits, uint32_t width, uint32_t height,
                          bool spatial, Budget *budget, uint32_t **output,
                          AbError *error) {
    uint32_t *cache = NULL, *selectors = NULL, *pixels = NULL;
    PrefixGroup *groups = NULL;
    size_t cache_count = 0, selector_count = 0, group_count = 1;
    unsigned cache_bits = 0, block_bits = 0;
    uint32_t block_width = 0, value;
    size_t count = (size_t)width * height;
    bool okay = false;
    if (!ab_webp_bits_read(bits, 1, &value, error)) {
        goto release_image;
    }
    if (value) {
        if (!ab_webp_bits_read(bits, 4, &value, error) || !value || value > 11) {
            ab_fail(error, "Invalid lossless WebP color-cache size");
            goto release_image;
        }
        cache_bits = value;
        cache_count = (size_t)1 << cache_bits;
        cache = allocate(budget, cache_count, sizeof(*cache), error);
        if (!cache) {
            goto release_image;
        }
    }
    if (spatial) {
        if (!ab_webp_bits_read(bits, 1, &value, error)) {
            goto release_image;
        }
        if (value) {
            if (!ab_webp_bits_read(bits, 3, &value, error)) {
                goto release_image;
            }
            block_bits = value + 2;
            uint32_t round = (1u << block_bits) - 1;
            block_width = (width + round) >> block_bits;
            uint32_t block_height = (height + round) >> block_bits;
            selector_count = (size_t)block_width * block_height;
            if (!entropy_image(bits, block_width, block_height, false, budget,
                               &selectors, error)) {
                goto release_image;
            }
            for (size_t index = 0; index < selector_count; ++index) {
                size_t selected = (selectors[index] >> 8) & 65535;
                if (selected >= group_count) {
                    group_count = selected + 1;
                }
            }
        }
    }
    groups = allocate(budget, group_count, sizeof(*groups), error);
    if (!groups) {
        goto release_image;
    }
    const size_t alphabets[5] = {280 + cache_count, 256, 256, 256, 40};
    for (size_t group = 0; group < group_count; ++group) {
        for (size_t code = 0; code < 5; ++code) {
            if (!read_prefix(bits, alphabets[code], budget, groups[group].codes + code,
                             error)) {
                goto release_image;
            }
        }
    }
    pixels = allocate(budget, count, sizeof(*pixels), error);
    if (!pixels || !decode_pixels(bits, width, height, groups, selectors, block_width,
                                  block_bits, cache, cache_bits, pixels, error)) {
        goto release_image;
    }
    *output = pixels;
    pixels = NULL;
    okay = true;

release_image:
    if (groups) {
        for (size_t group = 0; group < group_count; ++group) {
            for (size_t code = 0; code < 5; ++code) {
                close_prefix(budget, groups[group].codes + code);
            }
        }
    }
    discard(budget, groups, group_count, sizeof(*groups));
    discard(budget, pixels, count, sizeof(*pixels));
    discard(budget, selectors, selector_count, sizeof(*selectors));
    discard(budget, cache, cache_count, sizeof(*cache));
    return okay;
}

static uint32_t add_channels(uint32_t first, uint32_t second) {
    uint32_t result = 0;
    for (unsigned shift = 0; shift < 32; shift += 8) {
        result |= (((first >> shift) + (second >> shift)) & 255) << shift;
    }
    return result;
}

static bool alpha_stream(AbWebpBits *bits, uint32_t width, uint32_t height,
                         Budget *budget, uint32_t **output, AbError *error) {
    uint32_t *palette = NULL, *encoded = NULL, *restored = NULL;
    uint32_t colors = 0, encoded_width = width;
    unsigned packed = 0;
    bool okay = false;
    uint32_t transform;
    while (true) {
        if (!ab_webp_bits_read(bits, 1, &transform, error)) {
            goto release_stream;
        }
        if (!transform) {
            break;
        }
        if (!ab_webp_bits_read(bits, 2, &transform, error)) {
            goto release_stream;
        }
        if (transform != 3 || palette) {
            ab_fail(error, "Unsupported/repeated lossless WebP alpha transform %u",
                    transform);
            goto release_stream;
        }
        if (!ab_webp_bits_read(bits, 8, &colors, error)) {
            goto release_stream;
        }
        ++colors;
        if (!entropy_image(bits, colors, 1, false, budget, &palette, error)) {
            goto release_stream;
        }
        for (uint32_t index = 1; index < colors; ++index) {
            palette[index] = add_channels(palette[index], palette[index - 1]);
        }
        packed = colors <= 2 ? 3 : colors <= 4 ? 2 : colors <= 16 ? 1 : 0;
        encoded_width = (width + ((1u << packed) - 1)) >> packed;
    }
    if (!entropy_image(bits, encoded_width, height, true, budget, &encoded, error)) {
        goto release_stream;
    }
    if (!palette) {
        *output = encoded;
        encoded = NULL;
        okay = true;
        goto release_stream;
    }
    if (packed) {
        restored = allocate(budget, (size_t)width * height, sizeof(*restored), error);
        if (!restored) {
            goto release_stream;
        }
    } else {
        restored = encoded;
        encoded = NULL;
    }
    unsigned field_bits = 8u >> packed;
    uint32_t mask = (1u << field_bits) - 1;
    const uint32_t *source = encoded ? encoded : restored;
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint32_t green =
                (source[(size_t)y * encoded_width + (x >> packed)] >> 8) & 255;
            unsigned shift = (x & ((1u << packed) - 1)) * field_bits;
            uint32_t index = (green >> shift) & mask;
            restored[(size_t)y * width + x] = index < colors ? palette[index] : 0;
        }
    }
    *output = restored;
    restored = NULL;
    okay = true;

release_stream:
    discard(budget, palette, colors, sizeof(*palette));
    discard(budget, encoded, (size_t)encoded_width * height, sizeof(*encoded));
    discard(budget, restored, (size_t)width * height, sizeof(*restored));
    return okay;
}

bool ab_webp_alpha_decode(const AbWebpStill *image, size_t allocation_limit,
                          AbBuffer *output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing WebP alpha output");
    }
    *output = (AbBuffer){0};
    if (!image || !image->width || !image->height || image->width > 16384 ||
        image->height > 16384 || image->width > SIZE_MAX / image->height ||
        image->alpha_filter > 3 || image->alpha_preprocessing ||
        (image->has_alpha && (image->lossless || !image->alpha.data))) {
        return ab_fail(error, "Invalid/unsupported WebP alpha input");
    }
    size_t count = (size_t)image->width * image->height;
    Budget budget = {.limit = allocation_limit};
    uint32_t *pixels = NULL;
    uint8_t *alpha = NULL;
    bool okay = false;
    if (image->has_alpha && image->compressed_alpha) {
        AbWebpBits bits;
        if (!ab_webp_bits_init(image->alpha, &bits, error) ||
            !alpha_stream(&bits, image->width, image->height, &budget, &pixels,
                          error)) {
            goto release_alpha;
        }
    } else if (image->has_alpha && image->alpha.size != count) {
        return ab_fail(error, "Invalid uncompressed WebP alpha size");
    }
    alpha = allocate(&budget, count, 1, error);
    if (!alpha) {
        goto release_alpha;
    }
    if (!image->has_alpha) {
        memset(alpha, 255, count);
    } else if (pixels) {
        for (size_t index = 0; index < count; ++index) {
            alpha[index] = (uint8_t)(pixels[index] >> 8);
        }
    } else {
        memcpy(alpha, image->alpha.data, count);
    }
    if (image->has_alpha &&
        !ab_webp_alpha_unfilter(alpha, count, image->width, image->height, image->width,
                                image->alpha_filter, 0, image->height, error)) {
        goto release_alpha;
    }
    *output = (AbBuffer){alpha, count};
    alpha = NULL;
    okay = true;

release_alpha:
    discard(&budget, pixels, count, sizeof(*pixels));
    discard(&budget, alpha, count, 1);
    return okay;
}
