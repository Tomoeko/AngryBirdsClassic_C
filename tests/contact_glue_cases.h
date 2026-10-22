#ifndef AB_TEST_CONTACT_GLUE_CASES_H
#define AB_TEST_CONTACT_GLUE_CASES_H

#include <stdint.h>

static const char *const contact_glue_names[] = {"first", "second", "absent"};

static void contact_glue_case(uint32_t sample, uint32_t input[5]) {
    static const uint32_t pairs[7][2] = {{0, 1}, {1, 0}, {0, 0}, {2, 1},
                                         {0, 2}, {2, 2}, {1, 1}};
    input[0] = sample < 512 ? sample & 255u : 240u | (sample & 4u);
    input[1] = (sample / 8) % 12;
    input[2] = (sample / 96) % 4;
    input[3] = input[1] < 5 ? 0 : pairs[input[1] - 5][0];
    input[4] = input[1] < 5 ? 1 : pairs[input[1] - 5][1];
}

#endif
