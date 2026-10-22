#include "ab/lzma.h"

#include <stdlib.h>
#include <string.h>

enum {
    MATCH = 0,
    REP = MATCH + 12 * 16,
    REP_FIRST = REP + 12,
    REP_SECOND = REP_FIRST + 12,
    REP_THIRD = REP_SECOND + 12,
    REP_SHORT = REP_THIRD + 12,
    SLOT = REP_SHORT + 12 * 16,
    DISTANCE = SLOT + 4 * 64,
    ALIGN = DISTANCE + 114,
    LENGTH = ALIGN + 16,
    REP_LENGTH = LENGTH + 514,
    LITERAL = REP_LENGTH + 514,
};

typedef struct {
    AbSpan input;
    size_t offset;
    uint32_t range;
    uint32_t code;
    bool failed;
} RangeDecoder;

typedef struct {
    RangeDecoder coder;
    uint16_t *probability;
    uint8_t *bytes;
    size_t size;
    size_t written;
    uint32_t history[4];
    uint32_t dictionary_size;
    unsigned state;
    unsigned literal_bits;
    unsigned literal_position_bits;
    unsigned position_bits;
} LzmaDecoder;

static void normalize(RangeDecoder *coder) {
    if (coder->range < UINT32_C(0x01000000)) {
        if (coder->offset == coder->input.size) {
            coder->failed = true;
            return;
        }
        coder->range <<= 8;
        coder->code = (coder->code << 8) | coder->input.data[coder->offset++];
    }
}

static unsigned model_bit(RangeDecoder *coder, uint16_t *probability) {
    uint32_t boundary = (coder->range >> 11) * *probability;
    unsigned bit = coder->code >= boundary;
    if (bit) {
        coder->code -= boundary;
        coder->range -= boundary;
        *probability = (uint16_t)(*probability - (*probability >> 5));
    } else {
        coder->range = boundary;
        *probability = (uint16_t)(*probability + ((2048 - *probability) >> 5));
    }
    normalize(coder);
    return bit;
}

static uint32_t tree_value(RangeDecoder *coder, uint16_t *probability, unsigned bits) {
    uint32_t node = 1;
    for (unsigned bit = 0; bit < bits; ++bit) {
        node = node * 2 + model_bit(coder, probability + node);
    }
    return node - (UINT32_C(1) << bits);
}

static uint32_t reverse_tree(RangeDecoder *coder, uint16_t *probability, int base,
                             unsigned bits) {
    uint32_t node = 1;
    uint32_t value = 0;
    for (unsigned position = 0; position < bits; ++position) {
        unsigned bit = model_bit(coder, probability + (size_t)(base + (int)node));
        node = node * 2 + bit;
        value |= (uint32_t)bit << position;
    }
    return value;
}

static uint32_t direct_value(RangeDecoder *coder, unsigned bits) {
    uint32_t value = 0;
    for (unsigned position = 0; position < bits; ++position) {
        coder->range >>= 1;
        unsigned bit = coder->code >= coder->range;
        if (bit) {
            coder->code -= coder->range;
        }
        value = (value << 1) | bit;
        normalize(coder);
    }
    return value;
}

static uint32_t match_length(LzmaDecoder *decoder, size_t base, unsigned position) {
    uint16_t *probability = decoder->probability + base;
    if (!model_bit(&decoder->coder, probability)) {
        return 2 + tree_value(&decoder->coder, probability + 2 + position * 8, 3);
    }
    if (!model_bit(&decoder->coder, probability + 1)) {
        return 10 +
               tree_value(&decoder->coder, probability + 2 + 128 + position * 8, 3);
    }
    return 18 + tree_value(&decoder->coder, probability + 2 + 256, 8);
}

