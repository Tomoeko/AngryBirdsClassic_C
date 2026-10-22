#ifndef AB_TEST_LEVEL_PROPERTIES_CASES_H
#define AB_TEST_LEVEL_PROPERTIES_CASES_H

#include <stdint.h>

enum {
    LEVEL_PROPERTY_CASES = 2048,
    LEVEL_PROPERTY_INPUT_WORDS = 12,
    LEVEL_PROPERTY_OUTPUT_WORDS = 18,
};

/* Scalar kinds: nil, false, true, number, "value", payload table.
 * Definition/material records: nil, false, table, zero, true, "value".
 * Operations: read, radius write, physical-name write, world forget,
 * nil physicsObject, script-name write to an absent name. */
static void level_property_case(uint32_t index, uint32_t input[12]) {
    static const uint32_t numbers[] = {0,
                                       UINT32_C(0x80000000),
                                       UINT32_C(0x3f59999a),
                                       UINT32_C(0xbf800000),
                                       UINT32_C(0x7f800000),
                                       UINT32_C(0xff800000),
                                       UINT32_C(0x7fc12345),
                                       UINT32_C(0x7f812345),
                                       1,
                                       UINT32_C(0x80000001)};
    input[0] = index;
    input[1] = index / 3u % 6u;
    input[2] = index / 5u % 6u;
    input[3] = index / 7u % 6u;
    input[4] = index / 11u % 6u;
    input[5] = index / 13u % 6u;
    input[6] = index % 6u;
    input[7] = index / 17u % 6u;
    input[8] = numbers[index / 19u % 10u];
    input[9] = numbers[index / 23u % 10u];
    input[10] = numbers[index / 29u % 10u];
    input[11] = index % 4u;
}

#endif
