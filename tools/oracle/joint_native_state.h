#ifndef AB_ORACLE_JOINT_NATIVE_STATE_H
#define AB_ORACLE_JOINT_NATIVE_STATE_H

#include "../../tests/joint_state_projection.h"

static void prepare_joint_definition(const uint32_t input[41], uint32_t sample,
                                     uint32_t index, const uint32_t *body_pointers,
                                     uint32_t definition[18]) {
    uint32_t kind = input[0];
    const uint32_t *data = input + 3;
    definition[0] = (const uint32_t[]){3, 8, 1, 2}[kind];
    definition[2] = body_pointers[input[1]];
    definition[3] = body_pointers[input[2]];
    for (size_t field = 0; field < 4; ++field) {
        definition[5 + field] = data[10 + field];
    }
    uint32_t motor = (sample >> (index + 1)) & 1u;
    uint32_t limit = (sample >> (index + 3)) & 1u;
    if (kind < 2) {
        definition[9] = data[14];
        definition[10] = data[15];
        definition[11] = data[16];
    } else if (kind == 2) {
        definition[9] = data[14];
        definition[10] = limit;
        definition[11] = data[15];
        definition[12] = data[16];
        definition[13] = motor;
        definition[14] = data[30];
        definition[15] = data[29];
    } else {
        definition[9] = data[32];
        definition[10] = data[33];
        definition[11] = data[14];
        definition[12] = limit;
        definition[13] = data[15];
        definition[14] = data[16];
        definition[15] = motor;
        definition[16] = data[29];
        definition[17] = data[30];
    }
}

static void print_joint_island_state(uint32_t kind, const uint32_t *joint,
                                     bool initialized) {
    uint32_t output[37] = {0};
    size_t centers = kind < 2 ? 35 : 38;
    for (size_t field = 0; field < 8; ++field) {
        output[field] = joint[centers + field];
    }
    if (kind == 0) {
        for (size_t field = 0; field < 4; ++field) {
            output[8 + field] = joint[31 + field];
        }
        output[12] = joint[29];
        output[13] = joint[30];
        output[14] = joint[43];
        output[15] = joint[24];
        output[16] = joint[19];
        output[17] = joint[25];
    } else if (kind == 1) {
        for (size_t field = 0; field < 4; ++field) {
            output[8 + field] = joint[31 + field];
        }
        for (size_t field = 0; field < 9; ++field) {
            output[12 + field] = joint[43 + field];
        }
        for (size_t field = 0; field < 3; ++field) {
            output[21 + field] = joint[26 + field];
        }
        output[24] = joint[25];
        output[25] = joint[19];
    } else if (kind == 2) {
        for (size_t field = 0; field < 4; ++field) {
            output[8 + field] = joint[34 + field];
            output[22 + field] = joint[21 + field];
        }
        for (size_t field = 0; field < 9; ++field) {
            output[12 + field] = joint[46 + field];
        }
        output[21] = joint[55];
        output[36] = joint[56];
    } else {
        for (size_t field = 0; field < 8; ++field) {
            output[8 + field] = joint[46 + field];
        }
        for (size_t field = 0; field < 9; ++field) {
            output[16 + field] = joint[54 + field];
        }
        for (size_t field = 0; field < 4; ++field) {
            output[26 + field] = joint[26 + field];
            output[30 + field] = joint[21 + field];
        }
        output[25] = joint[63];
        output[36] = joint[35];
    }
    if (!initialized) {
        joint_state_mask_uninitialized(kind, output);
    }
    print_words(output, 37);
}

#endif
