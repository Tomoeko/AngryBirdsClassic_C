#include "ab/physics_weld.h"

#include "weld_initialize_cases.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#if defined(AB_TEST_WELD_VELOCITY)
#define NATIVE_HEADER "AB_ANDROID_WELD_VELOCITY_V1 4096 8\n"
#define NATIVE_FOOTER "AB weld velocity reference complete\n"
enum { METADATA_WORDS = 3 };
#else
#define NATIVE_HEADER "AB_ANDROID_WELD_INITIALIZE_V1 4096\n"
#define NATIVE_FOOTER "AB weld initialization reference complete\n"
enum { METADATA_WORDS = 2 };
#endif

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Weld initialization failed at %d: %s\n", __LINE__,        \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool edges(void) {
    AbWeldDefinition definition = {0};
    AbJointSolverBody bodies[] = {{.inverse_mass = 1}, {.inverse_mass = 2}};
    AbWeldState state = {.impulse = {1, 2, 3}};
    AbVelocity velocities[] = {{{1, -1}, 0}, {{0, 2}, 0}};
    AbJointStep step = {0x1.111112p-5f, 0.5f, true};
    CHECK(ab_weld_velocity_initialize(&definition, bodies, step, &state, velocities,
                                      NULL) &&
          state.impulse.x == 0.5f && state.impulse.y == 1 && state.impulse.z == 1.5f &&
          velocities[0].linear.x == 0.5f && velocities[0].linear.y == -2 &&
          velocities[1].linear.x == 1 && velocities[1].linear.y == 4 &&
          bits(state.gamma) == 0 && bits(state.bias) == 0 &&
          bits(state.inverse.zz) == 0);
    definition.frequency = 4;
    definition.damping_ratio = 0.5f;
    bodies[0].inverse_inertia = 1;
    bodies[1].inverse_inertia = 2;
    AbVelocity before_velocities[2];
    memcpy(before_velocities, velocities, sizeof(velocities));
    step.warm_start = false;
    memset(&state, 0xa5, sizeof(state));
    CHECK(ab_weld_velocity_initialize(&definition, bodies, step, &state, velocities,
                                      NULL) &&
          bits(state.impulse.x) == 0 && bits(state.impulse.y) == 0 &&
          bits(state.impulse.z) == 0 && state.gamma > 0 && state.inverse.zz > 0 &&
          state.bias == 0 &&
          memcmp(before_velocities, velocities, sizeof(velocities)) == 0);
    state.impulse.x = NAN;
    AbWeldState before = state;
    step.warm_start = true;
    CHECK(!ab_weld_velocity_initialize(&definition, bodies, step, &state, velocities,
                                       NULL));
    step.warm_start = false;
    CHECK(!ab_weld_velocity_initialize(NULL, bodies, step, &state, velocities, NULL) &&
          !ab_weld_velocity_initialize(&definition, NULL, step, &state, velocities,
                                       NULL) &&
          !ab_weld_velocity_initialize(&definition, bodies, step, NULL, velocities,
                                       NULL) &&
          !ab_weld_velocity_initialize(&definition, bodies, step, &state, NULL, NULL));
    bodies[0].angle = FLT_MAX;
    CHECK(!ab_weld_velocity_initialize(&definition, bodies, step, &state, velocities,
                                       NULL));
    bodies[0].angle = 0;
    bodies[0].inverse_mass = -1;
    CHECK(!ab_weld_velocity_initialize(&definition, bodies, step, &state, velocities,
                                       NULL));
    bodies[0].inverse_mass = 1;
    step.seconds = -1;
    CHECK(!ab_weld_velocity_initialize(&definition, bodies, step, &state, velocities,
                                       NULL));
    step.seconds = 0x1.111112p-5f;
    definition.frequency = FLT_MAX;
    CHECK(!ab_weld_velocity_initialize(&definition, bodies, step, &state, velocities,
                                       NULL) &&
          memcmp(&state, &before, sizeof(state)) == 0 &&
          memcmp(velocities, before_velocities, sizeof(velocities)) == 0);
    return true;
}

