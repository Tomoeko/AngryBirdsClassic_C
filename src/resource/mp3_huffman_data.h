#ifndef AB_MP3_HUFFMAN_DATA_H
#define AB_MP3_HUFFMAN_DATA_H

#include "ab/mp3.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t child[2];
} AbMp3HuffmanNode;

typedef struct {
    const AbMp3HuffmanNode *nodes;
    uint16_t count;
    uint8_t depth, linbits;
} AbMp3HuffmanBook;

extern const AbMp3HuffmanBook ab_mp3_huffman_pairs[32];
extern const AbMp3HuffmanBook ab_mp3_huffman_quads[2];

/* Internal granule tail reader: emit only the count1 prefix whose signs fit.
 * An incomplete tree prefix consumes the remaining budget without emission. */
size_t ab_mp3_huffman_count1_tail(AbMp3HuffmanBits *bits, bool second,
                                  int16_t output[4]);

#endif
