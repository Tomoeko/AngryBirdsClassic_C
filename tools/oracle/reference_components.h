#ifndef AB_ORACLE_REFERENCE_COMPONENTS_H
#define AB_ORACLE_REFERENCE_COMPONENTS_H

#include <stddef.h>
#include <stdint.h>

static uint32_t component_random(uint32_t *state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return (*state & UINT32_C(0x807fffff)) | ((*state >> 23) % 255) << 23;
}

static void next_reference_components(uint32_t *state, size_t index,
                                      uint32_t output[2]) {
    static const uint32_t boundaries[] = {0x00800000, 0x39800000, 0x3ee00000,
                                          0x3f300000, 0x3f980000, 0x401c0000,
                                          0x4c800000, 0x7f7ffffd};
    output[0] = component_random(state);
    output[1] = component_random(state);
    if (index < 16) {
        output[0] = (index & 4 ? UINT32_C(0x3f800000) : 0) |
                    (index & 1 ? UINT32_C(0x80000000) : 0);
        output[1] = (index & 8 ? UINT32_C(0x3f800000) : 0) |
                    (index & 2 ? UINT32_C(0x80000000) : 0);
    } else if (index < 176) {
        size_t item = index - 16;
        output[0] = (boundaries[item / 20] - 2 + (uint32_t)(item % 5)) |
                    (item & 1 ? UINT32_C(0x80000000) : 0);
        output[1] = UINT32_C(0x3f800000) | (item & 2 ? UINT32_C(0x80000000) : 0);
    }
}

#endif
