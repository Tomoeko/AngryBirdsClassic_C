#include "ab/physics_separation.h"

#include <float.h>
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

_Static_assert(sizeof(AbSweep) == 9 * sizeof(uint32_t), "Sweep reference layout");
_Static_assert(sizeof(AbDistanceCache) == 3 * sizeof(uint32_t),
               "Cache reference layout");

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

static bool sweep_checks(void) {
    AbSweep sweep = {.local_center = {1, 0}, .center = {4, 2}, .alpha = 0.25f};
    AbRigidTransform transform;
    CHECK(ab_sweep_transform(&sweep, 0.5f, &transform));
    CHECK(transform.position.x == 1 && transform.position.y == 1 &&
          transform.sine == 0 && transform.cosine == 1);
    CHECK(ab_sweep_advance(&sweep, 0.625f));
    CHECK(sweep.previous_center.x == 2 && sweep.previous_center.y == 1 &&
          sweep.alpha == 0.625f && sweep.center.x == 4);
    AbSweep unchanged = sweep;
    CHECK(ab_sweep_advance(&sweep, 0));
    CHECK(memcmp(&sweep, &unchanged, sizeof(sweep)) == 0);
    CHECK(!ab_sweep_advance(&sweep, 2));
    CHECK(memcmp(&sweep, &unchanged, sizeof(sweep)) == 0);
    CHECK(ab_sweep_advance(&sweep, 1) && sweep.previous_center.x == 4);
    sweep.previous_angle = -0x1.921fb6p+2f;
    CHECK(ab_sweep_normalize(&sweep));
    CHECK(sweep.previous_angle == 0 && sweep.angle == 0x1.921fb6p+2f);
    sweep.previous_angle = sweep.angle = -0.0f;
    CHECK(ab_sweep_normalize(&sweep));
    CHECK(to_bits(sweep.previous_angle) == 0 && to_bits(sweep.angle) == 0);
    sweep.previous_angle = FLT_MAX;
    sweep.angle = -FLT_MAX;
    unchanged = sweep;
    CHECK(!ab_sweep_normalize(&sweep));
    CHECK(memcmp(&sweep, &unchanged, sizeof(sweep)) == 0);
    AbRigidTransform old_transform = transform;
    sweep.angle = INFINITY;
    CHECK(!ab_sweep_transform(&sweep, 0, &transform));
    CHECK(memcmp(&transform, &old_transform, sizeof(transform)) == 0);
    return true;
}

static bool separation_checks(void) {
    AbVec2 first_vertices[] = {{0, 0}, {0, 2}};
    AbVec2 second_vertices[] = {{3, 4}, {3, 2}};
    AbDistanceProxy first = {first_vertices, 1, 0}, second = {second_vertices, 1, 0};
    AbSweep sweep = {0};
    AbDistanceCache cache = {.count = 1};
    AbSeparation state = {.local_point = {-3, 9}};
    float value = -1;
    CHECK(ab_separation_initialize(&state, &cache, &first, &sweep, &second, &sweep, 0,
                                   &value));
    CHECK(state.kind == AB_SEPARATION_POINTS && value == 5 &&
          state.local_point.x == -3 && state.local_point.y == 9);
    int32_t first_index = 9, second_index = 9;
    CHECK(ab_separation_find(&state, 1, &first_index, &second_index, &value));
    CHECK(first_index == 0 && second_index == 0 && value == 5);
    CHECK(!ab_separation_evaluate(&state, 1, 0, 0, &value) && value == 5);
    first.count = 2;
    second_vertices[0] = (AbVec2){3, 0};
    cache.count = 2;
    cache.first[1] = 1;
    CHECK(ab_separation_initialize(&state, &cache, &first, &sweep, &second, &sweep, 0,
                                   &value));
    CHECK(state.kind == AB_SEPARATION_FACE_FIRST && value == 3);
    CHECK(ab_separation_find(&state, 1, &first_index, &second_index, &value));
    CHECK(first_index == -1 && second_index == 0 && value == 3);
    first.count = 1;
    second.count = 2;
    cache.first[1] = 0;
    cache.second[1] = 1;
    CHECK(ab_separation_initialize(&state, &cache, &first, &sweep, &second, &sweep, 0,
                                   &value));
    CHECK(state.kind == AB_SEPARATION_FACE_SECOND && value == 3);
    CHECK(ab_separation_find(&state, 1, &first_index, &second_index, &value));
    CHECK(first_index == 0 && second_index == -1 && value == 3);
    AbSeparation unchanged = state;
    cache.count = 3;
    CHECK(!ab_separation_initialize(&state, &cache, &first, &sweep, &second, &sweep, 0,
                                    &value));
    CHECK(memcmp(&state, &unchanged, sizeof(state)) == 0 && value == 3);
    CHECK(!ab_separation_find(&state, NAN, &first_index, &second_index, &value));
    CHECK(first_index == 0 && second_index == -1 && value == 3);
    return true;
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%8" SCNx32, words + index) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool compare_words(const uint32_t *actual, const uint32_t *expected,
                          size_t count, size_t sample, const char *section) {
    for (size_t field = 0; field < count; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr,
                    "Separation sample %zu %s field %zu: %08" PRIx32 " != %08" PRIx32
                    "\n",
                    sample, section, field, actual[field], expected[field]);
            return false;
        }
    }
    return true;
}

