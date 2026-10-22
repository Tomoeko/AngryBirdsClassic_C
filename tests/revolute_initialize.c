#include "ab/physics_revolute.h"

#include "revolute_initialize_cases.h"
#include "revolute_native.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#if defined(AB_TEST_REVOLUTE_VELOCITY)
#define NATIVE_HEADER "AB_ANDROID_REVOLUTE_VELOCITY_V1 4096 8\n"
#define NATIVE_FOOTER "AB revolute velocity reference complete\n"
enum { METADATA_WORDS = 5 };
#else
#define NATIVE_HEADER "AB_ANDROID_REVOLUTE_INITIALIZE_V1 4096\n"
#define NATIVE_FOOTER "AB revolute initialization reference complete\n"
enum { METADATA_WORDS = 4 };
#endif

static bool edges(void) {
    AbRevoluteDefinition definition = {
        .lower_angle = -1, .upper_angle = 1, .enable_limit = true};
    AbJointSolverBody bodies[] = {
        {.inverse_mass = 1, .inverse_inertia = 1},
        {.angle = -1, .inverse_mass = 1, .inverse_inertia = 1}};
    AbJointStep step = {0x1.111112p-6f, 1, true};
    AbRevoluteState state = {
        .impulse = {0, 0, 3}, .motor_impulse = 4, .limit = AB_JOINT_LIMIT_LOWER};
    AbVelocity velocities[2] = {0};
    CHECK(ab_revolute_velocity_initialize(&definition, bodies, step, &state, velocities,
                                          NULL) &&
          state.limit == AB_JOINT_LIMIT_LOWER && state.impulse.z == 3 &&
          bits(state.motor_impulse) == 0 && state.motor_mass == 0.5f &&
          velocities[0].angular == -3 && velocities[1].angular == 3);
    bodies[1].angle = 1;
    CHECK(ab_revolute_velocity_initialize(&definition, bodies, step, &state, velocities,
                                          NULL) &&
          state.limit == AB_JOINT_LIMIT_UPPER && bits(state.impulse.z) == 0);
    definition.lower_angle = definition.upper_angle = 0;
    definition.enable_motor = true;
    state.impulse.z = 2;
    state.motor_impulse = 3;
    memset(velocities, 0, sizeof(velocities));
    CHECK(ab_revolute_velocity_initialize(&definition, bodies, step, &state, velocities,
                                          NULL) &&
          state.limit == AB_JOINT_LIMIT_EQUAL && state.impulse.z == 2 &&
          state.motor_impulse == 3 && velocities[0].angular == -5 &&
          velocities[1].angular == 5);
    definition.enable_limit = false;
    memset(velocities, 0, sizeof(velocities));
    CHECK(ab_revolute_velocity_initialize(&definition, bodies, step, &state, velocities,
                                          NULL) &&
          state.limit == AB_JOINT_LIMIT_INACTIVE && state.impulse.z == 2 &&
          state.motor_impulse == 3 && velocities[0].angular == -5 &&
          velocities[1].angular == 5);
    step.warm_start = false;
    memset(&state, 0xa5, sizeof(state));
    CHECK(ab_revolute_velocity_initialize(&definition, bodies, step, &state, velocities,
                                          NULL) &&
          bits(state.impulse.x) == 0 && bits(state.impulse.y) == 0 &&
          bits(state.impulse.z) == 0 && bits(state.motor_impulse) == 0);
    AbRevoluteState before = state;
    AbVelocity before_velocities[2];
    memcpy(before_velocities, velocities, sizeof(velocities));
    CHECK(!ab_revolute_velocity_initialize(NULL, bodies, step, &state, velocities,
                                           NULL) &&
          !ab_revolute_velocity_initialize(&definition, NULL, step, &state, velocities,
                                           NULL) &&
          !ab_revolute_velocity_initialize(&definition, bodies, step, NULL, velocities,
                                           NULL) &&
          !ab_revolute_velocity_initialize(&definition, bodies, step, &state, NULL,
                                           NULL));
    bodies[0].angle = FLT_MAX;
    CHECK(!ab_revolute_velocity_initialize(&definition, bodies, step, &state,
                                           velocities, NULL));
    bodies[0].angle = 0;
    definition.local_anchors[0].x = FLT_MAX;
    CHECK(!ab_revolute_velocity_initialize(&definition, bodies, step, &state,
                                           velocities, NULL));
    definition.local_anchors[0].x = 0;
    bodies[0].inverse_mass = -1;
    CHECK(!ab_revolute_velocity_initialize(&definition, bodies, step, &state,
                                           velocities, NULL) &&
          memcmp(&before, &state, sizeof(state)) == 0 &&
          memcmp(before_velocities, velocities, sizeof(velocities)) == 0);
    bodies[0].inverse_mass = 1;
    state.motor_impulse = NAN;
    before = state;
    step.warm_start = true;
    CHECK(!ab_revolute_velocity_initialize(&definition, bodies, step, &state,
                                           velocities, NULL) &&
          memcmp(&before, &state, sizeof(state)) == 0 &&
          memcmp(before_velocities, velocities, sizeof(velocities)) == 0);
    return true;
}

