#ifndef AB_TEST_ASSET_BUNDLE_CASES_H
#define AB_TEST_ASSET_BUNDLE_CASES_H

#include <stdint.h>

static const char *const bundle_names[] = {"alpha", "beta", "gamma", "delta", ""};

static uint32_t bundle_random(uint32_t *state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state >> 16;
}

/* Loading flag, bundle membership mask, requested index mask, five requested
 * names, five bundle index masks, and three members of each bundle. */
static void asset_bundle_case(uint32_t index, uint32_t input[28]) {
    uint32_t state = UINT32_C(0x520b7d1e) ^ index;
    input[0] = index & 1u;
    input[1] = bundle_random(&state) & 31u;
    input[2] = index % 4 ? bundle_random(&state) & 31u : 31u;
    for (uint32_t slot = 0; slot < 5; ++slot) {
        input[3 + slot] = bundle_random(&state) % 5;
        input[8 + slot] = bundle_random(&state) & 7u;
        for (uint32_t member = 0; member < 3; ++member) {
            input[13 + slot * 3 + member] = bundle_random(&state) % 5;
        }
    }
    if (index % 8 == 0) {
        input[1] = 31;
        input[3] = 0;
        input[8] = 7;
        input[13] = input[14] = 0;
        input[15] = 1;
    }
}

#endif
