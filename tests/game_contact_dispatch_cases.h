#ifndef AB_TEST_GAME_CONTACT_DISPATCH_CASES_H
#define AB_TEST_GAME_CONTACT_DISPATCH_CASES_H

#include <stdint.h>

/* Stage, ordered collider pair, fixture/disable/handler/previous-pointer flags,
 * and a caller contact identity. The identity is opaque to the dispatcher. */
static void game_contact_dispatch_case(uint32_t sample, uint32_t output[5]) {
    uint32_t random = UINT32_C(0x520d15c7) ^ sample * UINT32_C(0x9e3779b9);
    random ^= random << 13;
    random ^= random >> 17;
    random ^= random << 5;
    output[0] = sample % 3u;
    output[1] = random % 12u;
    output[2] = (random / 12u) % 12u;
    output[3] = sample < 1024 ? sample & 31u : (random / 144u) & 31u;
    if (sample < 432) {
        output[1] = (sample / 3u) % 12u;
        output[2] = (sample / 36u) % 12u;
        output[3] = 11u | (sample & 1u ? 16u : 0u);
    }
    output[4] = UINT32_C(0x52000000) + sample * 4u;
}

#endif