static bool velocity_edges(void) {
    AbRevoluteDefinition definition = {
        .motor_speed = 2, .maximum_motor_torque = 1, .enable_motor = true};
    AbJointSolverBody bodies[] = {{.inverse_mass = 1, .inverse_inertia = 1},
                                  {.inverse_mass = 1, .inverse_inertia = 1}};
    AbJointStep step = {0.5f, 1, false};
    AbRevoluteState state;
    AbVelocity velocities[2] = {0};
    CHECK(ab_revolute_velocity_initialize(&definition, bodies, step, &state, velocities,
                                          NULL) &&
          ab_revolute_velocity_solve(&definition, &state, step.seconds, velocities,
                                     NULL) &&
          state.motor_impulse == 0.5f && velocities[0].angular == -0.5f &&
          velocities[1].angular == 0.5f &&
          ab_revolute_velocity_solve(&definition, &state, step.seconds, velocities,
                                     NULL) &&
          state.motor_impulse == 0.5f && velocities[0].angular == -0.5f &&
          velocities[1].angular == 0.5f);
    definition.enable_motor = false;
    definition.enable_limit = true;
    definition.lower_angle = -1;
    definition.upper_angle = 0;
    memset(velocities, 0, sizeof(velocities));
    velocities[1].angular = 1;
    CHECK(ab_revolute_velocity_initialize(&definition, bodies, step, &state, velocities,
                                          NULL) &&
          state.limit == AB_JOINT_LIMIT_UPPER &&
          ab_revolute_velocity_solve(&definition, &state, step.seconds, velocities,
                                     NULL) &&
          state.impulse.z == -0.5f && velocities[0].angular == 0.5f &&
          velocities[1].angular == 0.5f);
    definition.lower_angle = 0;
    definition.upper_angle = 1;
    memset(velocities, 0, sizeof(velocities));
    velocities[1].angular = 1;
    CHECK(ab_revolute_velocity_initialize(&definition, bodies, step, &state, velocities,
                                          NULL) &&
          state.limit == AB_JOINT_LIMIT_LOWER &&
          ab_revolute_velocity_solve(&definition, &state, step.seconds, velocities,
                                     NULL) &&
          state.impulse.z == 0 && velocities[0].angular == 0 &&
          velocities[1].angular == 1);
    AbRevoluteState before = state;
    AbVelocity before_velocities[2];
    memcpy(before_velocities, velocities, sizeof(velocities));
    CHECK(!ab_revolute_velocity_solve(NULL, &state, step.seconds, velocities, NULL) &&
          !ab_revolute_velocity_solve(&definition, NULL, step.seconds, velocities,
                                      NULL) &&
          !ab_revolute_velocity_solve(&definition, &state, step.seconds, NULL, NULL) &&
          !ab_revolute_velocity_solve(&definition, &state, -1, velocities, NULL) &&
          memcmp(&before, &state, sizeof(state)) == 0 &&
          memcmp(before_velocities, velocities, sizeof(velocities)) == 0);
    state.matrix.xx = NAN;
    before = state;
    CHECK(!ab_revolute_velocity_solve(&definition, &state, step.seconds, velocities,
                                      NULL) &&
          memcmp(&before, &state, sizeof(state)) == 0 &&
          memcmp(before_velocities, velocities, sizeof(velocities)) == 0);
    return true;
}

