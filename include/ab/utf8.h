#ifndef AB_UTF8_H
#define AB_UTF8_H

#include "ab/base.h"

/* Target UTFConverter mode 2, including its permissive legacy behavior.
 * ASCII consumes one byte. C0..F7 prefixes consume two through four bytes
 * when available, masking subsequent bytes without validating continuation
 * bits, overlong encodings, surrogates or Unicode range. Other/truncated
 * prefixes fail, preserve code, and consume one byte (zero for empty input).
 * Both output pointers are required. Invalid storage preserves both outputs. */
bool ab_utf8_decode(AbSpan input, size_t *consumed, uint32_t *code);

/* Target toUTF32string policy: append decoded values and skip one byte on
 * failure, without inserting a replacement character. No allocation. On
 * insufficient capacity, output may be partial and output_count is unchanged.
 * A zero capacity permits NULL output. Input/output storage must be disjoint. */
bool ab_utf8_to_codes(AbSpan input, uint32_t *output, size_t capacity,
                      size_t *output_count);

/* Target mode-2 encoding, including its signed int32 input behavior. Values
 * 0..0x1fffff use one through four bytes without excluding surrogates or values
 * above Unicode's range. Values 0x200000..0x7fffffff fail. Values with the high
 * bit set follow the original signed comparison and emit their low byte.
 * Insufficient capacity or rejected code sets written to zero, leaves bytes
 * unchanged and returns false. Invalid pointer storage preserves written.
 * A zero capacity permits NULL bytes. This is target compatibility behavior. */
bool ab_utf8_encode(uint32_t code, uint8_t *bytes, size_t capacity, size_t *written);

/* Original toUTF8string policy: encode each value and skip rejected values.
 * Caller-owned disjoint buffers; no allocation. On insufficient capacity,
 * bytes may be partial and output_count is unchanged. */
bool ab_utf8_from_codes(const uint32_t *codes, size_t count, uint8_t *output,
                        size_t capacity, size_t *output_count);

#endif
