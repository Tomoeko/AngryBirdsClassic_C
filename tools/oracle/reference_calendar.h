#ifndef AB_REFERENCE_CALENDAR_H
#define AB_REFERENCE_CALENDAR_H

#include <stdint.h>

static void reference_calendar(unsigned index, float values[12]) {
    static const float fixed[][12] = {
        {2026, 10, 21, 5, 4, 3, 2026, 10, 21, 5, 4, 3},
        {2026, 1, 1, 0, 0, 0, 2025, 12, 31, 23, 59, 59},
        {2026, 1, 1, 0, 0, 0.5f, 2026, 1, 1, 0, 0, -0.5f},
        {2026, 1, 1, 24, -1, 60, 2026, 1, 2, 0, 0, 0},
        {2024, 2, 29, 0, 0, 0, 2024, 3, 1, 0, 0, 0},
        {2025, 2, 29, 0, 0, 0, 2025, 3, 1, 0, 0, 0},
        {2038, 1, 19, 3, 14, 7, 2038, 1, 19, 3, 14, 8},
        {1901, 12, 13, 20, 45, 52, 1901, 12, 13, 20, 45, 51},
        {2100, 1, 1, 0, 0, 0, 1900, 1, 1, 0, 0, 0},
        {1969, 12, 31, 23, 59, 59, 1969, 12, 31, 23, 59, 58},
        {2026, 0, 0, 0, 0, 0, 2025, 11, 30, 0, 0, 0},
        {2027, 1, 1, 0, 0, 0, 2026, 1, 1, 0, 0, 0},
        {2026, 12.5f, 1, -0.5f, 59.5f, 59.5f, 2027, 1, 1, 0, 1, 0},
        {1999.5f, 2.5f, 1, 0, 0, 0, 2000, 3, 1, 0, 0, 0},
        {1970, 1, 1, 0, 0, 0, 1900, 1, 1, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    };
    if (index < sizeof(fixed) / sizeof(*fixed)) {
        for (unsigned field = 0; field < 12; ++field)
            values[field] = fixed[index][field];
        return;
    }
    static const float years[] = {1900, 1901, 1969, 1970, 2000, 2001,
                                  2024, 2026, 2037, 2038, 2039, 2100};
    uint32_t random = UINT32_C(0x520a6a7e) ^ (index * UINT32_C(2654435761));
    for (unsigned field = 0; field < 12; ++field) {
        random = random * UINT32_C(1664525) + UINT32_C(1013904223);
        unsigned position = field % 6;
        if (!position)
            values[field] = years[(random >> 16) % 12];
        else {
            unsigned bound = position == 1   ? 16u
                             : position == 2 ? 36u
                             : position == 3 ? 29u
                                             : 80u;
            int integer = (int)((random >> 8) % bound) - 2;
            values[field] = (float)integer + (float)(random & 3) * 0.25f;
        }
    }
}

#endif
