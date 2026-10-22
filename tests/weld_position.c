#include "ab/physics_weld.h"

#include "weld_position_cases.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Weld position failed at %d: %s\n", __LINE__, #condition); \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t bits(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

static bool edges(void) {
    AbWeldDefinition definition = {0};
    AbWeldState state = {.inverse_mass = {1, 1}, .inverse_inertia = {1, 1}};
    AbPositionState positions[] = {{{0, 0}, 0}, {{0.05f, 0}, 0}};
    CHECK(ab_weld_position_solve(&definition, &state, positions, NULL) ==
              AB_POSITION_RESOLVED &&
          positions[0].center.x == 0.025f && positions[1].center.x == 0.025f);
    positions[0].center.x = 0;
    positions[1].center.x = nextafterf(0.05f, 1);
    CHECK(ab_weld_position_solve(&definition, &state, positions, NULL) ==
              AB_POSITION_UNRESOLVED &&
          positions[0].center.x == positions[1].center.x);
    definition.frequency = 4;
    positions[0] = (AbPositionState){{0, 0}, 0};
    positions[1] = (AbPositionState){{0, 0}, 1};
    CHECK(ab_weld_position_solve(&definition, &state, positions, NULL) ==
              AB_POSITION_RESOLVED &&
          positions[0].angle == 0 && positions[1].angle == 1);
    definition.frequency = 0;
    state.inverse_inertia[0] = state.inverse_inertia[1] = 0;
    CHECK(ab_weld_position_solve(&definition, &state, positions, NULL) ==
              AB_POSITION_UNRESOLVED &&
          positions[0].angle == 0 && positions[1].angle == 1);
    AbPositionState before[2];
    memcpy(before, positions, sizeof(before));
    AbWeldState before_state = state;
    CHECK(
        ab_weld_position_solve(NULL, &state, positions, NULL) == AB_POSITION_INVALID &&
        ab_weld_position_solve(&definition, NULL, positions, NULL) ==
            AB_POSITION_INVALID &&
        ab_weld_position_solve(&definition, &state, NULL, NULL) == AB_POSITION_INVALID);
    definition.local_anchors[0].x = NAN;
    CHECK(ab_weld_position_solve(&definition, &state, positions, NULL) ==
          AB_POSITION_INVALID);
    definition.local_anchors[0].x = FLT_MAX;
    CHECK(ab_weld_position_solve(&definition, &state, positions, NULL) ==
          AB_POSITION_INVALID);
    definition.local_anchors[0].x = 0;
    CHECK(memcmp(before, positions, sizeof(before)) == 0 &&
          memcmp(&before_state, &state, sizeof(state)) == 0);
    positions[0].angle = FLT_MAX;
    memcpy(before, positions, sizeof(before));
    CHECK(ab_weld_position_solve(&definition, &state, positions, NULL) ==
              AB_POSITION_INVALID &&
          memcmp(before, positions, sizeof(before)) == 0 &&
          memcmp(&before_state, &state, sizeof(state)) == 0);
    positions[0].angle = 0;
    state.inverse_mass[1] = -1;
    memcpy(before, positions, sizeof(before));
    before_state = state;
    CHECK(ab_weld_position_solve(&definition, &state, positions, NULL) ==
              AB_POSITION_INVALID &&
          memcmp(before, positions, sizeof(before)) == 0 &&
          memcmp(&before_state, &state, sizeof(state)) == 0);
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool compare_words(FILE *file, const float *actual, size_t count,
                          uint32_t sample, uint32_t iteration) {
    uint32_t expected[24];
    CHECK(count <= 24 && words(file, expected, count));
    for (size_t field = 0; field < count; ++field) {
        if (bits(actual[field]) != expected[field]) {
            fprintf(stderr,
                    "Weld position sample %" PRIu32 " pass %" PRIu32 " field %zu: "
                    "%08" PRIx32 " != %08" PRIx32 "\n",
                    sample, iteration, field, bits(actual[field]), expected[field]);
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
    AbWeldDefinition definition = {{{values[10], values[11]}, {values[12], values[13]}},
                                   values[14],
                                   values[15],
                                   values[16]};
    AbJointStep step = {values[17], values[18], (sample & 1u) != 0};
    AbWeldState state = {.impulse = {values[19], values[20], values[21]}};
    AbVelocity velocities[] = {{{values[22], values[23]}, values[24]},
                               {{values[25], values[26]}, values[27]}};
    AbError error = {0};
    CHECK(ab_weld_velocity_initialize(&definition, bodies, step, &state, velocities,
                                      &error));
    const float initialized[] = {
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
    CHECK(compare_words(file, initialized, 24, sample, 0));
    for (size_t body = 0; body < 2; ++body) {
        size_t index = body * 5;
        CHECK(bits(state.local_centers[body].x) == input[index] &&
              bits(state.local_centers[body].y) == input[index + 1] &&
              bits(state.inverse_mass[body]) == input[index + 3] &&
              bits(state.inverse_inertia[body]) == input[index + 4]);
    }
    AbPositionState positions[] = {{{values[28], values[29]}, values[2]},
                                   {{values[30], values[31]}, values[7]}};
    const float centers[] = {positions[0].center.x, positions[0].center.y,
                             positions[0].angle,    positions[1].center.x,
                             positions[1].center.y, positions[1].angle};
    CHECK(compare_words(file, centers, 6, sample, 0));
    AbWeldState before = state;
    AbVelocity before_velocities[2];
    memcpy(before_velocities, velocities, sizeof(velocities));
    for (uint32_t iteration = 0; iteration < iterations; ++iteration) {
        AbPositionResult result =
            ab_weld_position_solve(&definition, &state, positions, &error);
        if (result == AB_POSITION_INVALID) {
            fprintf(stderr, "Weld position sample %" PRIu32 " pass %" PRIu32 ": %s\n",
                    sample, iteration + 1, error.message);
            return false;
        }
        uint32_t expected[7];
        CHECK(words(file, expected, 7));
        const uint32_t actual[] = {
            result == AB_POSITION_RESOLVED, bits(positions[0].center.x),
            bits(positions[0].center.y),    bits(positions[0].angle),
            bits(positions[1].center.x),    bits(positions[1].center.y),
            bits(positions[1].angle),
        };
        for (size_t field = 0; field < 7; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Weld position sample %" PRIu32 " pass %" PRIu32
                        " field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                        sample, iteration + 1, field, actual[field], expected[field]);
                return false;
            }
        }
        CHECK(memcmp(&before, &state, sizeof(state)) == 0 &&
              memcmp(before_velocities, velocities, sizeof(velocities)) == 0);
    }
    return true;
}

static bool native_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_WELD_POSITION_V1 4096 8\n") == 0);
    size_t passes = 0;
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t metadata[3], input[32], generated[32];
        uint32_t iterations = sample < 96 ? sample % 8 + 1 : sample % 9;
        CHECK(words(file, metadata, 3) && metadata[0] == sample &&
              metadata[1] == (sample & 1u) && metadata[2] == iterations &&
              words(file, input, 32));
        weld_position_case(sample, generated);
        CHECK(memcmp(input, generated, sizeof(input)) == 0 &&
              compare_case(file, input, sample, iterations));
        passes += iterations;
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB weld position reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Compared 4,096 native weld initializations and %zu position passes "
           "(%zu float words and %zu convergence decisions)\n",
           passes, 4096 * (size_t)30 + passes * 6, passes);
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
        fputs("Usage: ab_weld_position_tests [NATIVE_CORPUS]\n", stderr);
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    bool okay = file && native_checks(file);
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
