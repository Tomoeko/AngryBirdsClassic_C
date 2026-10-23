#ifndef AB_BIRD_HAT_CASES_H
#define AB_BIRD_HAT_CASES_H

#include <stdint.h>

enum { AB_BIRD_HAT_CASES = 1024, AB_BIRD_HAT_WORDS = 128 };

typedef struct {
    uint32_t counter_kind, destroyed_kind;
    float counter, x, y, dx, dy, velocity, height;
    uint32_t absent, change_counter, change_parameters, change_physics, remove_on_print;
} BirdHatCase;

static BirdHatCase bird_hat_case(uint32_t sample) {
    static const float counters[] = {0,        1,  9,    99,   999999, 10000000,
                                     16777216, -1, 0.1f, 0.5f, -0.0f,  123.25f};
    BirdHatCase value = {.counter_kind = sample % 4,
                         .destroyed_kind = sample / 64 % 5,
                         .counter = counters[sample / 4 % 12],
                         .x = ((float)(sample % 127) - 63) * 0.125f,
                         .y = ((float)(sample % 113) - 56) * 0.25f,
                         .dx = ((float)(sample % 31) - 15) * 0.0625f,
                         .dy = ((float)(sample % 29) - 14) * 0.03125f,
                         .velocity = ((float)(sample % 97) - 48) * 0.333333343f,
                         .height = 1.7f + (float)(sample % 7) * 0.03125f,
                         .absent = sample % 64 >= 60,
                         .change_counter = sample & 4,
                         .change_parameters = sample & 8,
                         .change_physics = sample & 16,
                         .remove_on_print = sample & 32};
    if (sample % 97 == 0)
        value.velocity = -0.0f;
    return value;
}

#endif
