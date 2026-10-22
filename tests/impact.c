#include "ab/physics_impact.h"

#include <inttypes.h>
#include <math.h>
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
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t to_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool edge_checks(void) {
    AbVec2 origin = {0, 0};
    AbImpactInput input = {.first = {&origin, 1, 0.5f},
                           .second = {&origin, 1, 0.5f},
                           .maximum_fraction = 1};
    AbImpactResult result;
    CHECK(ab_time_of_impact(&input, &result));
    CHECK(result.kind == AB_IMPACT_OVERLAPPED && result.fraction == 0 &&
          result.iterations == 0 && result.root_iterations == 0);
    input.second_sweep.previous_center.x = input.second_sweep.center.x = 4;
    CHECK(ab_time_of_impact(&input, &result));
    CHECK(result.kind == AB_IMPACT_SEPARATED && result.fraction == 1 &&
          result.iterations == 1 && result.root_iterations == 0);
    input.second_sweep.center.x = -4;
    CHECK(ab_time_of_impact(&input, &result));
    CHECK(result.kind == AB_IMPACT_TOUCHING && result.fraction > 0.3f &&
          result.fraction < 0.5f && result.root_iterations > 0);
    AbImpactResult unchanged = result;
    input.maximum_fraction = NAN;
    CHECK(!ab_time_of_impact(&input, &result));
    CHECK(memcmp(&result, &unchanged, sizeof(result)) == 0);
    input.maximum_fraction = 1;
    input.first.radius = -1;
    CHECK(!ab_time_of_impact(&input, &result));
    input.first.radius = 0;
    input.first_sweep.previous_angle = INFINITY;
    CHECK(!ab_time_of_impact(&input, &result));
    CHECK(memcmp(&result, &unchanged, sizeof(result)) == 0);
    input.first_sweep.previous_angle = 0;
    input.first.count = 0;
    CHECK(!ab_time_of_impact(&input, &result));
    CHECK(memcmp(&result, &unchanged, sizeof(result)) == 0);
    return true;
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, words + field) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool compare_sample(FILE *file, size_t sample, size_t kinds[5],
                           size_t *iterations, size_t *roots) {
    uint32_t data[55], expected[4];
    CHECK(read_words(file, data, 55) && read_words(file, expected, 4));
    CHECK(data[0] >= 1 && data[0] <= 8 && data[1] >= 1 && data[1] <= 8);
    AbVec2 vertices[2][8];
    for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
        for (size_t vertex = 0; vertex < data[endpoint]; ++vertex) {
            vertices[endpoint][vertex] =
                (AbVec2){from_bits(data[4 + endpoint * 16 + vertex * 2]),
                         from_bits(data[5 + endpoint * 16 + vertex * 2])};
        }
    }
    AbImpactInput input = {.first = {vertices[0], data[0], from_bits(data[2])},
                           .second = {vertices[1], data[1], from_bits(data[3])},
                           .maximum_fraction = from_bits(data[54])};
    _Static_assert(sizeof(AbSweep) == 9 * sizeof(uint32_t), "Sweep reference layout");
    memcpy(&input.first_sweep, data + 36, sizeof(AbSweep));
    memcpy(&input.second_sweep, data + 45, sizeof(AbSweep));
    uint8_t original[sizeof(input)];
    memcpy(original, &input, sizeof(original));
    AbImpactResult result;
    if (!ab_time_of_impact(&input, &result)) {
        fprintf(stderr, "Impact query %zu rejected a native input\n", sample);
        return false;
    }
    CHECK(memcmp(original, &input, sizeof(input)) == 0);
    const uint32_t actual[] = {(uint32_t)result.kind, to_bits(result.fraction),
                               result.iterations, result.root_iterations};
    for (size_t field = 0; field < 4; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr,
                    "Impact sample %zu field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    sample, field, actual[field], expected[field]);
            return false;
        }
    }
    CHECK((unsigned)result.kind <= AB_IMPACT_SEPARATED);
    ++kinds[result.kind];
    *iterations += result.iterations;
    *roots += result.root_iterations;
    return true;
}

static bool native_checks(FILE *file) {
    char header[128];
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB_ANDROID_IMPACT_V1 4096\n") == 0);
    size_t kinds[5] = {0}, iterations = 0, roots = 0;
    for (size_t sample = 0; sample < 4096; ++sample) {
        CHECK(compare_sample(file, sample, kinds, &iterations, &roots));
    }
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB impact reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 impact states/fractions and iteration counts match: "
           "%zu searches, %zu roots; kinds %zu/%zu/%zu/%zu/%zu.\n",
           iterations, roots, kinds[0], kinds[1], kinds[2], kinds[3], kinds[4]);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_impact_tests [NATIVE_IMPACT_TRACE]\n");
        return 2;
    }
    if (!edge_checks()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        perror(argv[1]);
        return 1;
    }
    bool okay = native_checks(file);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
