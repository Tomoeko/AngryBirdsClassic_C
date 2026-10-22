#ifndef AB_TEST_GAME_JOINT_PARAMETER_CASES_H
#define AB_TEST_GAME_JOINT_PARAMETER_CASES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Lua type tags and numeric/Boolean payloads for motor, speed, maximum,
 * limit, lower, upper, frequency and damping. Repeat assignments, including
 * opposite zero signs, after reseeding retained impulses and body sleep state.
 * Non-numeric strings and numeric/Boolean type mismatches exercise ignored keys.
 */
static void game_joint_parameter_case(uint32_t sample, uint32_t pass, uint32_t types[8],
                                      uint32_t values[8]) {
    uint32_t random = UINT32_C(0x52a7139b) ^ sample * UINT32_C(0x9e3779b9);
    for (size_t field = 0; field < 8; ++field) {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        bool boolean = field == 0 || field == 3;
        uint32_t mode = (random >> 16) % 5;
        if (sample < 128) {
            mode = field == (sample / 4) % 8 ? 1u : 0u;
        } else if (sample < 256) {
            mode = 1;
        }
        types[field] = mode == 0   ? 0
                       : mode == 2 ? (boolean ? 3u : 1u)
                       : mode == 3 ? 4u
                                   : (boolean ? 1u : 3u);
        if (boolean) {
            values[field] =
                types[field] == 3 ? UINT32_C(0x3f800000) : (sample >> (field + 2)) & 1u;
        } else {
            union {
                float number;
                uint32_t bits;
            } value = {.number = (float)((int32_t)(random & 63u) - 32) * 0.125f};
            values[field] = field == 2 ? value.bits & UINT32_C(0x7fffffff) : value.bits;
            if ((sample & 64u) && (field == 4 || field == 5)) {
                values[field] = pass ? UINT32_C(0x80000000) : 0;
            }
            if (types[field] == 1) {
                values[field] = sample & 1u;
            }
        }
    }
}

#endif