static bool compare_sample(FILE *file, size_t sample, size_t counts[3]) {
    uint32_t input[58], before[25], expected[25], actual[25], expected_value;
    CHECK(read_words(file, input, 58) && read_words(file, before, 25) &&
          read_words(file, expected, 25) && read_words(file, &expected_value, 1));
    CHECK(input[0] >= 1 && input[0] <= 8 && input[1] >= 1 && input[1] <= 8);
    AbVec2 vertices[2][8];
    for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
        for (size_t vertex = 0; vertex < input[endpoint]; ++vertex) {
            vertices[endpoint][vertex] =
                (AbVec2){from_bits(input[4 + endpoint * 16 + vertex * 2]),
                         from_bits(input[5 + endpoint * 16 + vertex * 2])};
        }
    }
    AbDistanceProxy proxies[] = {{vertices[0], input[0], from_bits(input[2])},
                                 {vertices[1], input[1], from_bits(input[3])}};
    AbSweep sweeps[2];
    memcpy(sweeps, input + 36, sizeof(sweeps));
    AbDistanceCache cache;
    memcpy(&cache, input + 54, sizeof(cache));
    AbSeparation state = {
        .local_point = {from_bits(before[21]), from_bits(before[22])}};
    float value;
    CHECK(ab_separation_initialize(&state, &cache, proxies, sweeps, proxies + 1,
                                   sweeps + 1, from_bits(input[57]), &value));
    uint32_t value_bits = to_bits(value);
    CHECK(compare_words(&value_bits, &expected_value, 1, sample, "initialize result"));
    memcpy(actual, before, sizeof(actual));
    actual[0] = 0x52000001;
    actual[1] = 0x52000002;
    memcpy(actual + 2, &state.first_sweep, sizeof(AbSweep));
    memcpy(actual + 11, &state.second_sweep, sizeof(AbSweep));
    actual[20] = (uint32_t)state.kind;
    actual[21] = to_bits(state.local_point.x);
    actual[22] = to_bits(state.local_point.y);
    actual[23] = to_bits(state.axis.x);
    actual[24] = to_bits(state.axis.y);
    CHECK(compare_words(actual, expected, 25, sample, "initialize state"));
    CHECK((unsigned)state.kind <= 2);
    ++counts[state.kind];
    AbSeparation unchanged = state;
    for (size_t query = 0; query < 5; ++query) {
        uint32_t controls[3], expected_query[5], actual_query[5];
        CHECK(read_words(file, controls, 3) && read_words(file, expected_query, 5));
        int32_t first_index, second_index;
        CHECK(ab_separation_find(&state, from_bits(controls[0]), &first_index,
                                 &second_index, &value));
        actual_query[0] = (uint32_t)first_index;
        actual_query[1] = (uint32_t)second_index;
        actual_query[2] = to_bits(value);
        CHECK(ab_separation_evaluate(&state, first_index, second_index,
                                     from_bits(controls[0]), &value));
        actual_query[3] = to_bits(value);
        CHECK(ab_separation_evaluate(&state, (int32_t)controls[1], (int32_t)controls[2],
                                     from_bits(controls[0]), &value));
        actual_query[4] = to_bits(value);
        CHECK(compare_words(actual_query, expected_query, 5, sample, "query"));
        CHECK(memcmp(&state, &unchanged, sizeof(state)) == 0);
    }
    return true;
}

static bool native_checks(FILE *file) {
    char header[128];
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB_ANDROID_SEPARATION_V1 4096 5\n") == 0);
    size_t counts[3] = {0};
    for (size_t sample = 0; sample < 4096; ++sample) {
        CHECK(compare_sample(file, sample, counts));
    }
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB separation reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 separating axes and 20480 support/fixed-index queries match: "
           "%zu/%zu/%zu kinds.\n",
           counts[0], counts[1], counts[2]);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_separation_tests [NATIVE_SEPARATION_TRACE]\n");
        return 2;
    }
    if (!sweep_checks() || !separation_checks()) {
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
