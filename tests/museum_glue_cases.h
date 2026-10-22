#ifndef AB_TEST_MUSEUM_GLUE_CASES_H
#define AB_TEST_MUSEUM_GLUE_CASES_H

#include <stdint.h>

/* Identity, grabber type, collider, plunger object, initial scalar types, and
 * velocity-host name mutations. Plunger may alias either argument. */
static void museum_glue_case(uint32_t sample, uint32_t output[6]) {
    output[0] = sample;
    output[1] = sample % 6u;
    output[2] = (sample / 6u) % 12u;
    output[3] = (sample / 72u) % 3u;
    output[4] = (sample / 216u) % 4u;
    output[5] = (sample / 864u) % 4u;
}

#endif
