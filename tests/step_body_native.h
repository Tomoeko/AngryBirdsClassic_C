#ifndef AB_TEST_STEP_BODY_NATIVE_H
#define AB_TEST_STEP_BODY_NATIVE_H

#include "ab/physics_step.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition);          \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float from_bits(uint32_t bits) {
    float number;
    memcpy(&number, &bits, sizeof(number));
    return number;
}

static uint32_t to_bits(float number) {
    uint32_t bits;
    memcpy(&bits, &number, sizeof(bits));
    return bits;
}

static AbBodyMotion read_body(const uint32_t words[64]) {
    return (AbBodyMotion){
        .mass =
            {
                .type = (AbBodyType)words[0],
                .fixed_rotation = (words[1] & 16) != 0,
                .transform = {{from_bits(words[3]), from_bits(words[4])},
                              from_bits(words[5]),
                              from_bits(words[6])},
                .local_center = {from_bits(words[7]), from_bits(words[8])},
                .previous_center = {from_bits(words[9]), from_bits(words[10])},
                .center = {from_bits(words[11]), from_bits(words[12])},
                .previous_angle = from_bits(words[13]),
                .angle = from_bits(words[14]),
                .linear_velocity = {from_bits(words[16]), from_bits(words[17])},
                .angular_velocity = from_bits(words[18]),
                .mass = from_bits(words[29]),
                .inverse_mass = from_bits(words[30]),
                .inertia = from_bits(words[31]),
                .inverse_inertia = from_bits(words[32]),
            },
        .force = {from_bits(words[19]), from_bits(words[20])},
        .torque = from_bits(words[21]),
        .linear_damping = from_bits(words[33]),
        .angular_damping = from_bits(words[34]),
        .gravity_scale = from_bits(words[35]),
        .sleep_time = from_bits(words[36]),
        .awake = (words[1] & 2) != 0,
        .sleep_allowed = (words[1] & 4) != 0,
        .bullet = (words[1] & 8) != 0,
        .active = (words[1] & 32) != 0,
        .user_data = words[37],
    };
}

static void write_body(const AbBodyMotion *body, uint32_t words[64]) {
    const AbBodyMassState *mass = &body->mass;
    words[1] = (words[1] & ~UINT32_C(2)) | (body->awake ? 2u : 0u);
    const float transform[] = {
        mass->transform.position.x,
        mass->transform.position.y,
        mass->transform.sine,
        mass->transform.cosine,
        mass->local_center.x,
        mass->local_center.y,
        mass->previous_center.x,
        mass->previous_center.y,
        mass->center.x,
        mass->center.y,
        mass->previous_angle,
        mass->angle,
    };
    for (size_t field = 0; field < 12; ++field) {
        words[3 + field] = to_bits(transform[field]);
    }
    const float motion[] = {mass->linear_velocity.x, mass->linear_velocity.y,
                            mass->angular_velocity,  body->force.x,
                            body->force.y,           body->torque};
    for (size_t field = 0; field < 6; ++field) {
        words[16 + field] = to_bits(motion[field]);
    }
    words[36] = to_bits(body->sleep_time);
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, words + field) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

#endif
