#include "ab/physics_distance_joint.h"

#include "distance_cases.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Distance failed at %d: %s\n", __LINE__, #condition);      \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t bits(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

static bool edges(void) {
    AbDistanceDefinition definition = {.length = 1};
    AbJointSolverBody bodies[] = {{.inverse_mass = 1, .inverse_inertia = 1},
                                  {.inverse_mass = 1, .inverse_inertia = 1}};
    AbVec2 centers[] = {{0, 0}, {2, 0}};
    AbJointStep step = {0x1.111112p-6f, 1, false};
    AbDistanceState state;
    memset(&state, 0xa5, sizeof(state));
    AbVelocity velocities[] = {{{1, 0}, 0}, {{-1, 0}, 0}};
    CHECK(ab_distance_velocity_initialize(&definition, bodies, centers, step, &state,
                                          velocities, NULL) &&
          state.axis.x == 1 && bits(state.axis.y) == 0 && state.mass == 0.5f &&
          bits(state.impulse) == 0 &&
          ab_distance_velocity_solve(&state, velocities, NULL) && state.impulse == 1 &&
          velocities[0].linear.x == 0 && velocities[1].linear.x == 0);
    AbPositionState positions[] = {{{0, 0}, 0}, {{2, 0}, 0}};
    CHECK(ab_distance_position_solve(&definition, &state, positions, NULL) ==
              AB_POSITION_UNRESOLVED &&
          positions[0].center.x == 0.1f && positions[1].center.x == 1.9f);
    centers[1].x = 0.05f;
    definition.length = 0;
    CHECK(ab_distance_velocity_initialize(&definition, bodies, centers, step, &state,
                                          velocities, NULL) &&
          bits(state.axis.x) == 0 && bits(state.axis.y) == 0);
    positions[0] = (AbPositionState){{0, 0}, 0};
    positions[1] = (AbPositionState){{0.05f, 0}, 0};
    CHECK(ab_distance_position_solve(&definition, &state, positions, NULL) ==
              AB_POSITION_UNRESOLVED &&
          positions[0].center.x == 0.025f && positions[1].center.x == 0.025f);
    definition.frequency = 4;
    definition.damping_ratio = 0.5f;
    CHECK(ab_distance_velocity_initialize(&definition, bodies, centers, step, &state,
                                          velocities, NULL) &&
          state.gamma > 0 && state.bias > 0);
    AbPositionState before_positions[2];
    memcpy(before_positions, positions, sizeof(positions));
    AbDistanceState before_state = state;
    CHECK(ab_distance_position_solve(&definition, &state, positions, NULL) ==
              AB_POSITION_RESOLVED &&
          memcmp(before_positions, positions, sizeof(positions)) == 0 &&
          memcmp(&before_state, &state, sizeof(state)) == 0);
    AbVelocity before_velocities[2];
    memcpy(before_velocities, velocities, sizeof(velocities));
    CHECK(!ab_distance_velocity_initialize(NULL, bodies, centers, step, &state,
                                           velocities, NULL) &&
          !ab_distance_velocity_initialize(&definition, NULL, centers, step, &state,
                                           velocities, NULL) &&
          !ab_distance_velocity_initialize(&definition, bodies, NULL, step, &state,
                                           velocities, NULL) &&
          !ab_distance_velocity_initialize(&definition, bodies, centers, step, NULL,
                                           velocities, NULL) &&
          !ab_distance_velocity_initialize(&definition, bodies, centers, step, &state,
                                           NULL, NULL) &&
          !ab_distance_velocity_solve(NULL, velocities, NULL) &&
          !ab_distance_velocity_solve(&state, NULL, NULL) &&
          ab_distance_position_solve(NULL, &state, positions, NULL) ==
              AB_POSITION_INVALID &&
          ab_distance_position_solve(&definition, NULL, positions, NULL) ==
              AB_POSITION_INVALID &&
          ab_distance_position_solve(&definition, &state, NULL, NULL) ==
              AB_POSITION_INVALID);
    definition.length = -1;
    CHECK(!ab_distance_velocity_initialize(&definition, bodies, centers, step, &state,
                                           velocities, NULL));
    definition.length = 1;
    bodies[0].angle = FLT_MAX;
    CHECK(!ab_distance_velocity_initialize(&definition, bodies, centers, step, &state,
                                           velocities, NULL));
    bodies[0].angle = 0;
    definition.frequency = FLT_MAX;
    CHECK(!ab_distance_velocity_initialize(&definition, bodies, centers, step, &state,
                                           velocities, NULL) &&
          memcmp(&before_state, &state, sizeof(state)) == 0 &&
          memcmp(before_velocities, velocities, sizeof(velocities)) == 0);
    state.axis.x = NAN;
    before_state = state;
    CHECK(!ab_distance_velocity_solve(&state, velocities, NULL) &&
          memcmp(&before_state, &state, sizeof(state)) == 0 &&
          memcmp(before_velocities, velocities, sizeof(velocities)) == 0);
    definition.frequency = 0;
    positions[1].center.x = FLT_MAX;
    memcpy(before_positions, positions, sizeof(positions));
    CHECK(ab_distance_position_solve(&definition, &state, positions, NULL) ==
              AB_POSITION_INVALID &&
          memcmp(before_positions, positions, sizeof(positions)) == 0);
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool compare_state(FILE *file, const AbDistanceState *state,
                          const AbVelocity velocities[2], uint32_t sample,
                          uint32_t pass) {
    const float actual[] = {
        state->offsets[0].x,
        state->offsets[0].y,
        state->offsets[1].x,
        state->offsets[1].y,
        state->local_centers[0].x,
        state->local_centers[0].y,
        state->local_centers[1].x,
        state->local_centers[1].y,
        state->inverse_mass[0],
        state->inverse_mass[1],
        state->inverse_inertia[0],
        state->inverse_inertia[1],
        state->axis.x,
        state->axis.y,
        state->mass,
        state->gamma,
        state->bias,
        state->impulse,
        velocities[0].linear.x,
        velocities[0].linear.y,
        velocities[0].angular,
        velocities[1].linear.x,
        velocities[1].linear.y,
        velocities[1].angular,
    };
    uint32_t expected[24];
    CHECK(words(file, expected, 24));
    for (size_t field = 0; field < 24; ++field) {
        if (bits(actual[field]) != expected[field]) {
            fprintf(stderr,
                    "Distance sample %" PRIu32 " velocity pass %" PRIu32
                    " field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    sample, pass, field, bits(actual[field]), expected[field]);
            return false;
        }
    }
    return true;
}

static bool compare_positions(FILE *file, const AbPositionState positions[2],
                              AbPositionResult result, bool initial, uint32_t sample,
                              uint32_t pass) {
    uint32_t expected[7];
    const uint32_t actual[] = {
        result == AB_POSITION_RESOLVED, bits(positions[0].center.x),
        bits(positions[0].center.y),    bits(positions[0].angle),
        bits(positions[1].center.x),    bits(positions[1].center.y),
        bits(positions[1].angle),
    };
    size_t offset = initial ? 1 : 0;
    CHECK(words(file, expected, 7 - offset));
    for (size_t field = offset; field < 7; ++field) {
        if (actual[field] != expected[field - offset]) {
            fprintf(stderr,
                    "Distance sample %" PRIu32 " position pass %" PRIu32
                    " field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    sample, pass, field, actual[field], expected[field - offset]);
            return false;
        }
    }
    return true;
}

static bool compare_case(FILE *file, const uint32_t input[32], uint32_t sample,
                         uint32_t iterations) {
    float values[32];
    memcpy(values, input, sizeof(values));
    AbJointSolverBody bodies[2];
    for (size_t body = 0; body < 2; ++body) {
        size_t index = body * 5;
        bodies[body] = (AbJointSolverBody){{values[index], values[index + 1]},
                                           values[index + 2],
                                           values[index + 3],
                                           values[index + 4]};
    }
    AbVec2 centers[] = {{values[28], values[29]}, {values[30], values[31]}};
    AbDistanceDefinition definition = {
        {{values[10], values[11]}, {values[12], values[13]}},
        values[14],
        values[15],
        values[16],
    };
    AbDistanceState state = {.impulse = values[19]};
    AbJointStep step = {values[17], values[18], (sample & 1u) != 0};
    AbVelocity velocities[] = {{{values[22], values[23]}, values[24]},
                               {{values[25], values[26]}, values[27]}};
    AbError error = {0};
    if (!ab_distance_velocity_initialize(&definition, bodies, centers, step, &state,
                                         velocities, &error)) {
        fprintf(stderr, "Distance sample %" PRIu32 " rejected: %s\n", sample,
                error.message);
        return false;
    }
    CHECK(compare_state(file, &state, velocities, sample, 0));
    for (uint32_t pass = 0; pass < iterations; ++pass) {
        AbDistanceState before = state;
        CHECK(ab_distance_velocity_solve(&state, velocities, &error) &&
              compare_state(file, &state, velocities, sample, pass + 1));
        before.impulse = state.impulse;
        CHECK(memcmp(&before, &state, sizeof(state)) == 0);
    }
    AbPositionState positions[] = {{centers[0], values[2]}, {centers[1], values[7]}};
    CHECK(compare_positions(file, positions, AB_POSITION_RESOLVED, true, sample, 0));
    AbDistanceState before_state = state;
    AbVelocity before_velocities[2];
    memcpy(before_velocities, velocities, sizeof(velocities));
    for (uint32_t pass = 0; pass < iterations; ++pass) {
        AbPositionResult result =
            ab_distance_position_solve(&definition, &state, positions, &error);
        if (result == AB_POSITION_INVALID) {
            fprintf(stderr, "Distance sample %" PRIu32 " position %" PRIu32 ": %s\n",
                    sample, pass + 1, error.message);
            return false;
        }
        CHECK(compare_positions(file, positions, result, false, sample, pass + 1) &&
              memcmp(&before_state, &state, sizeof(state)) == 0 &&
              memcmp(before_velocities, velocities, sizeof(velocities)) == 0);
    }
    return true;
}

static bool native_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_DISTANCE_JOINT_V1 4096 8\n") == 0);
    size_t passes = 0;
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t metadata[3], input[32], generated[32];
        uint32_t iterations = sample < 192 ? sample % 8 + 1 : sample % 9;
        CHECK(words(file, metadata, 3) && metadata[0] == sample &&
              metadata[1] == (sample & 1u) && metadata[2] == iterations &&
              words(file, input, 32));
        distance_case(sample, generated);
        CHECK(memcmp(input, generated, sizeof(input)) == 0 &&
              compare_case(file, input, sample, iterations));
        passes += iterations;
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB distance joint reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Compared 4,096 native distance initializations, %zu velocity passes "
           "and %zu position passes (%zu float words and %zu convergence results)\n",
           passes, passes, 4096 * (size_t)30 + passes * 30, passes);
    return true;
}

int main(int argc, char **argv) {
    if (!edges()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    if (argc != 2) {
        fputs("Usage: ab_distance_joint_tests [NATIVE_CORPUS]\n", stderr);
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    bool okay = file && native_checks(file);
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
