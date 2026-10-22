#ifndef AB_TEST_BLOCK_REMOVAL_SPAWN_CASES_H
#define AB_TEST_BLOCK_REMOVAL_SPAWN_CASES_H

#include <stdint.h>

static void block_removal_spawn_case(uint32_t sample, uint32_t output[15]) {
    static const uint32_t sizes[] = {0,          0x3f800000, 0x40a00000, 0xc0a00000,
                                     0x7f7fffff, 0x00800000, 0x40800000, 0x80000000};
    static const uint32_t angles[] = {0,          0x80000000, 0x3fc90fdb, 0xbfc90fdb,
                                      0x41880000, 0xc1880000, 0x7f7fffff, 0xff7fffff};
    static const uint32_t target[] = {0, 0x40000000, 0x3e800000, 0x41000000};
    static const uint32_t amounts[] = {0, 0x3f800000, 0x40200000, 0x40400000};
    static const uint32_t death[] = {0, 0x3f800000, 0x40000000};
    static const uint32_t choices[] = {0x3f800000, 0, 0x40000000, 0xbf800000};
    static const uint32_t units[] = {0x3e800000, 0, 0x80000000, 0x3f400000};
    static const uint32_t scales[] = {0x40000000, 0x3f000000, 0, 0xbf800000};
    static const uint32_t pi[] = {0x40490fdb, 0x3f800000, 0x40000000, 0x3e800000};
    output[0] = sample;
    output[1] = sample;
    if (sample >= 1024) {
        output[1] = 3u | (sample & 12u);
    }
    output[2] = sizes[(sample / 3u) % 8u];
    output[3] = sizes[(sample / 7u) % 8u];
    output[4] = target[(sample / 11u) % 4u];
    output[5] = angles[(sample / 13u) % 8u];
    output[6] = sizes[(sample / 17u) % 8u];
    output[7] = sizes[(sample / 19u) % 8u];
    output[8] = amounts[(sample / 23u) % 4u];
    output[9] = death[(sample / 29u) % 3u];
    output[10] = choices[(sample / 31u) % 4u];
    output[11] = units[(sample / 37u) % 4u];
    output[12] = scales[(sample / 41u) % 4u];
    output[13] = pi[(sample / 43u) % 4u];
    output[14] = (sample / 47u) % 4u;
    if (sample < 128) {
        output[2] = 0x40a00000;
        output[3] = 0x40800000;
        output[4] = 0x40000000;
        output[5] = 0;
        output[6] = 0x41200000;
        output[7] = 0x41a00000;
        output[8] = 0x40400000;
        output[9] = 0x40000000;
        output[10] = 0x3f800000;
        output[11] = 0x3e800000;
        output[12] = 0x40000000;
        output[13] = 0x40490fdb;
        output[14] = 1;
    }
}

#endif
