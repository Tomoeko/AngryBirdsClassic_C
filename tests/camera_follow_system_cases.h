#ifndef AB_TEST_CAMERA_FOLLOW_SYSTEM_CASES_H
#define AB_TEST_CAMERA_FOLLOW_SYSTEM_CASES_H

#include <stdint.h>

enum {
#if defined(AB_TEST_CAMERA_FOLLOW_WORLD) || defined(AB_ORACLE_CAMERA_FOLLOW_WORLD)
    OWNER_FOLLOW_SCENES = 48,
#else
    OWNER_FOLLOW_SCENES = 16,
#endif
#if defined(AB_TEST_CAMERA_FOLLOW_END_SEQUENCE) ||                                     \
    defined(AB_ORACLE_CAMERA_FOLLOW_END_SEQUENCE)
    OWNER_FOLLOW_STEPS = 160,
    OWNER_FOLLOW_OUTPUT_WORDS = 80,
#else
    OWNER_FOLLOW_STEPS = 32,
    OWNER_FOLLOW_OUTPUT_WORDS = 64,
#endif
    OWNER_FOLLOW_CONFIG_WORDS = 24,
    OWNER_FOLLOW_COMMAND_WORDS = 12,
    OWNER_FOLLOW_EVENT_WORDS = 16,
#if defined(AB_TEST_CAMERA_FOLLOW_WORLD) || defined(AB_ORACLE_CAMERA_FOLLOW_WORLD)
    OWNER_FOLLOW_MAX_EVENTS = 2048,
#else
    OWNER_FOLLOW_MAX_EVENTS = 16,
#endif
};

typedef union {
    float number;
    uint32_t word;
} OwnerFollowNumber;

static uint32_t owner_follow_word(float number) {
    OwnerFollowNumber value = {.number = number};
    return value.word;
}

static float owner_follow_number(uint32_t word) {
    OwnerFollowNumber value = {.word = word};
    return value.number;
}

/* Explicit scene inputs: pixel size; two snap positions/zoom; first bird
 * position/launch; predicted point/time; gravity; transform scale; flags and
 * second bird position/launch. Flags: 1 no predicted hit, 2 no first launch,
 * 4 default allowCancel absent (otherwise false), 8 zero compensation supplied. */
static void camera_follow_system_config(uint32_t scene, uint32_t output[24]) {
#if defined(AB_TEST_CAMERA_FOLLOW_END_SEQUENCE) ||                                     \
    defined(AB_ORACLE_CAMERA_FOLLOW_END_SEQUENCE)
    scene += 2;
#endif
    output[0] = scene;
    const float values[] = {scene & 1u ? 640.0f : 480.0f,
                            scene & 1u ? 360.0f : 320.0f,
                            -200,
                            scene & 2u ? -40.0f : 0.0f,
                            2,
                            1000,
                            scene & 2u ? 40.0f : 0.0f,
                            1,
                            (float)(scene % 3u),
                            scene & 4u ? -4.0f : -20.0f,
                            scene & 8u ? -8.0f : 8.0f,
                            scene & 4u ? 4.0f : -12.0f,
                            40,
                            scene < 2   ? -2.5f
                            : scene < 4 ? -20
                                        : -5,
                            scene == 2   ? 0.25f
                            : scene == 3 ? 0.6f
                                         : 0.75f + (float)(scene % 4u) * 0.25f,
                            20,
                            20};
    for (uint32_t index = 0; index < 17; ++index) {
        output[index + 1] = owner_follow_word(values[index]);
    }
    output[18] = (scene == 12 ? 1u : 0u) | (scene == 13 ? 2u : 0u) |
                 (scene & 1u ? 4u : 0u) | (scene & 2u ? 8u : 0u);
    output[19] = owner_follow_word(-2);
    output[20] = owner_follow_word(-16);
    output[21] = owner_follow_word(scene & 8u ? 6.0f : -6.0f);
    output[22] = owner_follow_word(-8);
    output[23] = 0;
#if defined(AB_TEST_CAMERA_FOLLOW_WORLD) || defined(AB_ORACLE_CAMERA_FOLLOW_WORLD)
    output[9] = owner_follow_word((float)(scene % 3u) - 1);
    output[10] = owner_follow_word(-6 + (float)(scene % 3u));
    output[11] = owner_follow_word(scene % 4u == 0   ? 0.0f
                                   : scene % 4u == 1 ? 1.0f
                                   : scene % 4u == 2 ? -1.0f
                                                     : 2.0f);
    output[12] = owner_follow_word(scene & 4u ? 0.0f : -2.0f);
    output[19] = owner_follow_word(-1);
    output[20] = owner_follow_word(-4);
    output[21] = owner_follow_word(scene & 1u ? -1 : 2.0f);
    output[22] = owner_follow_word(-1);
    if (scene >= 18) {
        output[10] = owner_follow_word(-12 + (float)(scene % 3u));
        output[20] = owner_follow_word(-12);
    }
    if (scene >= 34) {
        output[10] = owner_follow_word(-1.5f);
        output[12] = owner_follow_word(-10);
        output[20] = owner_follow_word(0);
        output[22] = owner_follow_word(-8);
    }
#endif
}

/* Every command performs its action, then one update. Actions: 1 follow first,
 * 2 follow second, 3 remove first, 4 remove second, 5 clear follow, 6 goto
 * position, 7 goto first snap, 8 remove nil, 9 queue position then remove nil. */
static void camera_follow_system_command(uint32_t scene, uint32_t step,
                                         uint32_t output[12]) {
    uint32_t action = step == 0 || step == 12 ? 1u
                      : step == 4             ? 4u
                      : step == 5             ? 3u
                      : step == 7             ? 2u
                      : step == 10            ? 6u
                      : step == 21            ? 5u
                      : step == 22            ? 7u
                      : step == 29            ? 9u
                      : step == 30            ? 8u
                                              : 0u;
    uint32_t flags = step == 8 ? 1u : 0;
    if (step == 8 || step == 9 || step >= 23) {
        flags |= 2u;
    }
    output[0] = action;
    output[1] = owner_follow_word(step % 9u == 0 ? 0 : step % 7u == 0 ? 0.25f : 0.1f);
    output[2] = flags;
    output[3] = step == 27 ? 0u : step == 25 ? 1u : step == 26 ? 4u : 7u;
    output[4] = owner_follow_word(300 + (float)(scene % 4u) * 20);
    output[5] = owner_follow_word((float)(scene % 3u) * -10);
    output[6] = owner_follow_word(1.25f);
    output[7] = owner_follow_word(step & 1u ? 0.25f : 0);
    output[8] = owner_follow_word(step & 1u ? 0.5f : 0);
    output[9] = owner_follow_word((float)(step % 3u) * 0.125f);
    output[10] = owner_follow_word((float)(step % 5u) * -0.125f);
    output[11] = step;
#if defined(AB_TEST_CAMERA_FOLLOW_END_SEQUENCE) ||                                     \
    defined(AB_ORACLE_CAMERA_FOLLOW_END_SEQUENCE)
    if (step >= 32) {
        output[0] = step == 36 || step == 96 ? 10u : 0u;
        output[1] = owner_follow_word(0.25f);
        output[2] = step == 100 && (scene & 1u) ? 1u : 0u;
        if (step >= 45 && step <= 50) {
            output[2] |= 4u;
        }
    }
#endif
}

#endif