static bool velocity_edges(void) {
    AbWeldDefinition definition = {0};
    AbJointSolverBody bodies[] = {{.inverse_mass = 1, .inverse_inertia = 1},
                                  {.inverse_mass = 1, .inverse_inertia = 1}};
    AbWeldState state;
    AbVelocity velocities[] = {{{1, 0}, 0}, {{-1, 0}, 0}};
    AbJointStep step = {0x1.111112p-5f, 1, false};
    CHECK(ab_weld_velocity_initialize(&definition, bodies, step, &state, velocities,
                                      NULL) &&
          ab_weld_velocity_solve(&definition, &state, velocities, NULL) &&
          velocities[0].linear.x == 0 && velocities[1].linear.x == 0 &&
          state.impulse.x == 1);
    definition.frequency = 4;
    definition.damping_ratio = 0.5f;
    velocities[0].angular = 1;
    velocities[1].angular = -1;
    CHECK(ab_weld_velocity_initialize(&definition, bodies, step, &state, velocities,
                                      NULL) &&
          ab_weld_velocity_solve(&definition, &state, velocities, NULL) &&
          velocities[0].angular < 1 && velocities[1].angular > -1 &&
          state.impulse.z != 0);
    AbWeldState before = state;
    AbVelocity before_velocities[2];
    memcpy(before_velocities, velocities, sizeof(velocities));
    CHECK(!ab_weld_velocity_solve(NULL, &state, velocities, NULL) &&
          !ab_weld_velocity_solve(&definition, NULL, velocities, NULL) &&
          !ab_weld_velocity_solve(&definition, &state, NULL, NULL) &&
          memcmp(&state, &before, sizeof(state)) == 0 &&
          memcmp(velocities, before_velocities, sizeof(velocities)) == 0);
    state.inverse.xx = NAN;
    before = state;
    CHECK(!ab_weld_velocity_solve(&definition, &state, velocities, NULL) &&
          memcmp(&state, &before, sizeof(state)) == 0 &&
          memcmp(velocities, before_velocities, sizeof(velocities)) == 0);
    state.inverse.xx = FLT_MAX;
    definition.frequency = 0;
    velocities[0].linear.x = FLT_MAX;
    velocities[1].linear.x = -FLT_MAX;
    before = state;
    memcpy(before_velocities, velocities, sizeof(velocities));
    CHECK(!ab_weld_velocity_solve(&definition, &state, velocities, NULL) &&
          memcmp(&state, &before, sizeof(state)) == 0 &&
          memcmp(velocities, before_velocities, sizeof(velocities)) == 0);
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool compare_case(const uint32_t encoded[28], FILE *file, uint32_t sample,
                         bool warm, uint32_t iterations) {
    float values[28];
    for (size_t index = 0; index < 28; ++index) {
        memcpy(values + index, encoded + index, sizeof(float));
    }
    AbJointSolverBody bodies[2];
    for (size_t body = 0; body < 2; ++body) {
        size_t index = body * 5;
        bodies[body] = (AbJointSolverBody){{values[index], values[index + 1]},
                                           values[index + 2],
                                           values[index + 3],
                                           values[index + 4]};
    }
    AbWeldDefinition definition = {{{values[10], values[11]}, {values[12], values[13]}},
                                   values[14],
                                   values[15],
                                   values[16]};
    AbJointStep step = {values[17], values[18], warm};
    AbWeldState state = {.impulse = {values[19], values[20], values[21]}};
    AbVelocity velocities[] = {{{values[22], values[23]}, values[24]},
                               {{values[25], values[26]}, values[27]}};
    AbError error = {0};
    if (!ab_weld_velocity_initialize(&definition, bodies, step, &state, velocities,
                                     &error)) {
        fprintf(stderr, "Weld sample %" PRIu32 " rejected: %s\n", sample,
                error.message);
        return false;
    }
    for (uint32_t iteration = 0; iteration <= iterations; ++iteration) {
        if (iteration &&
            !ab_weld_velocity_solve(&definition, &state, velocities, &error)) {
            fprintf(stderr, "Weld sample %" PRIu32 " iteration %" PRIu32 ": %s\n",
                    sample, iteration, error.message);
            return false;
        }
        for (size_t body = 0; body < 2; ++body) {
            size_t index = body * 5;
            CHECK(bits(state.local_centers[body].x) == encoded[index] &&
                  bits(state.local_centers[body].y) == encoded[index + 1] &&
                  bits(state.inverse_mass[body]) == encoded[index + 3] &&
                  bits(state.inverse_inertia[body]) == encoded[index + 4]);
        }
        uint32_t expected[24];
        CHECK(words(file, expected, 24));
        const float actual[] = {
            state.offsets[0].x,
            state.offsets[0].y,
            state.offsets[1].x,
            state.offsets[1].y,
            state.inverse.xx,
            state.inverse.xy,
            state.inverse.xz,
            state.inverse.xy,
            state.inverse.yy,
            state.inverse.yz,
            state.inverse.xz,
            state.inverse.yz,
            state.inverse.zz,
            state.gamma,
            state.bias,
            state.impulse.x,
            state.impulse.y,
            state.impulse.z,
            velocities[0].linear.x,
            velocities[0].linear.y,
            velocities[0].angular,
            velocities[1].linear.x,
            velocities[1].linear.y,
            velocities[1].angular,
        };
        for (size_t index = 0; index < 24; ++index) {
            if (bits(actual[index]) != expected[index]) {
                fprintf(stderr,
                        "Weld sample %" PRIu32 " iteration %" PRIu32 " field %zu: "
                        "%08" PRIx32 " != %08" PRIx32 "\n",
                        sample, iteration, index, bits(actual[index]), expected[index]);
                return false;
            }
        }
    }
    return true;
}

static bool native_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) && strcmp(line, NATIVE_HEADER) == 0);
    size_t iteration_count = 0;
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t metadata[3] = {0}, input[28], generated[28];
        CHECK(words(file, metadata, METADATA_WORDS) && metadata[0] == sample &&
              metadata[1] == (sample & 1u) && words(file, input, 28));
        uint32_t iterations = METADATA_WORDS == 3 ? metadata[2] : 0;
        CHECK(iterations == (METADATA_WORDS == 3 ? sample % 9 : 0));
        weld_initialize_case(sample, generated);
        CHECK(memcmp(input, generated, sizeof(input)) == 0 &&
              compare_case(input, file, sample, metadata[1] != 0, iterations));
        iteration_count += iterations;
    }
    CHECK(fgets(line, sizeof(line), file) && strcmp(line, NATIVE_FOOTER) == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Compared 4,096 native weld initializations, %zu velocity iterations and "
           "%zu float words\n",
           iteration_count, (4096 + iteration_count) * 24);
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
#if defined(AB_TEST_WELD_VELOCITY)
        fputs("Usage: ab_weld_velocity_tests [NATIVE_CORPUS]\n", stderr);
#else
        fputs("Usage: ab_weld_initialize_tests [NATIVE_CORPUS]\n", stderr);
#endif
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    bool okay = file && native_checks(file);
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
