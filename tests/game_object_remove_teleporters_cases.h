#ifndef AB_TEST_GAME_OBJECT_REMOVE_TELEPORTERS_CASES_H
#define AB_TEST_GAME_OBJECT_REMOVE_TELEPORTERS_CASES_H

#include <stdint.h>

static void game_object_remove_teleporters_case(uint32_t sample, uint32_t input[5]) {
    input[0] = sample;
    input[1] = sample % 64u;
    input[2] = (sample / 64u) % 4u;
    input[3] = (sample / 256u) % 2u;
    input[4] = (sample / 512u) % 2u;
}

#endif
