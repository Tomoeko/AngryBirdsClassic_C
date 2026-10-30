#ifndef AB_TEST_CAMERA_FOLLOW_CASES_H
#define AB_TEST_CAMERA_FOLLOW_CASES_H

#include <stdint.h>

enum {
    FOLLOW_CASES = 256,
    FOLLOW_INPUT_WORDS = 32,
    FOLLOW_STATE_WORDS = 32,
    FOLLOW_EVENT_WORDS = 24,
    FOLLOW_MAX_EVENTS = 128,
    FOLLOW_CURVE_SAMPLES = 6,
    FOLLOW_CURVE_WORDS = 6,
    FOLLOW_UPDATES = 4,
    FOLLOW_CANCEL = 1,
    FOLLOW_NO_LAUNCH = 2,
    FOLLOW_NO_HIT = 4,
    FOLLOW_CACHED_HIT = 8,
    FOLLOW_SCROLL_PRESENT = 16,
    FOLLOW_PEAK_PRESENT = 32,
    FOLLOW_COMPENSATION_PRESENT = 64,
    FOLLOW_ALLOW_CANCEL = 128
};

typedef union {
    float number;
    uint32_t word;
} FollowNumber;

static uint32_t follow_word(float value) {
    FollowNumber number = {.number = value};
    return number.word;
}

static float follow_number(uint32_t value) {
    FollowNumber number = {.word = value};
    return number.number;
}

static void camera_follow_case(uint32_t sample, uint32_t output[FOLLOW_INPUT_WORDS]) {
    uint32_t flags = sample % 8u == 0   ? FOLLOW_CANCEL
                     : sample % 8u == 1 ? FOLLOW_NO_LAUNCH
                     : sample % 8u == 2 ? FOLLOW_NO_HIT
                                        : 0;
    flags |= sample % 11u == 0 ? FOLLOW_CACHED_HIT : 0;
    flags |= sample % 7u == 0 ? FOLLOW_SCROLL_PRESENT : 0;
    flags |= sample % 13u == 0 ? FOLLOW_PEAK_PRESENT : 0;
    flags |= sample & 64u ? FOLLOW_COMPENSATION_PRESENT : 0;
    flags |= sample & 128u ? FOLLOW_ALLOW_CANCEL : 0;
    output[0] = sample;
    output[1] = flags;
    float values[30] = {(float)(sample % 9u) - 4,
                        (float)(sample % 7u) - 3,
                        sample % 3u == 0   ? -6.0f
                        : sample % 3u == 1 ? 0.0f
                                           : 8.0f,
                        sample & 8u ? -12.0f : 4.0f,
                        sample & 16u ? 20.0f : 4.0f,
                        (float)(sample % 11u) - 5,
                        (float)(sample % 13u) - 6,
                        0.5f + (float)(sample % 5u) * 0.25f,
                        20,
                        0.125f,
                        (float)(sample % 5u) - 2,
                        0.75f + (float)(sample % 4u) * 0.25f,
                        0,
                        (float)(sample % 17u) - 8,
                        sample & 4u ? 1.0f : 2.0f,
                        30,
                        -40,
                        (float)(sample % 3u) * 0.5f - 0.5f,
                        sample & 2u ? 0.25f : -0.25f,
                        sample & 32u ? 10000.0f : -10000.0f,
                        0.25f + (float)(sample % 3u) * 0.25f,
                        0.5f + (float)(sample % 5u) * 0.25f,
                        20,
                        0,
                        0,
                        (float)(sample % 4u) * 0.5f - 0.5f,
                        0,
                        (float)(sample % 3u) * 0.35f,
                        0.25f,
                        0};
    static const float distances[] = {0, 999, 1000, 1001};
    values[12] = values[5] * values[8] + distances[(sample / 4u) % 4u];
    for (uint32_t index = 0; index < 30; ++index) {
        output[index + 2] = follow_word(values[index]);
    }
}

static float camera_follow_seconds(uint32_t update) {
    const float values[] = {0.25f, 0.5f, 1, 4};
    return values[update];
}

static float camera_follow_curve_time(uint32_t index) {
    const float values[] = {0, 0.25f, 0.75f, 1.5f, 4, 8};
    return values[index];
}

/* Controlled environment responses shared by the native and C fixtures.
 * These are test inputs, not implementations of camera projection/limits. */
static void camera_follow_response(const uint32_t input[FOLLOW_INPUT_WORDS],
                                   uint32_t kind, uint32_t ordinal,
                                   uint32_t event[FOLLOW_EVENT_WORDS]) {
    float x = follow_number(event[5]), y = follow_number(event[6]);
    float a = 0, b = 0, c = 0;
    if (kind == 1) {
        a = follow_number(input[7]);
        b = follow_number(input[8]);
        c = follow_number(input[9]);
    } else if (kind == 2) {
        a = x * follow_number(input[10]);
        b = y * follow_number(input[10]);
    } else if (kind == 3) {
        a = 0;
        b = x * follow_number(input[11]);
        b = b + follow_number(input[12]);
    } else if (kind == 4) {
        a = follow_number(input[13]) + (float)ordinal * 0.125f;
    } else if (kind == 5) {
        a = follow_number(input[14]);
        b = follow_number(input[15]);
        c = follow_number(input[16]);
    } else if (kind == 6) {
        a = event[4] & 1u ? x : follow_number(input[17]);
        b = event[4] & 2u ? y : follow_number(input[18]);
        a = a + follow_number(input[19]);
        b = b + follow_number(input[20]);
        c = event[4] & 4u ? follow_number(event[7]) : follow_number(input[13]);
    } else if (kind == 7) {
        b = follow_number(input[21]);
    } else if (kind == 8) {
        a = follow_number(input[ordinal == 1 ? 22 : 23]);
    } else if (kind == 10) {
        a = follow_number(input[24]) + (float)ordinal * 0.25f;
    }
    event[13] = follow_word(a);
    event[14] = follow_word(b);
    event[15] = follow_word(c);
}

#endif
