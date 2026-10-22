#ifndef AB_VP8_TABLES_H
#define AB_VP8_TABLES_H

#include <stdint.h>

enum { AB_VP8_COEFFICIENT_PROBABILITIES = 4 * 8 * 3 * 11 };

extern const uint8_t ab_vp8_coefficient_defaults[AB_VP8_COEFFICIENT_PROBABILITIES];
extern const uint8_t ab_vp8_coefficient_updates[AB_VP8_COEFFICIENT_PROBABILITIES];
extern const uint8_t ab_vp8_quantizer_dc[128];
extern const uint16_t ab_vp8_quantizer_ac[128];
extern const uint8_t ab_vp8_intra4_probabilities[10][10][9];

#endif
