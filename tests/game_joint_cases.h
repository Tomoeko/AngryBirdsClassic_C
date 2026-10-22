#ifndef AB_TEST_GAME_JOINT_CASES_H
#define AB_TEST_GAME_JOINT_CASES_H

#include <stddef.h>
#include <stdint.h>

/* Sample/type/optional-field mask, coordinate type, two body poses, two
 * anchors, frequency/damping/speed/torque/limits/axis/break force, Boolean
 * flags, and the four legacy coordinate-type-1 positional arguments. */
static void game_joint_case(uint32_t sample, uint32_t input[28]) {
    uint32_t random = UINT32_C(0x520b17d3) ^ sample * UINT32_C(0x9e3779b9);
    input[0] = sample;
    input[1] = 1 + sample % 4;
    input[2] = sample < 64    ? 0
               : sample < 128 ? UINT32_C(0x7fff)
                              : (sample * UINT32_C(0x7f4a7c15)) & UINT32_C(0x7fff);
    for (size_t field = 3; field < 28; ++field) {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        union {
            float number;
            uint32_t bits;
        } value = {.number = (float)((int32_t)(random & 1023u) - 512) * 0x1p-7f};
        input[field] = value.bits;
    }
    union {
        float number;
        uint32_t bits;
    } coordinate = {.number = (float)((sample / 4) % 3)};
    input[3] = coordinate.bits;
    input[17] &= UINT32_C(0x7fffffff);
    input[23] = (sample / 8) & 31u;
    if (sample < 32) {
        input[6] = input[9] = 0;
    }
    if (sample < 128) {
        const uint32_t boundaries[] = {0,
                                       UINT32_C(0x80000000),
                                       1,
                                       UINT32_C(0x80000001),
                                       UINT32_C(0x007fffff),
                                       UINT32_C(0x00800000),
                                       UINT32_C(0x3d4ccccd),
                                       UINT32_C(0xbd4ccccd)};
        for (size_t field = 4; field < 14; ++field) {
            input[field] = boundaries[(sample / 4 + field) % 8];
        }
        input[6] = sample & 4u ? UINT32_C(0x80000000) : 0;
        input[9] = sample & 8u ? UINT32_C(0x80000000) : 0;
        input[20] = sample & 16u ? UINT32_C(0x80000000) : 0;
        input[21] = sample & 32u ? UINT32_C(0x80000000) : 0;
    }
}

#endif
