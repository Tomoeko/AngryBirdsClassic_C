#ifndef AB_REFERENCE_CREATE_BOX_H
#define AB_REFERENCE_CREATE_BOX_H

#include <stdint.h>

enum { CREATE_BOX_CASES = 256, CREATE_BOX_INPUTS = 11 };

static const char *const create_box_fields[] = {"name",
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

/* Controlled inputs to the unchanged native constructor. */
static void reference_create_box(unsigned sample, uint32_t output[11]) {
    float values[] = {(float)((int)(sample * 37 % 127) - 63) / 8.0f,
                      (float)((int)(sample * 43 % 127) - 63) / 16.0f,
                      (float)(sample % 17 + 1) / 8.0f,
                      (float)(sample % 19 + 1) / 8.0f,
                      sample % 4 ? (float)(sample % 13 + 1) / 8.0f : 0,
                      (float)(sample % 9) / 16.0f,
                      (float)(sample % 7) / 8.0f,
                      (float)((int)(sample % 31) - 15) / 4.0f};
    if (sample < 8) {
        values[0] = sample & 1 ? -0.0f : 0;
        values[1] = sample & 2 ? -0.0f : 0;
    }
    if (sample == 9)
        values[4] = -0.0f;
    if (sample >= 240) {
        values[2] = 1.5f;
        values[3] = 1.7f;
        values[4] = 0.001f;
    }
    for (unsigned index = 0; index < 8; ++index) {
        union {
            float value;
            uint32_t bits;
        } word = {.value = values[index]};
        output[index] = word.bits;
    }
    output[8] = (sample & 1) != 0;
    output[9] = (sample & 2) != 0;
    output[10] = (sample & 4) != 0;
}

#endif
