#ifndef AB_TEST_GAME_OBJECT_REMOVE_JOINTS_CASES_H
#define AB_TEST_GAME_OBJECT_REMOVE_JOINTS_CASES_H

#include <stdint.h>

static const char *const removal_joint_names[] = {"joint0", "joint1", "joint2",
                                                  "joint3"};
static const char *const removal_object_names[] = {"first", "second", "third",
                                                   "missing"};

static void game_object_remove_joints_case(uint32_t sample, uint32_t header[4],
                                           uint32_t rows[4][4]) {
    static const uint32_t timers[] = {0,          0x80000000, 0x3e800000, 0xbf800000,
                                      0x7f800000, 0xff800000, 0x7fc12345, 0xffc12345};
    header[0] = sample;
    header[1] = sample % 4u;
    header[2] = (sample / 4u) % 2u;
    header[3] = (sample / 8u) % 16u;
    for (uint32_t index = 0; index < 4; ++index) {
        rows[index][0] = (sample / (index + 1u) + index) % 5u + 1u;
        rows[index][1] = (sample / (index + 2u) + index) % 3u;
        rows[index][2] = (rows[index][1] + 1u + ((sample >> (index + 3u)) & 1u)) % 3u;
        rows[index][3] = timers[(sample / (index + 1u) + index) % 8u];
    }
}

#endif
