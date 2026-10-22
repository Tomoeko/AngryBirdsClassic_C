#ifndef AB_TEST_GAME_CONTACT_ROUTE_CASES_H
#define AB_TEST_GAME_CONTACT_ROUTE_CASES_H

#include <stddef.h>
#include <stdint.h>

/* Three ordered 12-by-12 handler tables, then stage/first/second/new token.
 * Token six stands for an opaque caller handler; no fixture invokes it. */
static void game_contact_route_case(uint32_t sample, uint32_t output[436]) {
    uint32_t random = UINT32_C(0x520c17a5) ^ sample * UINT32_C(0x9e3779b9);
    for (size_t index = 0; index < 432; ++index) {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        output[index] = sample ? random % 7u : 0;
    }
    output[432] = (sample / 144u) % 3u;
    output[433] = sample % 12u;
    output[434] = (sample / 12u) % 12u;
    output[435] = sample % 7u;
}

#endif
