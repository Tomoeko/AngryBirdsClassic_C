#ifndef AB_TEST_BIRD_REMOVE_CASES_H
#define AB_TEST_BIRD_REMOVE_CASES_H

#include <stdint.h>

enum {
    BIRD_REMOVE_INPUT_WORDS = 6,
    BIRD_REMOVE_STATE_WORDS = 60,
    BIRD_REMOVE_EVENT_WORDS = 72,
    BIRD_REMOVE_MAX_EVENTS = 12,
};

typedef enum {
    BIRD_REMOVE_BIRD,
    BIRD_REMOVE_SCRIPT,
    BIRD_REMOVE_DEFINITION,
    BIRD_REMOVE_HATS,
    BIRD_REMOVE_HAT_ENTRY,
    BIRD_REMOVE_HAT_BODY,
    BIRD_REMOVE_OBJECTS_A,
    BIRD_REMOVE_WORLD_A,
    BIRD_REMOVE_DYNAMIC_A,
    BIRD_REMOVE_BIRDS_A,
    BIRD_REMOVE_OTHERS_A,
    BIRD_REMOVE_OBJECTS_B,
    BIRD_REMOVE_WORLD_B,
    BIRD_REMOVE_DYNAMIC_B,
    BIRD_REMOVE_BIRDS_B,
    BIRD_REMOVE_OTHERS_B,
    BIRD_REMOVE_PARTICLES,
    BIRD_REMOVE_CAMERA,
    BIRD_REMOVE_EFFECT,
    BIRD_REMOVE_TABLE_COUNT,
} BirdRemoveTable;

static void bird_remove_case(uint32_t sample, uint32_t output[6]) {
    static const uint32_t volumes[] = {0,
                                       UINT32_C(0x80000000),
                                       UINT32_C(0x3f333333),
                                       UINT32_C(0xbf800000),
                                       UINT32_C(0x7f800000),
                                       UINT32_C(0xff800000),
                                       UINT32_C(0x7fc12345),
                                       UINT32_C(0xffc54321)};
    output[0] = sample;
    output[1] = sample & 511u;
    output[2] = sample < 512 ? 0u : 1u + (sample - 512u) % 15u;
    output[3] = (sample / 19u) % 6u;
    output[4] = volumes[(sample / 7u) % 8u];
    output[5] = output[1] & 16u ? 1u : (sample / 41u) % 6u;
}

#endif
