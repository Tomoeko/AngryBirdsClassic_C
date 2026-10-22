#ifndef AB_TEST_LEVEL_EFFECT_CASES_H
#define AB_TEST_LEVEL_EFFECT_CASES_H

#include <stdint.h>

/* Four base methods, each with eight populated definitions and eight scalar/
 * empty definitions. Receivers and bird arguments vary but remain unread. */
static void level_effect_case(uint32_t sample, uint32_t output[5]) {
    output[0] = sample;
    output[1] = sample / 16u;
    output[2] = sample % 16u;
    output[3] = (sample * 3u + 1u) % 8u;
    output[4] = (sample * 5u + 3u) % 8u;
}

#endif
