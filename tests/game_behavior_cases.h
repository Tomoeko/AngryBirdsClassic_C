#ifndef AB_TEST_GAME_BEHAVIOR_CASES_H
#define AB_TEST_GAME_BEHAVIOR_CASES_H

#include <stdint.h>

static const char *const behavior_group_names[] = {"movement", "animation", "collision",
                                                   "specialty"};
static const char *const behavior_event_names[] = {
    "onBirdCollision", "onBlockCollision", "onDestroy", "customEvent"};

/* Sample, hash hint, group count, insertion rotation/direction, first-callback
 * action, event, argument count; 20 method kinds; 16 group slots; eight typed
 * arguments with payloads. Nil gaps in groups stop ipairs, including later
 * non-nil entries. Method kinds distinguish nil/false/two callables/zero/table. */
static void game_behavior_case(uint32_t sample, uint32_t output[60]) {
    output[0] = sample;
    static const uint32_t hints[] = {0, 1, 3, 4};
    output[1] = hints[(sample / 5u) % 4u];
    output[2] = sample % 5u;
    output[3] = (sample / 20u) % 4u;
    output[4] = (sample / 80u) & 1u;
    output[5] = (sample / 3u) % 9u;
    output[6] = (sample / 7u) % 4u;
    output[7] = (sample / 11u) % 9u;
    for (uint32_t id = 0; id < 20; ++id) {
        uint32_t word = (sample + id * 701u) * UINT32_C(1664525) + UINT32_C(1013904223);
        output[8 + id] = (word >> 19) % (sample < 2048u ? 4u : 6u);
    }
    for (uint32_t slot = 0; slot < 16; ++slot) {
        output[28 + slot] = slot % 4u == 3u ? 0u : slot + 1u;
        if (sample % 7u == 0 && slot % 4u == 1u) {
            output[28 + slot] = 0;
        }
        if (sample % 13u == 0 && slot % 4u == 0) {
            output[28 + slot] = (slot + 4u) % 16u + 1u;
        }
    }
    for (uint32_t argument = 0; argument < 8; ++argument) {
        uint32_t word =
            (sample + argument * 131u) * UINT32_C(1664525) + UINT32_C(1013904223);
        output[44 + argument * 2] = (sample / (argument + 1u) + argument) % 6u;
        output[45 + argument * 2] =
            (word & UINT32_C(0x807fffff)) | ((100u + ((word >> 23) % 50u)) << 23);
        if (sample % 17u == 0) {
            output[45 + argument * 2] = argument & 1u ? UINT32_C(0x80000000) : 0;
        }
        if (output[44 + argument * 2] == 5) {
            output[45 + argument * 2] = 101u + (argument & 1u);
        }
    }
    if (output[6] < 2) {
        uint32_t leading = output[6] == 0 ? 2u : 1u;
        output[7] = leading + 6u;
        for (uint32_t argument = 0; argument < output[7]; ++argument) {
            output[44 + argument * 2] =
                argument < leading ? leading == 2 ? 5u : 4u : 3u;
            if (leading == 2 && argument < 2) {
                output[45 + argument * 2] = 101u + argument;
            }
            if ((sample & 1u) && argument >= leading + 4u) {
                output[44 + argument * 2] = 0;
            }
        }
    } else if (output[6] == 2) {
        output[7] = 0;
    }
}

#endif
