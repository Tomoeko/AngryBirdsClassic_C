#include "ab/pack.h"

#include "ab/hash.h"
#include "ab/lzma.h"

#include <string.h>

typedef struct {
    AbSpan bytes;
    size_t offset;
    AbError *error;
} HeaderReader;

static uint64_t fixed64(const uint8_t *bytes) {
    return (uint64_t)ab_u32le(bytes) | (uint64_t)ab_u32le(bytes + 4) << 32;
}

static bool read_byte(HeaderReader *reader, uint8_t *value) {
    if (reader->offset == reader->bytes.size) {
        return ab_fail(reader->error, "Truncated 7z header");
    }
    *value = reader->bytes.data[reader->offset++];
    return true;
}

static bool expect_byte(HeaderReader *reader, uint8_t expected) {
    uint8_t value = 0;
    return read_byte(reader, &value) &&
           (value == expected ||
            ab_fail(reader->error,
                    "Unsupported 7z header property 0x%02x; expected 0x%02x", value,
                    expected));
}

static bool read_number(HeaderReader *reader, uint64_t *value) {
    uint8_t first = 0;
    if (!read_byte(reader, &first)) {
        return false;
    }
    *value = 0;
    unsigned mask = 128;
    for (unsigned index = 0; index < 8; ++index) {
        if (!(first & mask)) {
            *value |= (uint64_t)(first & (mask - 1)) << (index * 8);
            return true;
        }
        uint8_t byte = 0;
        if (!read_byte(reader, &byte)) {
            return false;
        }
        *value |= (uint64_t)byte << (index * 8);
        mask >>= 1;
    }
    return true;
}

static bool expect_number(HeaderReader *reader, uint64_t expected) {
    uint64_t value = 0;
    return read_number(reader, &value) &&
           (value == expected ||
            ab_fail(reader->error, "Unsupported 7z stream or coder count"));
}

static bool take_bytes(HeaderReader *reader, uint64_t count, AbSpan *output) {
    if (count > SIZE_MAX ||
        !ab_slice(reader->bytes, reader->offset, (size_t)count, output)) {
        return ab_fail(reader->error, "7z property exceeds header bounds");
    }
    reader->offset += (size_t)count;
    return true;
}

static bool single_digest(HeaderReader *reader, bool *defined, uint32_t *crc) {
    uint8_t all = 0;
    if (!read_byte(reader, &all)) {
        return false;
    }
    *defined = all != 0;
    if (all > 1) {
        return ab_fail(reader->error, "Invalid 7z digest definition flag");
    }
    if (!all) {
        uint8_t mask = 0;
        if (!read_byte(reader, &mask)) {
            return false;
        }
        *defined = (mask & 128) != 0;
    }
    if (*defined) {
        AbSpan bytes;
        if (!take_bytes(reader, 4, &bytes)) {
            return false;
        }
        *crc = ab_u32le(bytes.data);
    }
    return true;
}

static bool optional_digest(HeaderReader *reader, bool *defined, uint32_t *crc) {
    if (reader->offset < reader->bytes.size &&
        reader->bytes.data[reader->offset] == 0x0a) {
        ++reader->offset;
        return single_digest(reader, defined, crc);
    }
    return true;
}

static bool file_metadata(HeaderReader *reader) {
    if (!expect_byte(reader, 5) || !expect_number(reader, 1)) {
        return false;
    }
    bool named = false;
    for (;;) {
        uint8_t type = 0;
        uint64_t size = 0;
        AbSpan data;
        if (!read_byte(reader, &type)) {
            return false;
        }
        if (!type) {
            break;
        }
        if (!read_number(reader, &size) || !take_bytes(reader, size, &data)) {
            return false;
        }
        if (type == 0x11) {
            if (named || data.size < 5 || data.size % 2 != 1 || data.data[0] != 0 ||
                ab_u16le(data.data + data.size - 2) != 0) {
                return ab_fail(reader->error, "Invalid or external 7z filename");
            }
            for (size_t offset = 1; offset + 2 < data.size; offset += 2) {
                if (!ab_u16le(data.data + offset)) {
                    return ab_fail(reader->error, "Multiple or empty 7z filenames");
                }
            }
            named = true;
        } else if (type == 0x0e) {
            if (data.size != 1 || data.data[0] & 128) {
                return ab_fail(reader->error, "Empty 7z streams are unsupported");
            }
        } else if (type == 0x12 || type == 0x13 || type == 0x14 || type == 0x15) {
            /* Metadata is not game content; validate the bounded property shape. */
            HeaderReader property = {data, 0, reader->error};
            uint8_t all = 0;
            uint8_t external = 0;
            if (!read_byte(&property, &all) || all > 1) {
                return ab_fail(reader->error, "Invalid 7z metadata definition flag");
            }
            bool defined = all != 0;
            if (!all) {
                uint8_t mask = 0;
                if (!read_byte(&property, &mask)) {
                    return false;
                }
                defined = (mask & 128) != 0;
            }
            if (!read_byte(&property, &external) || external) {
                return ab_fail(reader->error, "External 7z metadata is unsupported");
            }
            size_t expected = defined ? (type == 0x15 ? 4 : 8) : 0;
            if (data.size - property.offset != expected) {
                return ab_fail(reader->error, "Invalid 7z metadata length");
            }
        } else if (type != 0x19) {
            return ab_fail(reader->error, "Unsupported 7z file property 0x%02x", type);
        }
    }
    return (named && expect_byte(reader, 0) && reader->offset == reader->bytes.size) ||
           ab_fail(reader->error, "Incomplete 7z file header or trailing properties");
}

