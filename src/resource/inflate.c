#include "ab/inflate.h"

#include <string.h>

enum { MAX_BITS = 15, MAX_SYMBOLS = 288 };

typedef struct {
    AbSpan input;
    size_t offset;
    uint32_t pending;
    unsigned bit_count;
} BitReader;

typedef struct {
    uint16_t count[MAX_BITS + 1];
    uint16_t first_code[MAX_BITS + 1];
    uint16_t first_index[MAX_BITS + 1];
    uint16_t symbols[MAX_SYMBOLS];
} Huffman;

static bool read_bits(BitReader *reader, unsigned count, uint32_t *value,
                      AbError *error) {
    while (reader->bit_count < count) {
        if (reader->offset == reader->input.size) {
            return ab_fail(error, "Truncated DEFLATE bitstream");
        }
        reader->pending |= (uint32_t)reader->input.data[reader->offset++]
                           << reader->bit_count;
        reader->bit_count += 8;
    }
    *value = reader->pending & ((UINT32_C(1) << count) - 1);
    reader->pending >>= count;
    reader->bit_count -= count;
    return true;
}

static bool build_huffman(Huffman *tree, const uint8_t *lengths, size_t count,
                          bool allow_single, bool allow_empty, AbError *error) {
    *tree = (Huffman){0};
    unsigned used = 0;
    for (size_t index = 0; index < count; ++index) {
        if (lengths[index] > MAX_BITS) {
            return ab_fail(error, "DEFLATE code length exceeds 15 bits");
        }
        if (lengths[index]) {
            ++tree->count[lengths[index]];
            ++used;
        }
    }
    if (!used) {
        return allow_empty || ab_fail(error, "Empty DEFLATE Huffman tree");
    }
    int space = 1;
    unsigned code = 0;
    unsigned index = 0;
    for (unsigned length = 1; length <= MAX_BITS; ++length) {
        space = space * 2 - tree->count[length];
        if (space < 0) {
            return ab_fail(error, "Oversubscribed DEFLATE Huffman tree");
        }
        code = (code + tree->count[length - 1]) << 1;
        tree->first_code[length] = (uint16_t)code;
        tree->first_index[length] = (uint16_t)index;
        index += tree->count[length];
    }
    if (space && !(allow_single && used == 1 && tree->count[1] == 1)) {
        return ab_fail(error, "Incomplete DEFLATE Huffman tree");
    }
    uint16_t next[MAX_BITS + 1];
    memcpy(next, tree->first_index, sizeof(next));
    for (size_t symbol = 0; symbol < count; ++symbol) {
        unsigned length = lengths[symbol];
        if (length) {
            tree->symbols[next[length]++] = (uint16_t)symbol;
        }
    }
    return true;
}

static bool decode_symbol(BitReader *reader, const Huffman *tree, uint32_t *symbol,
                          AbError *error) {
    uint32_t code = 0;
    for (unsigned length = 1; length <= MAX_BITS; ++length) {
        uint32_t bit = 0;
        if (!read_bits(reader, 1, &bit, error)) {
            return false;
        }
        code = (code << 1) | bit;
        uint32_t first = tree->first_code[length];
        if (code >= first && code - first < tree->count[length]) {
            *symbol = tree->symbols[tree->first_index[length] + code - first];
            return true;
        }
    }
    return ab_fail(error, "Invalid DEFLATE Huffman code");
}

static bool fixed_trees(Huffman *literal, Huffman *distance, AbError *error) {
    uint8_t lengths[288];
    for (size_t symbol = 0; symbol < 288; ++symbol) {
        if (symbol < 144) {
            lengths[symbol] = 8;
        } else if (symbol < 256) {
            lengths[symbol] = 9;
        } else if (symbol < 280) {
            lengths[symbol] = 7;
        } else {
            lengths[symbol] = 8;
        }
    }
    uint8_t distances[32];
    memset(distances, 5, sizeof(distances));
    return build_huffman(literal, lengths, 288, false, false, error) &&
           build_huffman(distance, distances, 32, false, false, error);
}

