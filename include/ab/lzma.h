#ifndef AB_LZMA_H
#define AB_LZMA_H

#include "ab/base.h"

/* The APK's nine-byte magic followed by a known-size LZMA-alone stream. */
bool ab_lzma_asset(AbSpan input, size_t limit, AbBuffer *output, AbError *error);

/* Five-byte property block plus a raw LZMA1 stream with a known decoded size. */
bool ab_lzma_raw(AbSpan input, AbSpan properties, uint64_t decoded_size, size_t limit,
                 AbBuffer *output, AbError *error);

#endif
