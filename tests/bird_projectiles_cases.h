#ifndef AB_TEST_BIRD_PROJECTILES_CASES_H
#define AB_TEST_BIRD_PROJECTILES_CASES_H

#include <stdint.h>

/* Second sensor, controllable1/2, shot1/2, specialty1/2, definition mask, list
 * presence/count, host mutations, old boomerang/collision/explode values and
 * eight initial entry identities. Controller/flag values are
 * nil/false/true/zero/string/table; specialty adds BOOMERANG/OTHER/table. */
static void bird_projectiles_case(uint32_t sample, uint32_t output[25]) {
    output[0] = sample;
    for (uint32_t field = 1; field < 17; ++field) {
        output[field] = ((sample * (field * 2u + 1u)) / (field + 1u)) % 6u;
    }
    output[6] = ((sample * 13u) / 7u) % 7u;
    output[7] = ((sample * 17u) / 9u) % 7u;
    output[8] = (sample / 5u) & 3u;
    output[9] = sample % 13u != 0;
    output[10] = sample % 9u;
    output[11] = (sample * 73u + sample / 7u) & 255u;
    for (uint32_t slot = 0; slot < 8; ++slot) {
        output[17 + slot] = ((sample * (slot * 2u + 1u)) / (slot + 1u)) % 10u;
    }
    if (sample < 216u) {
        output[1] = (sample / 36u) % 6u;
        output[2] = sample % 6u;
        output[3] = (sample / 6u) % 6u;
        output[4] = output[5] = 2;
        output[6] = output[7] = 4;
        output[8] = 3;
        output[9] = 1;
        output[10] = 8;
        output[11] = 0;
    }
}

static uint32_t bird_projectile_name(uint32_t sample, uint32_t object) {
    return object < 2 ? object + 1u : (sample + object / 2u) % 3u + 1u;
}

#endif
