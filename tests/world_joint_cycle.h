#ifndef AB_TEST_WORLD_JOINT_CYCLE_H
#define AB_TEST_WORLD_JOINT_CYCLE_H

#include "joint_island_cases.h"

static void world_joint_cycle_operation(size_t frame, uint32_t operation[4]) {
    /* Kind, logical joint/body slot, first body, second body. */
    static const uint8_t operations[29][4] = {
        {1, 0, 0, 1}, {1, 1, 1, 2}, {1, 2, 2, 3}, {1, 3, 1, 2},  {3, 4, 0, 4},
        {4, 1, 0, 0}, {2, 1, 0, 0}, {1, 1, 3, 4}, {11, 0, 0, 0}, {1, 0, 4, 0},
        {5, 2, 0, 0}, {6, 2, 0, 0}, {1, 2, 2, 3}, {1, 3, 1, 2},  {7, 3, 0, 0},
        {8, 3, 0, 0}, {2, 3, 0, 0}, {2, 2, 0, 0}, {1, 4, 0, 4},  {5, 4, 0, 0},
        {6, 4, 0, 0}, {1, 1, 1, 4}, {9, 1, 0, 0}, {2, 1, 0, 0},  {1, 1, 1, 2},
        {1, 2, 2, 3}, {1, 3, 3, 4}, {1, 4, 0, 4}, {1, 0, 0, 1},
    };
    for (size_t field = 0; field < 4; ++field) {
        operation[field] = frame % 64 < 29 ? operations[frame % 64][field] : 0;
    }
    if (frame % 64 >= 60) {
        operation[0] = frame % 64 == 63 ? 10u : 2u;
        operation[1] = (uint32_t)(frame % 64 - 60);
    }
}

static void world_joint_cycle_input(uint32_t scene, uint32_t frame,
                                    const uint32_t operation[4], uint32_t input[41]) {
    joint_island_input(scene * 256 + frame, operation[1], 5, input);
    input[0] = (scene + operation[1]) % 4;
    input[1] = operation[2];
    input[2] = operation[3];
    if (input[0] == 0) {
        input[17] &= UINT32_C(0x7fffffff);
    }
}

#endif