static bool compare_case(FILE *file, const uint32_t input[32], uint32_t sample,
                         uint32_t iterations) {
    float values[31];
    memcpy(values, input, sizeof(values));
    AbJointSolverBody bodies[2];
    for (size_t body = 0; body < 2; ++body) {
        size_t index = body * 5;
        bodies[body] = (AbJointSolverBody){{values[index], values[index + 1]},
                                           values[index + 2],
                                           values[index + 3],
                                           values[index + 4]};
    }
    AbRevoluteDefinition definition = {
        .local_anchors = {{values[10], values[11]}, {values[12], values[13]}},
        .reference_angle = values[14],
        .lower_angle = values[15],
        .upper_angle = values[16],
        .motor_speed = values[30],
        .maximum_motor_torque = values[29],
        .enable_motor = ((sample >> 1) & 1u) != 0,
        .enable_limit = ((sample >> 2) & 1u) != 0,
    };
    AbJointStep step = {values[17], values[18], (sample & 1u) != 0};
    AbRevoluteState state = {.impulse = {values[19], values[20], values[21]},
                             .motor_impulse = values[28],
                             .limit = (AbJointLimitState)input[31]};
    AbVelocity velocities[] = {{{values[22], values[23]}, values[24]},
                               {{values[25], values[26]}, values[27]}};
    AbError error = {0};
    if (!ab_revolute_velocity_initialize(&definition, bodies, step, &state, velocities,
                                         &error)) {
        fprintf(stderr, "Revolute sample %" PRIu32 " rejected: %s\n", sample,
                error.message);
        return false;
    }
    AbRevoluteState preserved = state;
    for (uint32_t pass = 0; pass <= iterations; ++pass) {
        if (pass && !ab_revolute_velocity_solve(&definition, &state, step.seconds,
                                                velocities, &error)) {
            fprintf(stderr,
                    "Revolute sample %" PRIu32 " velocity pass %" PRIu32 ": %s\n",
                    sample, pass, error.message);
            return false;
        }
        CHECK(compare_state(file, state, velocities, sample, pass));
        preserved.impulse = state.impulse;
        preserved.motor_impulse = state.motor_impulse;
        CHECK(memcmp(&preserved, &state, sizeof(state)) == 0);
    }
    return true;
}

static bool native_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) && strcmp(line, NATIVE_HEADER) == 0);
    size_t iterations = 0;
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t metadata[5] = {0}, input[32], generated[32];
        CHECK(words(file, metadata, METADATA_WORDS) && metadata[0] == sample &&
              metadata[1] == (sample & 1u) && metadata[2] == ((sample >> 1) & 1u) &&
              metadata[3] == ((sample >> 2) & 1u) && words(file, input, 32));
        CHECK(metadata[4] == (METADATA_WORDS == 5 ? sample % 9 : 0));
        iterations += metadata[4];
        revolute_initialize_case(sample, generated);
        CHECK(memcmp(input, generated, sizeof(input)) == 0 &&
              compare_case(file, input, sample, metadata[4]));
    }
    CHECK(fgets(line, sizeof(line), file) && strcmp(line, NATIVE_FOOTER) == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Compared 4,096 native revolute initializations and %zu velocity passes "
           "(%zu float words and %zu limit states)\n",
           iterations, (4096 + iterations) * 32, 4096 + iterations);
    return true;
}

int main(int argc, char **argv) {
    if (!edges() || !velocity_edges()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    if (argc != 2) {
        fputs("Usage: ab_revolute_initialize_tests [NATIVE_CORPUS]\n", stderr);
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    bool okay = file && native_checks(file);
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
