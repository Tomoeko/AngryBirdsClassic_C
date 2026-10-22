#include "prismatic_native.h"
#include "prismatic_position_cases.h"

#include <float.h>
#include <math.h>

static bool edges(void) {
    AbPrismaticDefinition definition = {0};
    CHECK(ab_prismatic_axes_prepare((AbVec2){1, 0}, &definition.local, NULL));
    AbPrismaticState state = {.inverse_mass = {1, 1}, .inverse_inertia = {1, 1}};
    AbPositionState positions[] = {{{0, 0}, 0}, {{0, 0.05f}, 0}};
    CHECK(ab_prismatic_position_solve(&definition, &state, positions, NULL) ==
              AB_POSITION_RESOLVED &&
          positions[0].center.y == 0.025f && positions[1].center.y == 0.025f);
    positions[0] = (AbPositionState){{0, 0}, 0};
    positions[1] = (AbPositionState){{2, 0}, 0};
    CHECK(ab_prismatic_position_solve(&definition, &state, positions, NULL) ==
              AB_POSITION_RESOLVED &&
          positions[0].center.x == 0 && positions[1].center.x == 2);
    definition.enable_limit = true;
    definition.lower_translation = definition.upper_translation = 2;
    CHECK(ab_prismatic_position_solve(&definition, &state, positions, NULL) ==
              AB_POSITION_UNRESOLVED &&
          bits(positions[0].center.x) == UINT32_C(0x3dccccce) &&
          positions[1].center.x == 1.9f);
    definition.lower_translation = 0;
    definition.upper_translation = 1;
    positions[0] = (AbPositionState){{0, 0}, 0};
    positions[1] = (AbPositionState){{-0.01f, 0}, 0};
    CHECK(ab_prismatic_position_solve(&definition, &state, positions, NULL) ==
              AB_POSITION_RESOLVED &&
          positions[0].center.x == 0 && positions[1].center.x == -0.01f);
    definition.lower_translation = -1;
    definition.upper_translation = 0;
    positions[1].center.x = 0.05f;
    CHECK(ab_prismatic_position_solve(&definition, &state, positions, NULL) ==
              AB_POSITION_RESOLVED &&
          positions[0].center.x == 0 && positions[1].center.x == 0.05f);
    positions[1].center.x = nextafterf(0.05f, 1);
    CHECK(ab_prismatic_position_solve(&definition, &state, positions, NULL) ==
          AB_POSITION_UNRESOLVED);
    AbPositionState before[2];
    memcpy(before, positions, sizeof(before));
    AbPrismaticState before_state = state;
    CHECK(ab_prismatic_position_solve(NULL, &state, positions, NULL) ==
              AB_POSITION_INVALID &&
          ab_prismatic_position_solve(&definition, NULL, positions, NULL) ==
              AB_POSITION_INVALID &&
          ab_prismatic_position_solve(&definition, &state, NULL, NULL) ==
              AB_POSITION_INVALID);
    state.inverse_mass[0] = state.inverse_mass[1] = FLT_MAX;
    CHECK(ab_prismatic_position_solve(&definition, &state, positions, NULL) ==
          AB_POSITION_INVALID);
    state.inverse_mass[0] = state.inverse_mass[1] = 1;
    definition.local_anchors[0].x = NAN;
    CHECK(ab_prismatic_position_solve(&definition, &state, positions, NULL) ==
              AB_POSITION_INVALID &&
          memcmp(before, positions, sizeof(before)) == 0 &&
          memcmp(&before_state, &state, sizeof(state)) == 0);
    definition.local_anchors[0].x = 0;
    state.inverse_mass[0] = -1;
    before_state = state;
    CHECK(ab_prismatic_position_solve(&definition, &state, positions, NULL) ==
              AB_POSITION_INVALID &&
          memcmp(before, positions, sizeof(before)) == 0 &&
          memcmp(&before_state, &state, sizeof(state)) == 0);
    state.inverse_mass[0] = 1;
    positions[0].angle = FLT_MAX;
    memcpy(before, positions, sizeof(before));
    CHECK(ab_prismatic_position_solve(&definition, &state, positions, NULL) ==
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
                    "Prismatic position sample %" PRIu32 " pass %" PRIu32
                    " field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    sample, pass, field, actual[field], expected[field - offset]);
            return false;
        }
    }
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
        .enable_limit = prismatic_position_limit_enabled(sample) != 0,
    };
    CHECK(ab_prismatic_axes_prepare((AbVec2){values[32], values[33]}, &definition.local,
                                    NULL));
    AbVec2 centers[] = {{values[34], values[35]}, {values[36], values[37]}};
    AbJointStep step = {values[17], values[18], (sample & 1u) != 0};
    AbPrismaticState state = {.impulse = {values[19], values[20], values[21]},
                              .motor_impulse = values[28],
                              .limit = (AbJointLimitState)input[31]};
    AbVelocity velocities[] = {{{values[22], values[23]}, values[24]},
                               {{values[25], values[26]}, values[27]}};
    AbError error = {0};
    CHECK(ab_prismatic_velocity_initialize(&definition, bodies, centers, step, &state,
                                           velocities, &error) &&
          compare_state(file, definition.local, state, velocities, sample, 0));
    AbPositionState positions[] = {{{values[34], values[35]}, values[2]},
                                   {{values[36], values[37]}, values[7]}};
    CHECK(compare_positions(file, positions, AB_POSITION_RESOLVED, true, sample, 0));
    AbPrismaticState before = state;
    AbVelocity before_velocities[2];
    memcpy(before_velocities, velocities, sizeof(velocities));
    for (uint32_t pass = 0; pass < iterations; ++pass) {
        AbPositionResult result =
            ab_prismatic_position_solve(&definition, &state, positions, &error);
        if (result == AB_POSITION_INVALID) {
            fprintf(stderr,
                    "Prismatic position sample %" PRIu32 " pass %" PRIu32 ": %s\n",
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
          strcmp(line, "AB_ANDROID_PRISMATIC_POSITION_V1 4096 8\n") == 0);
    size_t passes = 0;
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t metadata[5], input[38], generated[38];
        uint32_t iterations = sample < 512 ? sample % 8 + 1 : sample % 9;
        CHECK(words(file, metadata, 5) && metadata[0] == sample &&
              metadata[1] == (sample & 1u) && metadata[2] == ((sample >> 1) & 1u) &&
              metadata[3] == prismatic_position_limit_enabled(sample) &&
              metadata[4] == iterations && words(file, input, 38));
        prismatic_position_case(sample, generated);
        CHECK(memcmp(input, generated, sizeof(input)) == 0 &&
              compare_case(file, input, sample, iterations));
        passes += iterations;
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB prismatic position reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Compared 4,096 native prismatic initializations and %zu position passes "
           "(%zu float words, 4,096 limit states and %zu convergence results)\n",
           passes, 4096 * (size_t)46 + passes * 6, passes);
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
        fputs("Usage: ab_prismatic_position_tests [NATIVE_CORPUS]\n", stderr);
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    bool okay = file && native_checks(file);
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
