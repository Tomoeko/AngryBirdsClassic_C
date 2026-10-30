#ifndef AB_TEST_BIRD_IMPACT_WORLD_CASES_H
#define AB_TEST_BIRD_IMPACT_WORLD_CASES_H

#include "game_raycast_cases.h"

enum {
    IMPACT_WORLD_CASES = 4,
    IMPACT_WORLD_INPUT_WORDS = 10,
    IMPACT_WORLD_OUTPUT_WORDS = 16,
    IMPACT_WORLD_EVENT_WORDS = 20,
    IMPACT_WORLD_MAX_EVENTS = 2048
};

static void bird_impact_world_case(uint32_t scene, uint32_t profile,
                                   uint32_t output[IMPACT_WORLD_INPUT_WORDS]) {
    static const float inputs[4][7] = {{-8, -2, 8, 0, 0, 10, 0.25f},
                                       {8, 0, -6, -2, 4, 5, 0.5f},
                                       {-5, -7, 5, 2, 20, 20, 0.75f},
                                       {0, 8, 2, -10, 10, 10, 1}};
    output[0] = scene;
    output[1] = profile;
    for (uint32_t index = 0; index < 7; ++index) {
        output[index + 2] = world_ray_word(inputs[profile][index]);
    }
    output[9] = scene % 3u != 0;
#if defined(AB_TEST_BIRD_IMPACT_WORLD_NUMERIC) ||                                      \
    defined(AB_ORACLE_BIRD_IMPACT_WORLD_NUMERIC)
    output[4] = 0;
    output[5] = world_ray_word(profile == 1 ? -2.0f : profile == 3 ? -10.0f : 0.0f);
    output[6] = world_ray_word(profile == 0 ? 0.0f : 20.0f);
    output[7] = world_ray_word(profile == 3 ? 2.0f : 10.0f);
#endif
}

#endif
