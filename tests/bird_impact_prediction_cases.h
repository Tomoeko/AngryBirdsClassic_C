#ifndef AB_TEST_BIRD_IMPACT_PREDICTION_CASES_H
#define AB_TEST_BIRD_IMPACT_PREDICTION_CASES_H

#include <stdint.h>

enum {
    PREDICT_INPUT_WORDS = 15,
    PREDICT_OUTPUT_WORDS = 14,
    PREDICT_EVENT_WORDS = 12,
    PREDICT_MAX_EVENTS = 1600
};

static void bird_impact_prediction_case(uint32_t sample, uint32_t output[15]) {
    static const uint32_t steps[] = {UINT32_C(0x3f800000), UINT32_C(0x40800000),
                                     UINT32_C(0x41200000), UINT32_C(0x41c00000),
                                     UINT32_C(0xc1200000), 0,
                                     UINT32_C(0x7fc12345), UINT32_C(0x80000000)};
    static const uint32_t velocities[][2] = {
        {UINT32_C(0x41a00000), UINT32_C(0xc1200000)},
        {UINT32_C(0xc1700000), UINT32_C(0x40000000)},
        {0, 0},
        {UINT32_C(0x80000000), UINT32_C(0x41200000)}};
    output[0] = sample;
    output[1] = sample < 64 ? 0u : sample & 15u;
    output[2] = sample % 3u; /* Nil, false, numeric steps. */
    output[3] = steps[(sample / 3u) % 8u];
    output[4] = UINT32_C(0x3f800000) + ((sample * 271u) & UINT32_C(0x007fffff));
    output[5] = sample & 1u ? UINT32_C(0x80000000) : UINT32_C(0xc1000000);
    output[6] = velocities[(sample / 4u) % 4u][0];
    output[7] = velocities[(sample / 4u) % 4u][1];
    output[8] = sample & 1u ? UINT32_C(0x41a00000) : UINT32_C(0x419ce80a);
    output[9] = velocities[(sample / 7u + 1u) % 4u][0];
    output[10] = velocities[(sample / 7u + 1u) % 4u][1];
    output[11] = sample & 2u ? UINT32_C(0xc1200000) : UINT32_C(0x3f800000);
    output[12] = sample & 4u ? 0 : UINT32_C(0x3f000000);
    output[13] = UINT32_C(0xbf800000);
    output[14] = sample % 6u;
    if (output[2] == 2 && !(output[3] & UINT32_C(0x7fffffff))) {
        output[14] = 0; /* Infinite interval with persistent hits would not stop. */
    }
}

static uint32_t bird_impact_prediction_hit(uint32_t mode, uint32_t query) {
    return mode == 1   ? 1u
           : mode == 2 ? query % 7u == 0
           : mode == 3 ? query % 3u == 2
           : mode == 4 ? query <= 3u
           : mode == 5 ? query % 3u == 0
                       : 0u;
}

static void bird_impact_prediction_point(uint32_t sample, uint32_t query,
                                         uint32_t output[2]) {
    output[0] =
        UINT32_C(0x3f800000) + ((sample * 17u + query * 7u) & UINT32_C(0x007fffff));
    output[1] =
        UINT32_C(0xbf800000) + ((sample * 23u + query * 13u) & UINT32_C(0x007fffff));
}

#endif