static bool literal_byte(LzmaDecoder *decoder, AbError *error) {
    if (decoder->written == decoder->size) {
        return ab_fail(error, "LZMA literal exceeds declared output size");
    }
    unsigned previous = decoder->written ? decoder->bytes[decoder->written - 1] : 0;
    size_t position =
        decoder->written & ((UINT32_C(1) << decoder->literal_position_bits) - 1);
    size_t context =
        (position << decoder->literal_bits) | (previous >> (8 - decoder->literal_bits));
    uint16_t *probability = decoder->probability + LITERAL + context * 768;
    uint32_t node = 1;
    if (decoder->state >= 7) {
        if (decoder->history[0] > decoder->written) {
            return ab_fail(error, "LZMA matched literal has no history");
        }
        unsigned match = decoder->bytes[decoder->written - decoder->history[0]];
        do {
            unsigned predicted = (match >> 7) & 1;
            match <<= 1;
            unsigned bit =
                model_bit(&decoder->coder, probability + ((1 + predicted) << 8) + node);
            node = node * 2 + bit;
            if (bit != predicted) {
                break;
            }
        } while (node < 256);
    }
    while (node < 256) {
        node = node * 2 + model_bit(&decoder->coder, probability + node);
    }
    decoder->bytes[decoder->written++] = (uint8_t)(node - 256);
    if (decoder->state < 4) {
        decoder->state = 0;
    } else if (decoder->state < 10) {
        decoder->state -= 3;
    } else {
        decoder->state -= 6;
    }
    return true;
}

static bool copy_match(LzmaDecoder *decoder, uint32_t length, AbError *error) {
    uint32_t back = decoder->history[0];
    if (!back || back > decoder->written || back > decoder->dictionary_size ||
        length > decoder->size - decoder->written) {
        return ab_fail(error, "LZMA match exceeds history, dictionary, or output size");
    }
    for (uint32_t index = 0; index < length; ++index) {
        decoder->bytes[decoder->written] = decoder->bytes[decoder->written - back];
        ++decoder->written;
    }
    return true;
}

static bool repeated_match(LzmaDecoder *decoder, unsigned position, AbError *error) {
    uint16_t *probability = decoder->probability;
    unsigned state = decoder->state;
    if (!model_bit(&decoder->coder, probability + REP_FIRST + state)) {
        if (!model_bit(&decoder->coder,
                       probability + REP_SHORT + state * 16 + position)) {
            decoder->state = state < 7 ? 9 : 11;
            return copy_match(decoder, 1, error);
        }
    } else {
        uint32_t distance = 0;
        if (!model_bit(&decoder->coder, probability + REP_SECOND + state)) {
            distance = decoder->history[1];
        } else {
            if (!model_bit(&decoder->coder, probability + REP_THIRD + state)) {
                distance = decoder->history[2];
            } else {
                distance = decoder->history[3];
                decoder->history[3] = decoder->history[2];
            }
            decoder->history[2] = decoder->history[1];
        }
        decoder->history[1] = decoder->history[0];
        decoder->history[0] = distance;
    }
    uint32_t length = match_length(decoder, REP_LENGTH, position);
    decoder->state = state < 7 ? 8 : 11;
    return copy_match(decoder, length, error);
}

static bool new_match(LzmaDecoder *decoder, unsigned position, bool *ended,
                      AbError *error) {
    uint32_t length = match_length(decoder, LENGTH, position);
    unsigned length_context = length - 2 < 4 ? length - 2 : 3;
    uint32_t slot = tree_value(&decoder->coder,
                               decoder->probability + SLOT + length_context * 64, 6);
    uint32_t distance = slot;
    if (slot >= 4) {
        unsigned bits = (slot >> 1) - 1;
        distance = (2 | (slot & 1)) << bits;
        if (slot < 14) {
            int base = (int)distance - (int)slot - 1;
            distance += reverse_tree(&decoder->coder, decoder->probability + DISTANCE,
                                     base, bits);
        } else {
            distance += direct_value(&decoder->coder, bits - 4) << 4;
            distance +=
                reverse_tree(&decoder->coder, decoder->probability + ALIGN, 0, 4);
        }
    }
    if (distance == UINT32_MAX) {
        *ended = true;
        return decoder->written == decoder->size ||
               ab_fail(error, "LZMA end marker precedes declared output size");
    }
    for (size_t index = 3; index > 0; --index) {
        decoder->history[index] = decoder->history[index - 1];
    }
    decoder->history[0] = distance + 1;
    decoder->state = decoder->state < 7 ? 7 : 10;
    return copy_match(decoder, length, error);
}