static bool sevenzip_single(AbSpan input, size_t limit, AbBuffer *output,
                            AbError *error) {
    if (input.size < 32 || input.data[6] != 0 ||
        (input.data[7] != 3 && input.data[7] != 4)) {
        return ab_fail(error, "Unsupported or truncated 7z signature header");
    }
    if (ab_crc32((AbSpan){input.data + 12, 20}) != ab_u32le(input.data + 8)) {
        return ab_fail(error, "7z start-header CRC mismatch");
    }
    uint64_t next_offset = fixed64(input.data + 12);
    uint64_t next_size = fixed64(input.data + 20);
    if (next_offset > input.size - 32 || next_size != input.size - 32 - next_offset) {
        return ab_fail(error, "7z next-header bounds or archive length are invalid");
    }
    AbSpan header = {input.data + 32 + (size_t)next_offset, (size_t)next_size};
    if (ab_crc32(header) != ab_u32le(input.data + 28)) {
        return ab_fail(error, "7z next-header CRC mismatch");
    }
    HeaderReader reader = {header, 0, error};
    uint64_t pack_position = 0;
    uint64_t pack_size = 0;
    uint64_t decoded_size = 0;
    bool pack_crc_defined = false;
    bool decoded_crc_defined = false;
    uint32_t pack_crc = 0;
    uint32_t decoded_crc = 0;
    AbSpan properties;
    if (!expect_byte(&reader, 1) || !expect_byte(&reader, 4) ||
        !expect_byte(&reader, 6) || !read_number(&reader, &pack_position) ||
        !expect_number(&reader, 1) || !expect_byte(&reader, 9) ||
        !read_number(&reader, &pack_size) ||
        !optional_digest(&reader, &pack_crc_defined, &pack_crc) ||
        !expect_byte(&reader, 0) || !expect_byte(&reader, 7) ||
        !expect_byte(&reader, 0x0b) || !expect_number(&reader, 1) ||
        !expect_byte(&reader, 0) || !expect_number(&reader, 1) ||
        !expect_byte(&reader, 0x23) || !expect_byte(&reader, 3) ||
        !expect_byte(&reader, 1) || !expect_byte(&reader, 1) ||
        !expect_number(&reader, 5) || !take_bytes(&reader, 5, &properties) ||
        !expect_byte(&reader, 0x0c) || !read_number(&reader, &decoded_size) ||
        !optional_digest(&reader, &decoded_crc_defined, &decoded_crc) ||
        !expect_byte(&reader, 0)) {
        return false;
    }
    if (reader.offset < reader.bytes.size && reader.bytes.data[reader.offset] == 8) {
        ++reader.offset;
        if (reader.offset < reader.bytes.size &&
            reader.bytes.data[reader.offset] == 0x0d) {
            ++reader.offset;
            if (!expect_number(&reader, 1)) {
                return false;
            }
        }
        if (!optional_digest(&reader, &decoded_crc_defined, &decoded_crc) ||
            !expect_byte(&reader, 0)) {
            return false;
        }
    }
    if (!expect_byte(&reader, 0) || !file_metadata(&reader)) {
        return false;
    }
    if (pack_position > next_offset || pack_size > next_offset - pack_position ||
        !decoded_crc_defined || decoded_size > limit) {
        return ab_fail(error,
                       "Unsupported 7z stream bounds, size, or missing content CRC");
    }
    AbSpan packed = {input.data + 32 + (size_t)pack_position, (size_t)pack_size};
    if (pack_crc_defined && ab_crc32(packed) != pack_crc) {
        return ab_fail(error, "7z packed-stream CRC mismatch");
    }
    if (!ab_lzma_raw(packed, properties, decoded_size, limit, output, error)) {
        return false;
    }
    if (ab_crc32((AbSpan){output->data, output->size}) != decoded_crc) {
        ab_buffer_free(output);
        return ab_fail(error, "7z decoded content CRC mismatch");
    }
    return true;
}

bool ab_asset_unpack(AbSpan input, size_t limit, AbBuffer *output, AbError *error) {
    static const uint8_t sevenzip_magic[6] = {'7', 'z', 0xbc, 0xaf, 0x27, 0x1c};
    if (!output) {
        return ab_fail(error, "Missing asset-unpack output");
    }
    *output = (AbBuffer){0};
    if (input.data && input.size >= 6 &&
        memcmp(input.data, sevenzip_magic, sizeof(sevenzip_magic)) == 0) {
        return sevenzip_single(input, limit, output, error);
    }
    return ab_lzma_asset(input, limit, output, error);
}
