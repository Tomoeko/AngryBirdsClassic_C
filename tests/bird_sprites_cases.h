#ifndef AB_TEST_BIRD_SPRITES_CASES_H
#define AB_TEST_BIRD_SPRITES_CASES_H

#include <stdint.h>

/* Sample; shot1/2, sensor2, dontUpdate1/2, frozen2, specialty availability;
 * flying identity; specialty1/2; spriteCollision1/2; definition presence mask;
 * host mutation mask; unused sensor1; old hasCollided1/2. */
static void bird_sprites_case(uint32_t sample, uint32_t output[18]) {
    output[0] = sample;
    for (uint32_t field = 1; field < 18; ++field) {
        output[field] = ((sample * (field * 2u + 1u)) / (field + 1u)) % 6u;
    }
    output[8] = (sample / 5u) % 4u;
    output[9] = ((sample * 21u) / 10u) % 7u;
    output[10] = ((sample * 23u) / 11u) % 7u;
    output[13] = (sample / 7u) & 3u;
    output[14] = (sample * 73u + sample / 3u) & 4095u;
    if (sample < 1024u) {
        output[1] = sample % 6u;
        output[2] = (sample / 6u) % 6u;
        output[3] = 0;
        output[4] = output[5] = 0;
        output[6] = (sample / 36u) % 6u;
        output[9] = output[10] = (sample / 216u) % 7u;
        output[11] = 4;
        output[12] = 4;
        output[13] = 3;
    }
}

#endif
