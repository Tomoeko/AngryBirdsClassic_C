#include "webp_internal.h"

bool ab_webp_bits_init(AbSpan bytes, AbWebpBits *output, AbError *error) {
    if (!output || (bytes.size && !bytes.data)) {
        return ab_fail(error, "Invalid lossless WebP bit reader input");
    }
    *output = (AbWebpBits){.bytes = bytes};
    return true;
}

bool ab_webp_bits_read(AbWebpBits *reader, unsigned count, uint32_t *output,
                       AbError *error) {
    if (!reader || !output || count > 24 || reader->bit > 7 ||
        reader->offset > reader->bytes.size ||
        (reader->bytes.size && !reader->bytes.data) ||
        (reader->offset == reader->bytes.size && reader->bit)) {
        return ab_fail(error, "Invalid lossless WebP bit field/state");
    }
    unsigned total = reader->bit + count;
    size_t needed = (total + 7) / 8;
    if (needed > reader->bytes.size - reader->offset) {
        return ab_fail(error, "Truncated lossless WebP bit field");
    }
    uint32_t value = 0;
    for (size_t byte = 0; byte < needed; ++byte) {
        value |= (uint32_t)reader->bytes.data[reader->offset + byte] << (byte * 8);
    }
    value >>= reader->bit;
    *output = value & ((UINT32_C(1) << count) - 1);
    reader->offset += total / 8;
    reader->bit = total % 8;
    return true;
}
