#include "ab/image.h"

#include "ab/hash.h"
#include "ab/inflate.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t width;
    uint32_t height;
    unsigned channels;
    unsigned color;
    uint8_t palette[256][4];
    size_t palette_count;
    uint16_t transparent[3];
    bool has_transparency;
    bool saw_header;
    bool saw_data;
    bool data_finished;
    bool saw_end;
    size_t compressed_size;
} PngReader;

static uint32_t adler32(AbSpan bytes) {
    uint32_t first = 1;
    uint32_t second = 0;
    for (size_t index = 0; index < bytes.size; ++index) {
        first += bytes.data[index];
        if (first >= 65521) {
            first -= 65521;
        }
        second += first;
        if (second >= 65521) {
            second -= 65521;
        }
    }
    return second << 16 | first;
}

static bool image_header(PngReader *reader, AbSpan bytes, AbError *error) {
    if (reader->saw_header || bytes.size != 13) {
        return ab_fail(error, "Invalid or duplicate PNG IHDR");
    }
    reader->width = ab_u32be(bytes.data);
    reader->height = ab_u32be(bytes.data + 4);
    reader->color = bytes.data[9];
    if (!reader->width || !reader->height || reader->width > 16384 ||
        reader->height > 16384 || bytes.data[8] != 8 || bytes.data[10] ||
        bytes.data[11] || bytes.data[12]) {
        return ab_fail(
            error,
            "Unsupported PNG dimensions, depth, compression, filter, or interlace");
    }
    switch (reader->color) {
        case 0:
        case 3:
            reader->channels = 1;
            break;
        case 2:
            reader->channels = 3;
            break;
        case 4:
            reader->channels = 2;
            break;
        case 6:
            reader->channels = 4;
            break;
        default:
            return ab_fail(error, "Unsupported PNG color type");
    }
    reader->saw_header = true;
    return true;
}

static bool palette_chunk(PngReader *reader, AbSpan bytes, AbError *error) {
    if (reader->saw_data || reader->has_transparency || reader->palette_count ||
        !bytes.size || bytes.size > 768 || bytes.size % 3 || reader->color == 0 ||
        reader->color == 4) {
        return ab_fail(error, "Invalid PNG palette size, placement, or color type");
    }
    reader->palette_count = bytes.size / 3;
    for (size_t index = 0; index < reader->palette_count; ++index) {
        memcpy(reader->palette[index], bytes.data + index * 3, 3);
        reader->palette[index][3] = 255;
    }
    return true;
}

static bool transparency_chunk(PngReader *reader, AbSpan bytes, AbError *error) {
    if (reader->saw_data || reader->has_transparency) {
        return ab_fail(error, "Invalid PNG transparency placement");
    }
    if (reader->color == 3) {
        if (!reader->palette_count || !bytes.size ||
            bytes.size > reader->palette_count) {
            return ab_fail(error, "PNG transparency exceeds palette bounds");
        }
        for (size_t index = 0; index < bytes.size; ++index) {
            reader->palette[index][3] = bytes.data[index];
        }
    } else if ((reader->color == 0 && bytes.size == 2) ||
               (reader->color == 2 && bytes.size == 6)) {
        for (size_t index = 0; index < bytes.size / 2; ++index) {
            uint16_t value = (uint16_t)((uint16_t)bytes.data[index * 2] << 8 |
                                        bytes.data[index * 2 + 1]);
            if (value > 255) {
                return ab_fail(error, "PNG transparency key exceeds sample depth");
            }
            reader->transparent[index] = value;
        }
    } else {
        return ab_fail(error, "Invalid PNG transparency for color type");
    }
    reader->has_transparency = true;
    return true;
}

