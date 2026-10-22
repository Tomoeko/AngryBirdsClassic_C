#include "revolute_native.h"
#include "revolute_position_cases.h"

#include <float.h>
#include <math.h>

static bool edges(void) {
    AbRevoluteDefinition definition = {0};
    AbRevoluteState state = {
        .inverse_mass = {1, 1}, .inverse_inertia = {1, 1}, .motor_mass = 0.5f};
    AbPositionState positions[] = {{{0, 0}, 0}, {{0.05f, 0}, 0}};
    CHECK(ab_revolute_position_solve(&definition, &state, positions, NULL) ==
              AB_POSITION_RESOLVED &&
          positions[0].center.x == 0.025f && positions[1].center.x == 0.025f);
    definition.enable_limit = true;
    state.limit = AB_JOINT_LIMIT_EQUAL;
    positions[0] = (AbPositionState){{0, 0}, 0};
    positions[1] = (AbPositionState){{0, 0}, 1};
    CHECK(ab_revolute_position_solve(&definition, &state, positions, NULL) ==
              AB_POSITION_UNRESOLVED &&
          positions[0].angle > 0 && positions[1].angle < 1);
    state.limit = AB_JOINT_LIMIT_LOWER;
    definition.upper_angle = 1;
    positions[0].angle = 0;
    positions[1].angle = -0.01f;
    CHECK(ab_revolute_position_solve(&definition, &state, positions, NULL) ==
              AB_POSITION_RESOLVED &&
          positions[0].angle == 0 && positions[1].angle == -0.01f);
    state.limit = AB_JOINT_LIMIT_UPPER;
    definition.lower_angle = -1;
    definition.upper_angle = 0;
    positions[1].angle = 0x1.1df46cp-5f;
    CHECK(ab_revolute_position_solve(&definition, &state, positions, NULL) ==
              AB_POSITION_RESOLVED &&
          positions[0].angle == 0 && positions[1].angle == 0x1.1df46cp-5f);
    positions[1].angle = nextafterf(0x1.1df46cp-5f, 1);
    CHECK(ab_revolute_position_solve(&definition, &state, positions, NULL) ==
          AB_POSITION_UNRESOLVED);
    AbPositionState before[2];
    memcpy(before, positions, sizeof(before));
    AbRevoluteState before_state = state;
    CHECK(ab_revolute_position_solve(NULL, &state, positions, NULL) ==
              AB_POSITION_INVALID &&
          ab_revolute_position_solve(&definition, NULL, positions, NULL) ==
              AB_POSITION_INVALID &&
          ab_revolute_position_solve(&definition, &state, NULL, NULL) ==
              AB_POSITION_INVALID);
    definition.local_anchors[0].x = FLT_MAX;
    CHECK(ab_revolute_position_solve(&definition, &state, positions, NULL) ==
          AB_POSITION_INVALID);
    definition.local_anchors[0].x = NAN;
    CHECK(ab_revolute_position_solve(&definition, &state, positions, NULL) ==
              AB_POSITION_INVALID &&
          memcmp(before, positions, sizeof(before)) == 0 &&
          memcmp(&before_state, &state, sizeof(state)) == 0);
    definition.local_anchors[0].x = 0;
    state.inverse_mass[0] = -1;
    before_state = state;
    CHECK(ab_revolute_position_solve(&definition, &state, positions, NULL) ==
              AB_POSITION_INVALID &&
          memcmp(before, positions, sizeof(before)) == 0 &&
          memcmp(&before_state, &state, sizeof(state)) == 0);
    state.inverse_mass[0] = 1;
    positions[0].angle = FLT_MAX;
    memcpy(before, positions, sizeof(before));
    CHECK(ab_revolute_position_solve(&definition, &state, positions, NULL) ==
              AB_POSITION_INVALID &&
          memcmp(before, positions, sizeof(before)) == 0);
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
                    "Revolute position sample %" PRIu32 " pass %" PRIu32
                    " field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    sample, pass, field, actual[field], expected[field - offset]);
            return false;
        }
    }
    return true;
}

static bool compare_case(FILE *file, const uint32_t input[36], uint32_t sample,
                         uint32_t iterations) {
    float values[36];
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
        .enable_limit = revolute_position_limit_enabled(sample) != 0,
    };
    AbJointStep step = {values[17], values[18], (sample & 1u) != 0};
    AbRevoluteState state = {.impulse = {values[19], values[20], values[21]},
                             .motor_impulse = values[28],
                             .limit = (AbJointLimitState)input[31]};
    AbVelocity velocities[] = {{{values[22], values[23]}, values[24]},
                               {{values[25], values[26]}, values[27]}};
    AbError error = {0};
    CHECK(ab_revolute_velocity_initialize(&definition, bodies, step, &state, velocities,
                                          &error) &&
          compare_state(file, state, velocities, sample, 0));
    AbPositionState positions[] = {{{values[32], values[33]}, values[2]},
                                   {{values[34], values[35]}, values[7]}};
    CHECK(compare_positions(file, positions, AB_POSITION_RESOLVED, true, sample, 0));
    AbRevoluteState before = state;
    AbVelocity before_velocities[2];
    memcpy(before_velocities, velocities, sizeof(velocities));
    for (uint32_t pass = 0; pass < iterations; ++pass) {
        AbPositionResult result =
            ab_revolute_position_solve(&definition, &state, positions, &error);
        if (result == AB_POSITION_INVALID) {
            fprintf(stderr,
                    "Revolute position sample %" PRIu32 " pass %" PRIu32 ": %s\n",
                    sample, pass + 1, error.message);
            return false;
        }
        CHECK(compare_positions(file, positions, result, false, sample, pass + 1) &&
              memcmp(&before, &state, sizeof(state)) == 0 &&
              memcmp(before_velocities, velocities, sizeof(velocities)) == 0);
    }
    return true;
}

static bool native_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_REVOLUTE_POSITION_V1 4096 8\n") == 0);
    size_t passes = 0;
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t metadata[5], input[36], generated[36];
        uint32_t iterations = sample < 256 ? sample % 8 + 1 : sample % 9;
        CHECK(words(file, metadata, 5) && metadata[0] == sample &&
              metadata[1] == (sample & 1u) && metadata[2] == ((sample >> 1) & 1u) &&
              metadata[3] == revolute_position_limit_enabled(sample) &&
              metadata[4] == iterations && words(file, input, 36));
        revolute_position_case(sample, generated);
        CHECK(memcmp(input, generated, sizeof(input)) == 0 &&
              compare_case(file, input, sample, iterations));
        passes += iterations;
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB revolute position reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Compared 4,096 native revolute initializations and %zu position passes "
           "(%zu float words, 4,096 limit states and %zu convergence results)\n",
           passes, 4096 * (size_t)38 + passes * 6, passes);
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
        fputs("Usage: ab_revolute_position_tests [NATIVE_CORPUS]\n", stderr);
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    bool okay = file && native_checks(file);
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