static bool decode_stream(LzmaDecoder *decoder, AbError *error) {
    RangeDecoder *coder = &decoder->coder;
    if (coder->input.size < 5 || coder->input.data[0] != 0) {
        return ab_fail(error, "Invalid or truncated LZMA range-coder header");
    }
    coder->range = UINT32_MAX;
    coder->code = ab_u32be(coder->input.data + 1);
    coder->offset = 5;
    if (coder->code == UINT32_MAX) {
        return ab_fail(error, "Invalid LZMA initial range value");
    }
    bool ended = false;
    while (!ended) {
        if (decoder->written == decoder->size && coder->code == 0 &&
            coder->offset == coder->input.size) {
            return true;
        }
        unsigned position = (unsigned)(decoder->written &
                                       ((UINT32_C(1) << decoder->position_bits) - 1));
        uint16_t *probability = decoder->probability;
        if (!model_bit(coder, probability + MATCH + decoder->state * 16 + position)) {
            if (!literal_byte(decoder, error)) {
                return false;
            }
        } else if (model_bit(coder, probability + REP + decoder->state)) {
            if (!repeated_match(decoder, position, error)) {
                return false;
            }
        } else if (!new_match(decoder, position, &ended, error)) {
            return false;
        }
        if (coder->failed) {
            return ab_fail(error, "Truncated LZMA range-coded input");
        }
    }
    if (coder->code != 0 || coder->offset != coder->input.size) {
        return ab_fail(error, "LZMA range finalization mismatch or trailing input");
    }
    return true;
}

bool ab_lzma_raw(AbSpan input, AbSpan properties, uint64_t size, size_t limit,
                 AbBuffer *output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing LZMA output");
    }
    *output = (AbBuffer){0};
    if (!input.data || input.size < 5 || !properties.data || properties.size != 5) {
        return ab_fail(error, "Invalid raw LZMA input or properties");
    }
    unsigned property = properties.data[0];
    if (property >= 9 * 5 * 5) {
        return ab_fail(error, "Invalid LZMA property byte");
    }
    uint32_t dictionary = ab_u32le(properties.data + 1);
    if (size > limit || dictionary > limit) {
        return ab_fail(error, "LZMA output or dictionary exceeds supported size limit");
    }
    unsigned literal_bits = property % 9;
    unsigned literal_position_bits = property / 9 % 5;
    unsigned position_bits = property / 9 / 5;
    size_t probability_count =
        LITERAL + ((size_t)768 << (literal_bits + literal_position_bits));
    uint16_t *probability = malloc(probability_count * sizeof(*probability));
    uint8_t *bytes = malloc(size ? (size_t)size : 1);
    if (!probability || !bytes) {
        free(probability);
        free(bytes);
        return ab_fail(error, "Cannot allocate LZMA decoding buffers");
    }
    for (size_t index = 0; index < probability_count; ++index) {
        probability[index] = 1024;
    }
    LzmaDecoder decoder = {
        .coder = {.input = input},
        .probability = probability,
        .bytes = bytes,
        .size = (size_t)size,
        .history = {1, 1, 1, 1},
        .dictionary_size = dictionary < 4096 ? 4096 : dictionary,
        .literal_bits = literal_bits,
        .literal_position_bits = literal_position_bits,
        .position_bits = position_bits,
    };
    bool okay = decode_stream(&decoder, error);
    free(probability);
    if (!okay) {
        free(bytes);
        return false;
    }
    *output = (AbBuffer){bytes, (size_t)size};
    return true;
}

bool ab_lzma_asset(AbSpan input, size_t limit, AbBuffer *output, AbError *error) {
    static const uint8_t magic[9] = {0x89, 'L', 'Z', 'M', 'A', '\r', '\n', 0x1a, '\n'};
    if (!output) {
        return ab_fail(error, "Missing LZMA output");
    }
    *output = (AbBuffer){0};
    if (!input.data || input.size < 27 ||
        memcmp(input.data, magic, sizeof(magic)) != 0) {
        return ab_fail(error, "Unsupported or truncated target LZMA wrapper");
    }
    uint64_t size =
        (uint64_t)ab_u32le(input.data + 14) | (uint64_t)ab_u32le(input.data + 18) << 32;
    return ab_lzma_raw((AbSpan){input.data + 22, input.size - 22},
                       (AbSpan){input.data + 9, 5}, size, limit, output, error);
}
