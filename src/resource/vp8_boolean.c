#include "vp8_boolean.h"

static bool valid_reader(const AbVp8Boolean *reader) {
    return reader && reader->bytes.data && reader->bytes.size >= 2 &&
           reader->offset >= 2 && reader->offset <= reader->bytes.size &&
           reader->range >= 128 && reader->range <= 255 && reader->shifted < 8 &&
           reader->value < (reader->range << 8);
}

bool ab_vp8_boolean_init(AbSpan bytes, AbVp8Boolean *output, AbError *error) {
    if (!output || !bytes.data || bytes.size < 2 || bytes.data[0] == 255) {
        return ab_fail(error, "Invalid VP8 arithmetic reader input/initial interval");
    }
    *output = (AbVp8Boolean){.bytes = bytes,
                             .offset = 2,
                             .range = 255,
                             .value = ((uint32_t)bytes.data[0] << 8) | bytes.data[1]};
    return true;
}

static bool read_bit(AbVp8Boolean *reader, uint8_t probability, uint32_t *output,
                     AbError *error) {
    uint32_t split = 1 + (((reader->range - 1) * probability) >> 8);
    uint32_t boundary = split << 8;
    uint32_t bit = reader->value >= boundary;
    if (bit) {
        reader->value -= boundary;
        reader->range -= split;
    } else {
        reader->range = split;
    }
    while (reader->range < 128) {
        reader->range *= 2;
        reader->value *= 2;
        if (++reader->shifted == 8) {
            if (reader->offset == reader->bytes.size) {
                ab_fail(error, "Truncated VP8 arithmetic field");
                return false;
            }
            reader->value |= reader->bytes.data[reader->offset++];
            reader->shifted = 0;
        }
    }
    *output = bit;
    return true;
}

bool ab_vp8_boolean_bit(AbVp8Boolean *reader, uint8_t probability, uint32_t *output,
                        AbError *error) {
    if (!valid_reader(reader) || !output) {
        return ab_fail(error, "Invalid VP8 arithmetic bit reader/output");
    }
    AbVp8Boolean state = *reader;
    uint32_t value;
    if (!read_bit(&state, probability, &value, error)) {
        return false;
    }
    *reader = state;
    *output = value;
    return true;
}

static bool read_value(AbVp8Boolean *reader, unsigned count, uint32_t *output,
                       AbError *error) {
    uint32_t value = 0;
    for (unsigned index = 0; index < count; ++index) {
        uint32_t bit;
        if (!read_bit(reader, 128, &bit, error)) {
            return false;
        }
        value = (value << 1) | bit;
    }
    *output = value;
    return true;
}

bool ab_vp8_boolean_value(AbVp8Boolean *reader, unsigned count, uint32_t *output,
                          AbError *error) {
    if (!valid_reader(reader) || !output || count > 24) {
        return ab_fail(error, "Invalid VP8 unsigned arithmetic field");
    }
    AbVp8Boolean state = *reader;
    uint32_t value;
    if (!read_value(&state, count, &value, error)) {
        return false;
    }
    *reader = state;
    *output = value;
    return true;
}

bool ab_vp8_boolean_signed(AbVp8Boolean *reader, unsigned count, int32_t *output,
                           AbError *error) {
    if (!valid_reader(reader) || !output || count > 24) {
        return ab_fail(error, "Invalid VP8 signed arithmetic field");
    }
    AbVp8Boolean state = *reader;
    uint32_t value, negative;
    if (!read_value(&state, count, &value, error) ||
        !read_bit(&state, 128, &negative, error)) {
        return false;
    }
    *reader = state;
    *output = negative ? -(int32_t)value : (int32_t)value;
    return true;
}