static bool inspect_chunks(AbSpan input, PngReader *reader, uint8_t *compressed,
                           AbError *error) {
    size_t offset = 8;
    size_t copied = 0;
    while (offset < input.size) {
        if (!ab_range(input.size, offset, 12)) {
            return ab_fail(error, "Truncated PNG chunk header");
        }
        const uint8_t *chunk = input.data + offset;
        size_t size = ab_u32be(chunk);
        if (size > UINT32_C(0x7fffffff) || !ab_range(input.size, offset + 12, size)) {
            return ab_fail(error, "PNG chunk exceeds input bounds");
        }
        const uint8_t *type = chunk + 4;
        for (size_t index = 0; index < 4; ++index) {
            if (!((type[index] >= 'A' && type[index] <= 'Z') ||
                  (type[index] >= 'a' && type[index] <= 'z'))) {
                return ab_fail(error, "Invalid PNG chunk type");
            }
        }
        if ((type[2] & 32) ||
            ab_crc32((AbSpan){type, size + 4}) != ab_u32be(chunk + size + 8)) {
            return ab_fail(error, "PNG reserved type bit or chunk CRC mismatch");
        }
        AbSpan bytes = {chunk + 8, size};
        bool is_data = memcmp(type, "IDAT", 4) == 0;
        if (compressed) {
            if (is_data) {
                if (size > reader->compressed_size - copied) {
                    return ab_fail(error, "PNG IDAT sizes changed between passes");
                }
                memcpy(compressed + copied, bytes.data, size);
                copied += size;
            }
            offset += 12 + size;
            continue;
        }
        if (reader->saw_end) {
            return ab_fail(error, "PNG contains data after IEND");
        }
        if (memcmp(type, "IHDR", 4) == 0) {
            if (offset != 8) {
                return ab_fail(error, "PNG IHDR must be the first and only header");
            }
            if (!image_header(reader, bytes, error)) {
                return false;
            }
        } else if (!reader->saw_header) {
            return ab_fail(error, "PNG chunk precedes IHDR");
        } else if (memcmp(type, "PLTE", 4) == 0) {
            if (!palette_chunk(reader, bytes, error)) {
                return false;
            }
        } else if (memcmp(type, "tRNS", 4) == 0) {
            if (!transparency_chunk(reader, bytes, error)) {
                return false;
            }
        } else if (is_data) {
            if (reader->data_finished ||
                (reader->color == 3 && !reader->palette_count) ||
                size > input.size - reader->compressed_size) {
                return ab_fail(error, "Invalid PNG IDAT placement or total size");
            }
            reader->saw_data = true;
            reader->compressed_size += size;
        } else if (memcmp(type, "IEND", 4) == 0) {
            if (size || !reader->saw_data) {
                return ab_fail(error, "Invalid PNG IEND");
            }
            reader->saw_end = true;
        } else if (!(type[0] & 32)) {
            return ab_fail(error, "Unsupported critical PNG chunk");
        } else if (memcmp(type, "acTL", 4) == 0 || memcmp(type, "fcTL", 4) == 0 ||
                   memcmp(type, "fdAT", 4) == 0) {
            return ab_fail(error, "Animated PNG requires separate animation support");
        }
        if (reader->saw_data && !is_data) {
            reader->data_finished = true;
        }
        offset += 12 + size;
    }
    if (compressed) {
        return copied == reader->compressed_size ||
               ab_fail(error, "PNG IDAT copy size mismatch");
    }
    return (reader->saw_end && reader->compressed_size >= 6) ||
           ab_fail(error, "PNG is missing IEND or its zlib stream");
}

static unsigned paeth(unsigned left, unsigned above, unsigned corner) {
    int predicted = (int)left + (int)above - (int)corner;
    int left_error = abs(predicted - (int)left);
    int above_error = abs(predicted - (int)above);
    int corner_error = abs(predicted - (int)corner);
    if (left_error <= above_error && left_error <= corner_error) {
        return left;
    }
    return above_error <= corner_error ? above : corner;
}

static bool unfilter_rows(uint8_t *rows, size_t row_size, uint32_t height,
                          unsigned channels, AbError *error) {
    size_t stride = row_size + 1;
    for (uint32_t y = 0; y < height; ++y) {
        uint8_t *row = rows + (size_t)y * stride + 1;
        const uint8_t *above = y ? row - stride : NULL;
        unsigned filter = row[-1];
        if (filter > 4) {
            return ab_fail(error, "Unsupported PNG row filter");
        }
        for (size_t x = 0; x < row_size; ++x) {
            unsigned left = x >= channels ? row[x - channels] : 0;
            unsigned upper = above ? above[x] : 0;
            unsigned corner = above && x >= channels ? above[x - channels] : 0;
            unsigned prediction = 0;
            switch (filter) {
                case 1:
                    prediction = left;
                    break;
                case 2:
                    prediction = upper;
                    break;
                case 3:
                    prediction = (left + upper) / 2;
                    break;
                case 4:
                    prediction = paeth(left, upper, corner);
                    break;
            }
            row[x] = (uint8_t)(row[x] + prediction);
        }
    }
    return true;
}

