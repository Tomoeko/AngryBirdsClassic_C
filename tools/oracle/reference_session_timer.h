#ifndef AB_REFERENCE_SESSION_TIMER_H
#define AB_REFERENCE_SESSION_TIMER_H

#include <stdint.h>

/* Authored deterministic inputs, shared by the original-module probe and C
 * checker. No source implementation or expected result is shared here. */
static void reference_session_timer(unsigned profile, float date[6],
                                    float duration[3]) {
    static const float durations[][3] = {
        {0, 5, 0},         {0, 0, 0},       {0, 0, -1},     {1, 60, 60.5f},
        {-1, -2, -3.25f},  {0, 0, 59.75f},  {0, 0, -0.75f}, {0, 0, 3600},
        {0.5f, -1, -0.5f}, {0, 0, -59.25f}, {0, 0, -1.75f}, {0, 0, -3600},
    };
    date[0] = profile & 8 ? 2024 : 2026;
    date[1] = profile & 8 ? 2 : 12;
    date[2] = profile & 8 ? 29 : 31;
    date[3] = 23;
    date[4] = 59;
    date[5] = profile & 4 ? 59.75f : 0;
    for (unsigned index = 0; index < 3; ++index)
        duration[index] = durations[profile % 12][index];
}

static float reference_session_elapsed(unsigned step) {
    static const float values[] = {0,   0,    1,    59.5f, 60, 120, 299, 300, 301,
                                   600, 3599, 3600, 3601,  -1, -60, 0,   2,   10};
    return values[step];
}

#endif
