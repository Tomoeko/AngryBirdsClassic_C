#ifndef AB_TEST_GAME_JOINT_NAMED_CASES_H
#define AB_TEST_GAME_JOINT_NAMED_CASES_H

#include <stdint.h>

static const char *const joint_named_names[] = {"joint", "joint2", "joina", "",
                                                "absent"};

static void game_joint_named_case(uint32_t sample, uint32_t header[7],
                                  uint32_t rows[4][2]) {
    header[0] = sample;
    header[1] = (sample >> 3) & 1u;
    header[2] = header[1] ? (sample >> 7) & 15u : 0;
    header[3] = sample % 5;
    header[4] = 0;
    header[5] = sample & 16u ? header[3] : 1;
    header[6] = 0;
    for (uint32_t index = 0; index < 4; ++index) {
        uint32_t mode = (sample >> 5) & 3u;
        rows[index][0] = sample & (1u << index) ? 5 : 2;
        rows[index][1] = mode == 0    ? index
                         : mode == 1  ? index % 2
                         : mode == 2  ? 0
                         : index == 3 ? 0
                                      : index;
    }
}

#endif