static bool expand_rgba(const PngReader *reader, const uint8_t *rows, size_t row_size,
                        uint8_t *output, AbError *error) {
    size_t stride = row_size + 1;
    for (uint32_t y = 0; y < reader->height; ++y) {
        const uint8_t *row = rows + (size_t)y * stride + 1;
        for (uint32_t x = 0; x < reader->width; ++x) {
            const uint8_t *source = row + (size_t)x * reader->channels;
            uint8_t *pixel = output + ((size_t)y * reader->width + x) * 4;
            if (reader->color == 3) {
                if (source[0] >= reader->palette_count) {
                    return ab_fail(error, "PNG sample index exceeds palette");
                }
                memcpy(pixel, reader->palette[source[0]], 4);
            } else if (reader->color == 6) {
                memcpy(pixel, source, 4);
            } else if (reader->color == 2) {
                memcpy(pixel, source, 3);
                bool transparent = reader->has_transparency &&
                                   source[0] == reader->transparent[0] &&
                                   source[1] == reader->transparent[1] &&
                                   source[2] == reader->transparent[2];
                pixel[3] = transparent ? 0 : 255;
            } else {
                pixel[0] = source[0];
                pixel[1] = source[0];
                pixel[2] = source[0];
                bool transparent =
                    reader->has_transparency && source[0] == reader->transparent[0];
                pixel[3] = reader->color == 4 ? source[1] : (transparent ? 0 : 255);
            }
        }
    }
    return true;
}

void ab_image_free(AbImage *image) {
    if (image) {
        ab_buffer_free(&image->rgba);
        *image = (AbImage){0};
    }
}

static bool inspect_png(AbSpan input, PngReader *reader, AbError *error) {
    static const uint8_t signature[8] = {137, 'P', 'N', 'G', 13, 10, 26, 10};
    if (!input.data || input.size < 8 || memcmp(input.data, signature, 8) != 0) {
        return ab_fail(error, "Invalid PNG signature");
    }
    return inspect_chunks(input, reader, NULL, error);
}

bool ab_png_read_info(AbSpan input, AbPngInfo *output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing PNG metadata output");
    }
    PngReader reader = {0};
    if (!inspect_png(input, &reader, error)) {
        return false;
    }
    *output = (AbPngInfo){reader.width, reader.height, (uint8_t)reader.color,
                          reader.has_transparency};
    return true;
}

bool ab_png_decode(AbSpan input, size_t decoded_limit, AbImage *output,
                   AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing PNG output");
    }
    *output = (AbImage){0};
    PngReader reader = {0};
    if (!inspect_png(input, &reader, error)) {
        return false;
    }
    size_t width = reader.width;
    size_t height = reader.height;
    if (width > SIZE_MAX / 4 || height > decoded_limit / (width * 4)) {
        return ab_fail(error, "PNG decoded pixels exceed size limit");
    }
    size_t pixel_size = width * height * 4;
    size_t row_size = width * reader.channels;
    if (height > decoded_limit / (row_size + 1)) {
        return ab_fail(error, "PNG filtered scanlines exceed size limit");
    }
    size_t filtered_size = (row_size + 1) * height;
    uint8_t *compressed = malloc(reader.compressed_size);
    uint8_t *rows = malloc(filtered_size);
    uint8_t *pixels = malloc(pixel_size);
    bool okay = false;
    if (!compressed || !rows || !pixels) {
        ab_fail(error, "Cannot allocate PNG decoding buffers");
        goto release_buffers;
    }
    if (!inspect_chunks(input, &reader, compressed, error)) {
        goto release_buffers;
    }
    unsigned zlib_header = (unsigned)compressed[0] << 8 | compressed[1];
    if ((compressed[0] & 15) != 8 || compressed[0] >> 4 > 7 || (compressed[1] & 32) ||
        zlib_header % 31) {
        ab_fail(error, "Unsupported PNG zlib header or preset dictionary");
        goto release_buffers;
    }
    AbSpan deflate = {compressed + 2, reader.compressed_size - 6};
    if (!ab_inflate_raw(deflate, rows, filtered_size, error)) {
        goto release_buffers;
    }
    if (adler32((AbSpan){rows, filtered_size}) !=
        ab_u32be(compressed + reader.compressed_size - 4)) {
        ab_fail(error, "PNG zlib Adler-32 mismatch");
        goto release_buffers;
    }
    if (!unfilter_rows(rows, row_size, reader.height, reader.channels, error) ||
        !expand_rgba(&reader, rows, row_size, pixels, error)) {
        goto release_buffers;
    }
    *output = (AbImage){reader.width, reader.height, {pixels, pixel_size}};
    pixels = NULL;
    okay = true;

release_buffers:
    free(pixels);
    free(rows);
    free(compressed);
    return okay;
}
