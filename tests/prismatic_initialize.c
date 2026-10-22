#include "ab/physics_prismatic.h"

#include "prismatic_initialize_cases.h"
#include "prismatic_native.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#if defined(AB_TEST_PRISMATIC_VELOCITY)
#define NATIVE_HEADER "AB_ANDROID_PRISMATIC_VELOCITY_V1 4096 8\n"
#define NATIVE_FOOTER "AB prismatic velocity reference complete\n"
enum { METADATA_WORDS = 5 };
#else
#define NATIVE_HEADER "AB_ANDROID_PRISMATIC_INITIALIZE_V1 4096\n"
#define NATIVE_FOOTER "AB prismatic initialization reference complete\n"
enum { METADATA_WORDS = 4 };
#endif

static bool edges(void) {
    AbPrismaticAxes axes;
    CHECK(ab_prismatic_axes_prepare((AbVec2){3, 4}, &axes, NULL) &&
          axes.axis.x == 0.6f && axes.axis.y == 0.8f && axes.perpendicular.x == -0.8f &&
          axes.perpendicular.y == 0.6f);
    float tiny;
    uint32_t word = 1;
    memcpy(&tiny, &word, sizeof(tiny));
    CHECK(ab_prismatic_axes_prepare((AbVec2){tiny, -0.0f}, &axes, NULL) &&
          bits(axes.axis.x) == 1 && bits(axes.axis.y) == UINT32_C(0x80000000) &&
          bits(axes.perpendicular.x) == 0 && bits(axes.perpendicular.y) == 1);
    AbPrismaticAxes before_axes = axes;
    CHECK(!ab_prismatic_axes_prepare((AbVec2){1, 0}, NULL, NULL) &&
          !ab_prismatic_axes_prepare((AbVec2){FLT_MAX, 0}, &axes, NULL) &&
          !ab_prismatic_axes_prepare((AbVec2){NAN, 0}, &axes, NULL) &&
          memcmp(&axes, &before_axes, sizeof(axes)) == 0);
    AbPrismaticDefinition definition = {.enable_motor = true};
    CHECK(ab_prismatic_axes_prepare((AbVec2){1, 0}, &definition.local, NULL));
    AbJointSolverBody bodies[] = {{.inverse_mass = 1, .inverse_inertia = 1},
                                  {.inverse_mass = 1, .inverse_inertia = 1}};
    AbVec2 centers[] = {{0, 0}, {2, 0}};
    AbJointStep step = {0x1.111112p-6f, 0.5f, true};
    AbPrismaticState state = {.impulse = {1, 2, 3}, .motor_impulse = 4};
    AbVelocity velocities[2] = {0};
    CHECK(ab_prismatic_velocity_initialize(&definition, bodies, centers, step, &state,
                                           velocities, NULL) &&
          state.limit == AB_JOINT_LIMIT_INACTIVE && state.impulse.x == 0.5f &&
          state.impulse.y == 1 && bits(state.impulse.z) == 0 &&
          state.motor_impulse == 2 && state.motor_mass == 0.5f &&
          state.matrix.xx == 6 && state.matrix.xy == 2 && state.matrix.yy == 2 &&
          state.matrix.zz == 2 && velocities[0].linear.x == -2 &&
          velocities[0].linear.y == -0.5f && velocities[0].angular == -2 &&
          velocities[1].linear.x == 2 && velocities[1].linear.y == 0.5f &&
          velocities[1].angular == 1);
    bodies[0].inverse_inertia = bodies[1].inverse_inertia = 0;
    step.warm_start = false;
    memset(&state, 0xa5, sizeof(state));
    CHECK(ab_prismatic_velocity_initialize(&definition, bodies, centers, step, &state,
                                           velocities, NULL) &&
          state.matrix.yy == 1 && bits(state.impulse.x) == 0 &&
          bits(state.impulse.y) == 0 && bits(state.impulse.z) == 0 &&
          bits(state.motor_impulse) == 0);
    AbPrismaticState before = state;
    AbVelocity before_velocities[2];
    memcpy(before_velocities, velocities, sizeof(velocities));
    CHECK(!ab_prismatic_velocity_initialize(NULL, bodies, centers, step, &state,
                                            velocities, NULL) &&
          !ab_prismatic_velocity_initialize(&definition, NULL, centers, step, &state,
                                            velocities, NULL) &&
          !ab_prismatic_velocity_initialize(&definition, bodies, NULL, step, &state,
                                            velocities, NULL) &&
          !ab_prismatic_velocity_initialize(&definition, bodies, centers, step, NULL,
                                            velocities, NULL) &&
          !ab_prismatic_velocity_initialize(&definition, bodies, centers, step, &state,
                                            NULL, NULL));
    bodies[0].angle = FLT_MAX;
    CHECK(!ab_prismatic_velocity_initialize(&definition, bodies, centers, step, &state,
                                            velocities, NULL));
    bodies[0].angle = 0;
    bodies[0].inverse_mass = -1;
    CHECK(!ab_prismatic_velocity_initialize(&definition, bodies, centers, step, &state,
                                            velocities, NULL) &&
          memcmp(&before, &state, sizeof(state)) == 0 &&
          memcmp(before_velocities, velocities, sizeof(velocities)) == 0);
    bodies[0].inverse_mass = 1;
    state.impulse.x = NAN;
    before = state;
    step.warm_start = true;
    CHECK(!ab_prismatic_velocity_initialize(&definition, bodies, centers, step, &state,
                                            velocities, NULL) &&
          memcmp(&before, &state, sizeof(state)) == 0 &&
          memcmp(before_velocities, velocities, sizeof(velocities)) == 0);
    return true;
}

