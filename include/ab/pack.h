#ifndef AB_PACK_H
#define AB_PACK_H

#include "ab/base.h"

/* Target LZMA wrapper or a single-file 7z archive with one LZMA1 coder. */
bool ab_asset_unpack(AbSpan input, size_t limit, AbBuffer *output, AbError *error);

#endif
