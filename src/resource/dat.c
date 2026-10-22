#include "ab/dat.h"

#include <string.h>

bool ab_dat_bytes(AbDatReader *reader, size_t count, AbSpan *output, AbError *error) {
    if (!reader || !output || (reader->bytes.size && !reader->bytes.data) ||
        !ab_slice(reader->bytes, reader->offset, count, output)) {
        return ab_fail(error, "Truncated DAT field");
    }
    reader->offset += count;
    return true;
}

bool ab_dat_u16(AbDatReader *reader, uint16_t *output, AbError *error) {
    AbSpan bytes;
    if (!output || !ab_dat_bytes(reader, 2, &bytes, error)) {
        return ab_fail(error, "Cannot read DAT 16-bit field");
    }
    *output = (uint16_t)((uint16_t)bytes.data[0] << 8 | bytes.data[1]);
    return true;
}

bool ab_dat_u32(AbDatReader *reader, uint32_t *output, AbError *error) {
    AbSpan bytes;
    if (!output || !ab_dat_bytes(reader, 4, &bytes, error)) {
        return ab_fail(error, "Cannot read DAT 32-bit field");
    }
    *output = ab_u32be(bytes.data);
    return true;
}

bool ab_dat_i16(AbDatReader *reader, int16_t *output, AbError *error) {
    uint16_t value = 0;
    if (!output || !ab_dat_u16(reader, &value, error)) {
        return ab_fail(error, "Cannot read signed DAT 16-bit field");
    }
    int32_t signed_value = value < 0x8000 ? (int32_t)value : (int32_t)value - 65536;
    *output = (int16_t)signed_value;
    return true;
}

bool ab_dat_string(AbDatReader *reader, AbSpan *output, AbError *error) {
    int16_t count = 0;
    if (!output || !ab_dat_i16(reader, &count, error) || count < 0) {
        return ab_fail(error, "Invalid DAT string length");
    }
    return ab_dat_bytes(reader, (size_t)count, output, error);
}

bool ab_dat_next(AbDatReader *reader, AbDatChunk *chunk, AbError *error) {
    AbSpan header;
    if (!chunk || !ab_dat_bytes(reader, 8, &header, error)) {
        return ab_fail(error, "Truncated DAT chunk header");
    }
    uint32_t size = ab_u32be(header.data + 4);
    if (size > INT32_MAX || !ab_dat_bytes(reader, size, &chunk->payload, error)) {
        return ab_fail(error, "Invalid DAT chunk size");
    }
    chunk->tag = (AbSpan){header.data, 4};
    return true;
}

bool ab_dat_open(AbSpan input, const char tag[4], AbDatReader *reader, AbError *error) {
    if (!reader) {
        return ab_fail(error, "Missing DAT output");
    }
    *reader = (AbDatReader){0};
    AbDatReader container = {input, 0};
    AbDatChunk root;
    if (!tag || !ab_dat_next(&container, &root, error)) {
        return false;
    }
    if (memcmp(root.tag.data, tag, 4) != 0 || container.offset != input.size) {
        return ab_fail(error, "Invalid DAT root tag or length");
    }
    *reader = (AbDatReader){root.payload, 0};
    return true;
}
