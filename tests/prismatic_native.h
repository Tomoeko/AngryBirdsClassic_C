#ifndef AB_TEST_PRISMATIC_NATIVE_H
#define AB_TEST_PRISMATIC_NATIVE_H

#include "ab/physics_prismatic.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Prismatic check failed at %d: %s\n", __LINE__,            \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t bits(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool compare_state(FILE *file, AbPrismaticAxes local, AbPrismaticState state,
                          const AbVelocity velocities[2], uint32_t sample,
                          uint32_t pass) {
    const float actual[] = {
        local.axis.x,
        local.axis.y,
        local.perpendicular.x,
        local.perpendicular.y,
        state.local_centers[0].x,
        state.local_centers[0].y,
        state.local_centers[1].x,
        state.local_centers[1].y,
        state.inverse_mass[0],
        state.inverse_mass[1],
        state.inverse_inertia[0],
        state.inverse_inertia[1],
        state.axis.x,
        state.axis.y,
        state.perpendicular.x,
        state.perpendicular.y,
        state.perpendicular_arms[0],
        state.perpendicular_arms[1],
        state.axis_arms[0],
        state.axis_arms[1],
        state.matrix.xx,
        state.matrix.xy,
        state.matrix.xz,
        state.matrix.xy,
        state.matrix.yy,
        state.matrix.yz,
        state.matrix.xz,
        state.matrix.yz,
        state.matrix.zz,
        state.motor_mass,
        state.impulse.x,
        state.impulse.y,
        state.impulse.z,
        state.motor_impulse,
        velocities[0].linear.x,
        velocities[0].linear.y,
        velocities[0].angular,
        velocities[1].linear.x,
        velocities[1].linear.y,
        velocities[1].angular,
    };
    uint32_t expected[41];
    CHECK(words(file, expected, 41));
    for (size_t field = 0; field < 41; ++field) {
        uint32_t word = field < 40 ? bits(actual[field]) : (uint32_t)state.limit;
        if (word != expected[field]) {
            fprintf(stderr,
                    "Prismatic sample %" PRIu32 " pass %" PRIu32
                    " field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    sample, pass, field, word, expected[field]);
            return false;
        }
    }
    return true;
}
#endif
