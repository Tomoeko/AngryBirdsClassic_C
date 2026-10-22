#ifndef AB_TEST_GAME_JOINT_BREAK_CASES_H
#define AB_TEST_GAME_JOINT_BREAK_CASES_H

#include <stdint.h>

static const char *const joint_break_names[] = {"joint",  "joint2", "joint3", "joina",
                                                "joint4", "joint5", ""};
static const char *const joint_break_objects[] = {"first", "other",  "second", "both",
                                                  "",      "firstx", "xxxxx",  "none"};

static void game_joint_break_case(uint32_t sample, uint32_t header[5],
                                  uint32_t records[7][7]) {
    static const uint32_t forces[] = {0,
                                      UINT32_C(0x80000000),
                                      UINT32_C(0x3f800000),
                                      UINT32_C(0xbf800000),
                                      UINT32_C(0x3f7fffff),
                                      UINT32_C(0x3f800001),
                                      UINT32_C(0x40000000),
                                      UINT32_C(0xc0000000),
                                      UINT32_C(0x00000001),
                                      UINT32_C(0x80000001)};
    header[0] = sample;
    header[1] = (sample >> 1) % 8;
    header[2] = forces[(sample / 16) % 10];
    header[3] = 1 + sample % 6;
    header[4] = (sample >> 11) & 1u;
    for (uint32_t index = 0; index < 7; ++index) {
        uint32_t mode = (sample >> 8) & 3u;
        uint32_t name = mode == 0   ? index
                        : mode == 1 ? index % 2
                        : mode == 2 ? (index >= 5 ? 0 : index)
                                    : 0;
        uint32_t boolean_mode = (sample + index * 17) % 6;
        uint32_t force_mode = (sample / 6 + index * 5) % 6;
        uint32_t *record = records[index];
        record[0] = name;
        record[1] = (index + sample) % 6;
        record[2] = (index * 3 + (sample >> 4)) % 6;
        record[3] = boolean_mode < 3   ? (boolean_mode == 0 ? 0 : 1)
                    : boolean_mode < 5 ? 3
                                       : 4;
        record[4] = boolean_mode == 1   ? 0
                    : boolean_mode == 2 ? 1
                    : boolean_mode == 3 ? 0
                                        : UINT32_C(0x3f800000);
        record[5] = force_mode < 3 ? 3 : force_mode == 3 ? 0 : force_mode == 4 ? 1 : 4;
        record[6] = forces[(sample / 16 + index) % 10];
        if (sample >= 2048) {
            header[1] = 0;
            record[1] = index & 1u ? 1 : 0;
            record[2] = index & 1u ? 0 : (sample & 32u ? 0 : 2);
            record[3] = 1;
            record[4] = index == 5   ? (sample >> 7) & 1u
                        : index == 6 ? (sample >> 8) & 1u
                                     : 1;
            record[5] = sample & 64u ? 3 : 0;
            record[6] = forces[(sample / 16 + (sample >> 5)) % 10];
        }
    }
}

#endif
