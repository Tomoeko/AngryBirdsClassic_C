#ifndef AB_REFERENCE_WORLD_ITERATION_H
#define AB_REFERENCE_WORLD_ITERATION_H

#include <stdint.h>

enum { WI_PROFILES = 24, WI_KEYS = 24, WI_MAX_STEPS = 64, WI_RECORD_WORDS = 6 };

static const char *const wi_keys[WI_KEYS] = {
    "world_00", "world_01", "world_02", "world_03", "world_04", "world_05",
    "world_06", "world_07", "world_08", "world_09", "world_10", "world_11",
    "world_12", "world_13", "world_14", "world_15", "world_16", "world_17",
    "world_18", "world_19", "world_20", "world_21", "world_22", "world_23"};

typedef struct {
    uint32_t kind, key, value;
} WiAction;

static unsigned wi_initial_count(unsigned profile) {
    return profile % 5 + 2;
}

static unsigned wi_initial_key(unsigned profile, unsigned order) {
    return (profile + order * 3) % 8;
}

static unsigned wi_hint(unsigned profile) {
    return profile < 8 ? 0 : 1u << (profile % 4);
}

/* Controlled mutations only, without expected order/status/results. Both
 * backends retain the value returned by next before applying these actions. */
static unsigned wi_actions(unsigned profile, unsigned step, unsigned current,
                           WiAction output[16]) {
    if (step)
        return 0;
    unsigned count = 0;
    switch (profile % 8) {
        case 0:
            break;
        case 1:
            output[count++] = (WiAction){0, current, 0};
            break;
        case 2:
        case 3:
            if (profile % 8 == 3)
                output[count++] = (WiAction){0, current, 0};
            for (unsigned key = 8; key < 16; ++key)
                output[count++] = (WiAction){0, key, key + 1};
            break;
        case 4:
            output[count++] = (WiAction){0, current, 100 + profile};
            break;
        case 5:
            for (unsigned order = 0; order < wi_initial_count(profile); ++order) {
                unsigned key = wi_initial_key(profile, order);
                if (key != current) {
                    output[count++] = (WiAction){0, key, 0};
                    break;
                }
            }
            output[count++] = (WiAction){0, 16, 17};
            break;
        case 6:
            for (unsigned order = 0; order < wi_initial_count(profile); ++order)
                output[count++] = (WiAction){0, wi_initial_key(profile, order), 0};
            for (unsigned key = 8; key < 12; ++key)
                output[count++] = (WiAction){0, key, key + 1};
            break;
        case 7:
            output[count++] = (WiAction){0, current, 0};
            output[count++] = (WiAction){1, 0, 0};
            break;
    }
    return count;
}

#endif