static bool dynamic_trees(BitReader *reader, Huffman *literal, Huffman *distance,
                          AbError *error) {
    static const uint8_t order[19] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15,
    };
    uint32_t literal_count = 0;
    uint32_t distance_count = 0;
    uint32_t code_count = 0;
    if (!read_bits(reader, 5, &literal_count, error) ||
        !read_bits(reader, 5, &distance_count, error) ||
        !read_bits(reader, 4, &code_count, error)) {
        return false;
    }
    literal_count += 257;
    distance_count += 1;
    code_count += 4;
    if (literal_count > 286) {
        return ab_fail(error, "Reserved DEFLATE literal count");
    }
    uint8_t code_lengths[19] = {0};
    for (uint32_t index = 0; index < code_count; ++index) {
        uint32_t length = 0;
        if (!read_bits(reader, 3, &length, error)) {
            return false;
        }
        code_lengths[order[index]] = (uint8_t)length;
    }
    Huffman codes;
    if (!build_huffman(&codes, code_lengths, 19, false, false, error)) {
        return false;
    }
    uint8_t lengths[286 + 32] = {0};
    size_t total = literal_count + distance_count;
    size_t index = 0;
    while (index < total) {
        uint32_t symbol = 0;
        if (!decode_symbol(reader, &codes, &symbol, error)) {
            return false;
        }
        if (symbol <= 15) {
            lengths[index++] = (uint8_t)symbol;
            continue;
        }
        if (symbol == 16 && index == 0) {
            return ab_fail(error, "DEFLATE repeat has no previous code length");
        }
        unsigned extra_bits = symbol == 16 ? 2 : (symbol == 17 ? 3 : 7);
        uint32_t repeat = 0;
        if (!read_bits(reader, extra_bits, &repeat, error)) {
            return false;
        }
        repeat += symbol == 18 ? 11 : 3;
        if (repeat > total - index) {
            return ab_fail(error, "DEFLATE code length repeat exceeds tree");
        }
        uint8_t value = symbol == 16 ? lengths[index - 1] : 0;
        memset(lengths + index, value, repeat);
        index += repeat;
    }
    if (!lengths[256]) {
        return ab_fail(error, "DEFLATE tree has no end-of-block code");
    }
    return build_huffman(literal, lengths, literal_count, true, false, error) &&
           build_huffman(distance, lengths + literal_count, distance_count, true, true,
                         error);
}

/* Length and distance mappings are format data from RFC 1951, section 3.2.5. */
static bool compressed_block(BitReader *reader, const Huffman *literal,
                             const Huffman *distance, uint8_t *output, size_t capacity,
                             size_t *written, AbError *error) {
    static const uint16_t length_base[29] = {
        3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23,  27,
        31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258,
    };
    static const uint8_t length_bits[29] = {
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
        2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0,
    };
    static const uint16_t distance_base[30] = {
        1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
        33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
        1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577,
    };
    static const uint8_t distance_bits[30] = {
        0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
        6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13,
    };
    for (;;) {
        uint32_t symbol = 0;
        if (!decode_symbol(reader, literal, &symbol, error)) {
            return false;
        }
        if (symbol < 256) {
            if (*written == capacity) {
                return ab_fail(error, "DEFLATE literal exceeds output size");
            }
            output[(*written)++] = (uint8_t)symbol;
            continue;
        }
        if (symbol == 256) {
            return true;
        }
        if (symbol > 285) {
            return ab_fail(error, "Reserved DEFLATE length symbol");
        }
        uint32_t extra = 0;
        size_t length_index = symbol - 257;
        if (!read_bits(reader, length_bits[length_index], &extra, error)) {
            return false;
        }
        size_t length = length_base[length_index] + extra;
        if (!decode_symbol(reader, distance, &symbol, error)) {
            return false;
        }
        if (symbol >= 30) {
            return ab_fail(error, "Reserved DEFLATE distance symbol");
        }
        if (!read_bits(reader, distance_bits[symbol], &extra, error)) {
            return false;
        }
        size_t back = distance_base[symbol] + extra;
        if (back > *written || length > capacity - *written) {
            return ab_fail(error, "DEFLATE match exceeds history or output size");
        }
        /* Forward copying intentionally permits overlapping LZ77 matches. */
        for (size_t index = 0; index < length; ++index) {
            output[*written] = output[*written - back];
            ++*written;
        }
    }
}

bool ab_inflate_raw(AbSpan input, uint8_t *output, size_t output_size, AbError *error) {
    if ((!input.data && input.size) || (!output && output_size)) {
        return ab_fail(error, "Invalid DEFLATE buffers");
    }
    BitReader reader = {.input = input};
    size_t written = 0;
    uint32_t final = 0;
    do {
        uint32_t type = 0;
        if (!read_bits(&reader, 1, &final, error) ||
            !read_bits(&reader, 2, &type, error)) {
            return false;
        }
        if (type == 0) {
            reader.pending = 0;
            reader.bit_count = 0;
            uint32_t length = 0;
            uint32_t complement = 0;
            if (!read_bits(&reader, 16, &length, error) ||
                !read_bits(&reader, 16, &complement, error)) {
                return false;
            }
            if ((length ^ complement) != 65535 ||
                !ab_range(input.size, reader.offset, length) ||
                length > output_size - written) {
                return ab_fail(error, "Invalid DEFLATE stored-block bounds");
            }
            if (length) {
                memcpy(output + written, input.data + reader.offset, length);
            }
            reader.offset += length;
            written += length;
        } else if (type == 1 || type == 2) {
            Huffman literal;
            Huffman distance;
            bool okay = type == 1 ? fixed_trees(&literal, &distance, error)
                                  : dynamic_trees(&reader, &literal, &distance, error);
            if (!okay || !compressed_block(&reader, &literal, &distance, output,
                                           output_size, &written, error)) {
                return false;
            }
        } else {
            return ab_fail(error, "Reserved DEFLATE block type");
        }
    } while (!final);
    if (written != output_size || reader.offset != input.size) {
        return ab_fail(error, "DEFLATE output size mismatch or trailing input");
    }
    return true;
}