static bool velocity_edges(void) {
    AbPrismaticDefinition definition = {
        .enable_motor = true, .motor_speed = 2, .maximum_motor_force = 1};
    CHECK(ab_prismatic_axes_prepare((AbVec2){1, 0}, &definition.local, NULL));
    AbJointSolverBody bodies[] = {{.inverse_mass = 1}, {.inverse_mass = 1}};
    AbVec2 centers[2] = {0};
    AbJointStep step = {0.5f, 1, false};
    AbPrismaticState state;
    AbVelocity velocities[2] = {0};
    CHECK(ab_prismatic_velocity_initialize(&definition, bodies, centers, step, &state,
                                           velocities, NULL) &&
          ab_prismatic_velocity_solve(&definition, &state, step.seconds, velocities,
                                      NULL) &&
          state.motor_impulse == 0.5f && velocities[0].linear.x == -0.5f &&
          velocities[1].linear.x == 0.5f &&
          ab_prismatic_velocity_solve(&definition, &state, step.seconds, velocities,
                                      NULL) &&
          state.motor_impulse == 0.5f && velocities[0].linear.x == -0.5f &&
          velocities[1].linear.x == 0.5f);
    definition.enable_motor = false;
    definition.enable_limit = true;
    definition.lower_translation = -1;
    definition.upper_translation = 0;
    memset(velocities, 0, sizeof(velocities));
    velocities[1].linear.x = 1;
    CHECK(ab_prismatic_velocity_initialize(&definition, bodies, centers, step, &state,
                                           velocities, NULL) &&
          state.limit == AB_JOINT_LIMIT_UPPER &&
          ab_prismatic_velocity_solve(&definition, &state, step.seconds, velocities,
                                      NULL) &&
          state.impulse.z == -0.5f && velocities[0].linear.x == 0.5f &&
          velocities[1].linear.x == 0.5f);
    definition.lower_translation = 0;
    definition.upper_translation = 1;
    memset(velocities, 0, sizeof(velocities));
    velocities[1].linear.x = 1;
    CHECK(ab_prismatic_velocity_initialize(&definition, bodies, centers, step, &state,
                                           velocities, NULL) &&
          state.limit == AB_JOINT_LIMIT_LOWER &&
          ab_prismatic_velocity_solve(&definition, &state, step.seconds, velocities,
                                      NULL) &&
          bits(state.impulse.z) == 0 && velocities[0].linear.x == 0 &&
          velocities[1].linear.x == 1);
    AbPrismaticState before = state;
    AbVelocity before_velocities[2];
    memcpy(before_velocities, velocities, sizeof(velocities));
    CHECK(!ab_prismatic_velocity_solve(NULL, &state, step.seconds, velocities, NULL) &&
          !ab_prismatic_velocity_solve(&definition, NULL, step.seconds, velocities,
                                       NULL) &&
          !ab_prismatic_velocity_solve(&definition, &state, step.seconds, NULL, NULL) &&
          !ab_prismatic_velocity_solve(&definition, &state, -1, velocities, NULL) &&
          memcmp(&before, &state, sizeof(state)) == 0 &&
          memcmp(before_velocities, velocities, sizeof(velocities)) == 0);
    state.axis.x = NAN;
    before = state;
    CHECK(!ab_prismatic_velocity_solve(&definition, &state, step.seconds, velocities,
                                       NULL) &&
          memcmp(&before, &state, sizeof(state)) == 0 &&
          memcmp(before_velocities, velocities, sizeof(velocities)) == 0);
    return true;
}

