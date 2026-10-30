#ifndef AB_REFERENCE_CREATE_CIRCLE_H
#define AB_REFERENCE_CREATE_CIRCLE_H

#include <stdint.h>

enum { CREATE_CIRCLE_CASES = 128, CREATE_CIRCLE_INPUTS = 9 };

static const char *const create_circle_fields[] = {"name",
                                                   "sprite",
                                                   "type",
                                                   "x",
                                                   "y",
                                                   "width",
                                                   "height",
                                                   "radius",
                                                   "angle",
                                                   "density",
                                                   "friction",
                                                   "restitution",
                                                   "mass",
                                                   "xVel",
                                                   "yVel",
                                                   "z_order",
                                                   "definition",
                                                   "controllable",
                                                   "gameObjectScript"};

/* Prepared direct native constructor inputs; no recovered behavior in this file. */
static void reference_create_circle(unsigned sample, uint32_t output[9]) {
    float values[] = {(float)((int)(sample * 37 % 127) - 63) / 8.0f,
                      (float)((int)(sample * 43 % 127) - 63) / 16.0f,
                      (float)(sample % 11) / 8.0f,
                      sample % 4 ? (float)(sample % 13 + 1) / 8.0f : 0,
                      (float)(sample % 9) / 16.0f,
                      (float)(sample % 7) / 8.0f,
                      (float)((int)(sample % 31) - 15) / 4.0f};
    if (sample < 8) {
        values[0] = sample & 1 ? -0.0f : 0;
        values[1] = sample & 2 ? -0.0f : 0;
        values[2] = sample & 4 ? -0.0f : 0;
    }
    if (sample == 9)
        values[3] = -0.0f;
    for (unsigned index = 0; index < 7; ++index) {
        union {
            float value;
            uint32_t bits;
        } word = {.value = values[index]};
        output[index] = word.bits;
    }
    output[7] = sample & 1;
    output[8] = sample & 2 ? 1u : 0u;
}

/* Independent fixture-recreation inputs; these are not expected results. */
#if defined(AB_ORACLE_RESIZE_CIRCLE_ONLY) || defined(AB_TEST_RESIZE_CIRCLE)
static uint32_t reference_resize_circle(unsigned sample) {
    union {
        float number;
        uint32_t bits;
    } radius = {.number = (float)(sample * 29 % 61) / 8.0f};
    if (sample < 4)
        radius.number = sample & 1 ? -0.0f : 0;
    return radius.bits;
}
#endif

#endif
