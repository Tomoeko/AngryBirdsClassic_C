#ifndef AB_MIGHTY_EAGLE_TIMER_CASES_H
#define AB_MIGHTY_EAGLE_TIMER_CASES_H

#include <stdint.h>

enum { AB_MIGHTY_EAGLE_TIMER_CASES = 1024 };

static void mighty_timer_case(uint32_t sample, uint32_t output[6]) {
    const float timers[] = {-0.5f, 0, 6.2f, 6.200001f, 7.7f, 7.700001f, 8.7f, 12};
    const float deltas[] = {0, 1.0f / 60, 0.5f, 1, 2.5f, 8.7f};
    const float shakes[] = {0, 100, -1, -0.0f};
    union {
        float value;
        uint32_t bits;
    } value;
    value.value = timers[(sample / 3) % 8];
    output[0] = value.bits;
    value.value = deltas[(sample / 7) % 6];
    output[1] = value.bits;
    value.value = shakes[(sample / 11) % 4];
    output[2] = value.bits;
    value.value = (float)((int32_t)((sample * 37) % 129) - 64) / 3;
    output[3] = value.bits;
    value.value = (float)((int32_t)((sample * 61) % 129) - 64) / 3;
    output[4] = value.bits;
    output[5] = sample & 511;
}

#endif
