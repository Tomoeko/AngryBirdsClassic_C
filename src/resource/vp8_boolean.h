#ifndef AB_VP8_BOOLEAN_H
#define AB_VP8_BOOLEAN_H

#include "ab/base.h"

typedef struct {
    AbSpan bytes;
    size_t offset;
    uint32_t range, value;
    unsigned shifted;
} AbVp8Boolean;

/* Checked VP8 arithmetic-field reader. Borrow a valid encoded interval with at
 * least two supplied bytes; the initial code value must be below range 255.
 * Probability is the zero-symbol weight in units of 1/256. Fields use uniform
 * probability and are assembled most-significant bit first. Signed fields read
 * a following sign bit even for zero magnitude/width. Counts are 0..24.
 * No implicit input padding: truncation preserves reader and output. This
 * boundary does not reproduce the native reader's padding/EOF state layout. */
bool ab_vp8_boolean_init(AbSpan bytes, AbVp8Boolean *output, AbError *error);
bool ab_vp8_boolean_bit(AbVp8Boolean *reader, uint8_t probability, uint32_t *output,
                        AbError *error);
bool ab_vp8_boolean_value(AbVp8Boolean *reader, unsigned count, uint32_t *output,
                          AbError *error);
bool ab_vp8_boolean_signed(AbVp8Boolean *reader, unsigned count, int32_t *output,
                           AbError *error);

#endif
