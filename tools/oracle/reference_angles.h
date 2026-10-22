#ifndef AB_ORACLE_REFERENCE_ANGLES_H
#define AB_ORACLE_REFERENCE_ANGLES_H

#include <stddef.h>
#include <stdint.h>

static inline uint32_t next_reference_angle(uint32_t *state, size_t index) {
    const uint32_t special[] = {0,          0x80000000, 1,          0x80000001,
                                0x3fc90fdb, 0xbfc90fdb, 0x40490fdb, 0xc0490fdb};
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    uint32_t exponent = 115 + ((*state >> 23) % 22);
    return index < sizeof(special) / sizeof(special[0])
               ? special[index]
               : (*state & UINT32_C(0x807fffff)) | exponent << 23;
}

static inline uint32_t next_extended_angle(uint32_t *state, size_t index) {
    static const uint32_t boundaries[] = {0x39800000, 0x3f490fda, 0x4016cbe3,
                                          0x407b53d1, 0x40afeddf, 0x40e231d5,
                                          0x4dc90fda};
    uint32_t bits = next_reference_angle(state, index);
    if (index < 8) {
        return bits;
    }
    uint32_t exponent = (*state >> 23) % 156;
    if (index % 4 == 1) {
        exponent = 115 + exponent % 41;
    } else if (index % 4 == 3) {
        exponent = 134 + exponent % 22;
    }
    uint32_t magnitude = (*state & UINT32_C(0x007fffff)) | exponent << 23;
    if (index % 4 == 2) {
        size_t boundary = (index / 4) % (sizeof(boundaries) / sizeof(boundaries[0]));
        magnitude = boundaries[boundary] - 2 + (uint32_t)((index / 28) % 5);
    }
    if (magnitude > UINT32_C(0x4dc90fda)) {
        magnitude = UINT32_C(0x4dc90fda) - (*state & 255);
    }
    return (*state & UINT32_C(0x80000000)) | magnitude;
}

#endif
