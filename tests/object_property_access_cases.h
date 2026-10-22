#ifndef AB_TEST_OBJECT_PROPERTY_ACCESS_CASES_H
#define AB_TEST_OBJECT_PROPERTY_ACCESS_CASES_H

#include <stdint.h>

/* Sample, object presence, physical kind/value, three definition result kinds,
 * definition property, material kind/property, host mutations, numeric payload,
 * assigned value, initially present key and operation sequence. Kinds for
 * records are nil/false/table/zero/true/string; property values use
 * nil/false/true/number/string/table. */
static void object_property_access_case(uint32_t sample, uint32_t output[15]) {
    output[0] = sample;
    output[1] = sample % 11u != 0;
    for (uint32_t field = 2; field <= 9; ++field) {
        output[field] = ((sample * (field * 2u + 1u)) / (field + 1u)) % 6u;
    }
    output[10] = (sample / 7u) & 63u;
    uint32_t word = sample * UINT32_C(1664525) + UINT32_C(1013904223);
    output[11] = (word & UINT32_C(0x807fffff)) | ((100u + ((word >> 23) % 50u)) << 23);
    output[12] = (sample / 3u) % 6u;
    output[13] = (sample / 5u) & 1u;
    output[14] = (sample / 13u) % 4u;
    if (sample < 512u) {
        output[1] = 1;
        output[2] = 2;
        output[3] = sample % 6u;
        output[4] = output[5] = output[6] = 2;
        output[7] = (sample / 6u) % 6u;
        output[8] = 2;
        output[9] = (sample / 36u) % 6u;
        output[13] = 1;
        output[14] = 3;
    }
    if (sample % 17u == 0) {
        output[11] = sample & 1u ? UINT32_C(0x80000000) : 0;
    }
    if (sample % 19u == 0) {
        output[11] = UINT32_C(0x7f800000);
    } else if (sample % 23u == 0) {
        output[11] = UINT32_C(0xffa12345);
    }
}

#endif
