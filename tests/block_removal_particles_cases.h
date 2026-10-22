#ifndef AB_TEST_BLOCK_REMOVAL_PARTICLES_CASES_H
#define AB_TEST_BLOCK_REMOVAL_PARTICLES_CASES_H

#include <stdint.h>

static void block_removal_particles_case(uint32_t sample, uint32_t output[17]) {
    output[0] = sample;
    for (uint32_t field = 1; field < 17; ++field) {
        output[field] = (sample * (field * 2u + 1u) + sample / (field + 1u)) % 6u;
    }
    for (uint32_t list = 0; list < 2; ++list) {
        output[3 + list] = 0;
        for (uint32_t slot = 0; slot < 4; ++slot) {
            uint32_t kind =
                (sample / (slot + 1u) + sample * (list * 2u + 3u) + slot) % 6u;
            output[3 + list] |= kind << (slot * 3u);
        }
    }
    output[10] = (sample / 19u) & 15u;
    output[11] = (sample * 13u + sample / 7u) & 127u;
    output[12] = (sample * 17u + sample / 11u) & 127u;
    output[13] = (sample * 23u + sample / 13u) & 31u;
    output[14] = (sample * 29u + sample / 17u) & 15u;
    output[15] = sample % 2u + 1u;
    output[16] = (sample / 3u) % 2u + 1u;
    if (sample < 2048) {
        output[10] = 15;
    }
    if (sample < 512) {
        output[2] = sample % 2u ? 3u : 4u;
        output[3] = 3u | (4u << 3u);
        output[4] = 5u | (3u << 3u);
        output[11] = output[12] = output[13] = output[14] = 0;
    }
    if (sample >= 512 && sample < 640) {
        output[1] = sample % 2u;
        output[2] = 3;
        output[3] = 3u | (4u << 3u) | (5u << 6u) | (3u << 9u);
        output[5] = sample % 6u;
        output[8] = 5;
        output[11] = output[12] = output[13] = output[14] = 0;
    }
    if (sample >= 640 && sample < 768) {
        output[1] = 0;
        output[2] = 3;
        output[3] = 3;
        output[5] = 2;
        output[8] = 5;
        output[11] = output[12] = 0;
        output[13] = sample % 2u;
        output[14] = (sample / 2u) % 16u;
    }
}

#endif
