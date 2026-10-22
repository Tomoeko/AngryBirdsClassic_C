#ifndef AB_TEST_SPRITE_REGISTRY_CASES_H
#define AB_TEST_SPRITE_REGISTRY_CASES_H

#include <stddef.h>
#include <stdint.h>

static const char *const registry_names[] = {"", "bird", "bird-long", "Bird", "pig"};
static const char *const registry_owners[] = {"", "base", "theme", "base/alternate"};

static uint32_t registry_random(uint32_t *state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state;
}

/* Four source sets, three resource names each, followed by eight commands.
 * Command low two bits select a set, bit two requests removal; the owner label
 * is independent of the source identity. Duplicate resource names in a set use
 * the native source map's last definition. */
static void registry_case(size_t index, uint32_t input[24]) {
    uint32_t state = UINT32_C(0x5205a71e) ^ (uint32_t)index;
    for (size_t set = 0; set < 4; ++set) {
        input[set] = index % 17 == 0 ? 0 : registry_random(&state) % 8;
        for (size_t resource = 0; resource < 3; ++resource) {
            input[4 + set * 3 + resource] = registry_random(&state) % 5;
        }
    }
    for (size_t command = 0; command < 8; ++command) {
        input[16 + command] = registry_random(&state) >> 24;
    }
    if (index % 4 == 0) {
        input[16] = 0;
        input[17] = 8;
        input[18] = 2;
        input[19] = 0;
        input[20] = 4;
        input[21] = 6;
    }
}

#endif
