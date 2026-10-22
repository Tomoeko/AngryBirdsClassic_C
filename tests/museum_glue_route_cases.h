#ifndef AB_TEST_MUSEUM_GLUE_ROUTE_CASES_H
#define AB_TEST_MUSEUM_GLUE_ROUTE_CASES_H

#include <stdint.h>

/* Identity, ordered colliders, two initial dead values, initial disable/pair,
 * primary bird-disable result, primary dead insertions and grabber type. */
static void museum_glue_route_case(uint32_t sample, uint32_t output[9]) {
    output[0] = sample;
    output[1] = sample % 12u;
    output[2] = (sample / 12u) % 12u;
    if ((output[1] == 1 || output[1] == 3) && (output[2] == 1 || output[2] == 3)) {
        output[2] = 0;
    }
    output[3] = (sample / 144u) % 6u;
    output[4] = (sample / 864u) % 6u;
    output[5] = (sample / 23u) % 2u;
    output[6] = (sample / 41u) % 2u;
    output[7] = sample < 144 ? 0 : (sample / 67u) % 4u;
    output[8] = sample < 144 ? 0 : (sample * 11u / 37u) % 6u;
}

#endif