static bool compare_case(FILE *file, const uint32_t input[38], uint32_t sample,
                         uint32_t iterations) {
    float values[38];
    memcpy(values, input, sizeof(values));
    AbJointSolverBody bodies[2];
    for (size_t body = 0; body < 2; ++body) {
        size_t index = body * 5;
        bodies[body] = (AbJointSolverBody){{values[index], values[index + 1]},
                                           values[index + 2],
                                           values[index + 3],
                                           values[index + 4]};
    }
    AbPrismaticDefinition definition = {
        .local_anchors = {{values[10], values[11]}, {values[12], values[13]}},
        .reference_angle = values[14],
        .lower_translation = values[15],
        .upper_translation = values[16],
        .motor_speed = values[30],
        .maximum_motor_force = values[29],
        .enable_motor = ((sample >> 1) & 1u) != 0,
        .enable_limit = ((sample >> 2) & 1u) != 0,
    };
    AbError error = {0};
    CHECK(ab_prismatic_axes_prepare((AbVec2){values[32], values[33]}, &definition.local,
                                    &error));
    AbVec2 centers[] = {{values[34], values[35]}, {values[36], values[37]}};
    AbJointStep step = {values[17], values[18], (sample & 1u) != 0};
    AbPrismaticState state = {.impulse = {values[19], values[20], values[21]},
                              .motor_impulse = values[28],
                              .limit = (AbJointLimitState)input[31]};
    AbVelocity velocities[] = {{{values[22], values[23]}, values[24]},
                               {{values[25], values[26]}, values[27]}};
    if (!ab_prismatic_velocity_initialize(&definition, bodies, centers, step, &state,
                                          velocities, &error)) {
        fprintf(stderr, "Prismatic sample %" PRIu32 " rejected: %s\n", sample,
                error.message);
        return false;
    }
    AbPrismaticState preserved = state;
    for (uint32_t pass = 0; pass <= iterations; ++pass) {
        if (pass && !ab_prismatic_velocity_solve(&definition, &state, step.seconds,
                                                 velocities, &error)) {
            fprintf(stderr,
                    "Prismatic sample %" PRIu32 " velocity pass %" PRIu32 ": %s\n",
                    sample, pass, error.message);
            return false;
        }
        CHECK(compare_state(file, definition.local, state, velocities, sample, pass));
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
        uint32_t metadata[5] = {0}, input[38], generated[38];
        CHECK(words(file, metadata, METADATA_WORDS) && metadata[0] == sample &&
              metadata[1] == (sample & 1u) && metadata[2] == ((sample >> 1) & 1u) &&
              metadata[3] == ((sample >> 2) & 1u) && words(file, input, 38));
        CHECK(metadata[4] == (METADATA_WORDS == 5 ? sample % 9 : 0));
        iterations += metadata[4];
        prismatic_initialize_case(sample, generated);
        CHECK(memcmp(input, generated, sizeof(input)) == 0 &&
              compare_case(file, input, sample, metadata[4]));
    }
    CHECK(fgets(line, sizeof(line), file) && strcmp(line, NATIVE_FOOTER) == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Compared 4,096 native prismatic initializations and %zu velocity passes "
           "(%zu float words and %zu limit states)\n",
           iterations, (4096 + iterations) * 40, 4096 + iterations);
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
        fputs("Usage: ab_prismatic_initialize_tests [NATIVE_CORPUS]\n", stderr);
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    bool okay = file && native_checks(file);
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
