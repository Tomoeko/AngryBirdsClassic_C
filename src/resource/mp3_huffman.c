#include "ab/mp3.h"

#include "mp3_huffman_data.h"

#include <string.h>

static bool valid_bits(const AbMp3HuffmanBits *bits, AbError *error) {
    if (!bits || bits->input.size > 1024 || (bits->input.size && !bits->input.data) ||
        bits->limit > bits->input.size * 8 || bits->position > bits->limit) {
        return ab_fail(error, "Invalid MP3 Huffman bit window");
    }
    return true;
}

static bool take_bits(AbMp3HuffmanBits *bits, unsigned count, uint32_t *output,
                      AbError *error) {
    if (count > bits->limit - bits->position) {
        return ab_fail(error, "Truncated MP3 Huffman code or magnitude/sign");
    }
    uint32_t value = 0;
    for (unsigned index = 0; index < count; ++index) {
        size_t position = bits->position++;
        unsigned shift = 7u - (unsigned)(position % 8);
        value = value * 2 + ((uint32_t)bits->input.data[position / 8] >> shift & 1u);
    }
    *output = value;
    return true;
}

static bool decode_symbol(AbMp3HuffmanBits *bits, const AbMp3HuffmanBook *book,
                          uint32_t *output, AbError *error) {
    if (!book->count) {
        *output = 0;
        return true;
    }
    uint16_t node = 0;
    for (unsigned depth = 0; depth < book->depth; ++depth) {
        uint32_t bit = 0;
        if (node >= book->count) {
            return ab_fail(error, "Invalid compiled MP3 Huffman branch");
        }
        if (!take_bits(bits, 1, &bit, error)) {
            return false;
        }
        node = book->nodes[node].child[bit];
        if (node & UINT16_C(0x8000)) {
            *output = node & 255u;
            return true;
        }
    }
    return ab_fail(error, "Compiled MP3 Huffman depth exceeded");
}

static bool decode_signed(AbMp3HuffmanBits *bits, uint32_t value, unsigned linbits,
                          int16_t *output, AbError *error) {
    if (value == 15 && linbits) {
        uint32_t extension = 0;
        if (!take_bits(bits, linbits, &extension, error)) {
            return false;
        }
        value += extension;
    }
    uint32_t negative = 0;
    if (value && !take_bits(bits, 1, &negative, error)) {
        return false;
    }
    *output = (int16_t)(negative ? -(int32_t)value : (int32_t)value);
    return true;
}

bool ab_mp3_huffman_pair(AbMp3HuffmanBits *bits, uint32_t table, int16_t output[2],
                         AbError *error) {
    if (!valid_bits(bits, error)) {
        return false;
    }
    if (table >= 32 || !output) {
        return ab_fail(error, "Invalid MP3 Huffman pair table or output");
    }
    AbMp3HuffmanBits next = *bits;
    const AbMp3HuffmanBook *book = ab_mp3_huffman_pairs + table;
    uint32_t symbol = 0;
    int16_t values[2];
    if (!decode_symbol(&next, book, &symbol, error) ||
        !decode_signed(&next, symbol >> 4, book->linbits, values, error) ||
        !decode_signed(&next, symbol & 15u, book->linbits, values + 1, error)) {
        return false;
    }
    memcpy(output, values, sizeof(values));
    *bits = next;
    return true;
}

bool ab_mp3_huffman_quad(AbMp3HuffmanBits *bits, bool second, int16_t output[4],
                         AbError *error) {
    if (!valid_bits(bits, error)) {
        return false;
    }
    if (!output) {
        return ab_fail(error, "Missing MP3 Huffman quad output");
    }
    AbMp3HuffmanBits next = *bits;
    uint32_t symbol = 0;
    int16_t values[4];
    if (!decode_symbol(&next, ab_mp3_huffman_quads + (second ? 1 : 0), &symbol,
                       error)) {
        return false;
    }
    for (size_t index = 0; index < 4; ++index) {
        if (!decode_signed(&next, symbol >> (3u - (unsigned)index) & 1u, 0,
                           values + index, error)) {
            return false;
        }
    }
    memcpy(output, values, sizeof(values));
    *bits = next;
    return true;
}

size_t ab_mp3_huffman_count1_tail(AbMp3HuffmanBits *bits, bool second,
                                  int16_t output[4]) {
    uint32_t symbol = 0;
    if (!decode_symbol(bits, ab_mp3_huffman_quads + (second ? 1 : 0), &symbol, NULL) ||
        bits->position == bits->limit) {
        bits->position = bits->limit;
        return 0;
    }
    for (size_t index = 0; index < 4; ++index) {
        uint32_t value = symbol >> (3u - (unsigned)index) & 1u;
        if (value && bits->position == bits->limit) {
            return index;
        }
        if (!decode_signed(bits, value, 0, output + index, NULL)) {
            return index;
        }
    }
    return 4;
}
