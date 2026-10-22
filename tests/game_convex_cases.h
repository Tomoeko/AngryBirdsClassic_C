#ifndef AB_TEST_GAME_CONVEX_CASES_H
#define AB_TEST_GAME_CONVEX_CASES_H

#include "game_polygon_cases.h"

/* Strict convex subsets of a chamfered rectangle, in both winding orders.
 * Input20 is the vertex count. Extra point coordinates occupy input21..30;
 * the original twenty parameter words retain their shared positions.
 */
static void game_convex_case(uint32_t sample, uint32_t output[32]) {
    game_polygon_case(sample, output);
    for (size_t field = 20; field < 32; ++field) {
        output[field] = 0;
    }
    const uint32_t points[8][2] = {{0, UINT32_C(0x3e800000)},
                                   {UINT32_C(0x3e800000), 0},
                                   {UINT32_C(0x3f400000), 0},
                                   {UINT32_C(0x3f800000), UINT32_C(0x3e800000)},
                                   {UINT32_C(0x3f800000), UINT32_C(0x3f400000)},
                                   {UINT32_C(0x3f400000), UINT32_C(0x3f800000)},
                                   {UINT32_C(0x3e800000), UINT32_C(0x3f800000)},
                                   {0, UINT32_C(0x3f400000)}};
    uint32_t count = 3 + sample % 6;
    output[20] = count;
    for (size_t vertex = 0; vertex < count; ++vertex) {
        size_t order = sample & 64u ? count - vertex - 1 : vertex;
        size_t point = ((sample / 6) + order) % 8;
        size_t position = vertex < 3 ? 6 + vertex * 2 : 21 + (vertex - 3) * 2;
        output[position] = points[point][0];
        output[position + 1] = points[point][1];
    }
}

#endif
