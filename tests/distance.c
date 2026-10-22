#include "ab/physics_distance.h"

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

_Static_assert(sizeof(AbDistanceCache) == 3 * sizeof(uint32_t),
               "Native distance cache comparison requires three words");
_Static_assert(sizeof(AbDistanceResult) == 6 * sizeof(uint32_t),
               "Native distance output comparison requires six words");

static float from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool edge_checks(void) {
    AbVec2 first = {0, 0}, second = {3, 4};
    AbDistanceProxy first_proxy = {&first, 1, 1}, second_proxy = {&second, 1, 1};
    AbRigidTransform identity = {{0, 0}, 0, 1};
    AbDistanceCache cache = {0};
    AbDistanceResult result;
    CHECK(ab_shape_distance(first_proxy, identity, second_proxy, identity, false,
                            &cache, &result));
    CHECK(result.distance == 5 && result.iterations == 1 && cache.count == 1 &&
          result.first.x == 0 && result.second.y == 4);
    CHECK(ab_shape_distance(first_proxy, identity, second_proxy, identity, true, &cache,
                            &result));
    CHECK(result.distance == 3 && result.first.x == 0.6f && result.first.y == 0.8f);
    first_proxy.radius = second_proxy.radius = 3;
    CHECK(ab_shape_distance(first_proxy, identity, second_proxy, identity, true, &cache,
                            &result));
    CHECK(result.distance == 0 && result.first.x == 1.5f && result.first.y == 2 &&
          result.second.x == result.first.x && result.second.y == result.first.y);
    AbDistanceResult unchanged = result;
    AbDistanceCache old_cache = cache;
    first_proxy.count = 0;
    CHECK(!ab_shape_distance(first_proxy, identity, second_proxy, identity, true,
                             &cache, &result));
    CHECK(memcmp(&cache, &old_cache, sizeof(cache)) == 0);
    CHECK(memcmp(&result, &unchanged, sizeof(result)) == 0);
    first_proxy.count = 1;
    first_proxy.radius = -1;
    CHECK(!ab_shape_distance(first_proxy, identity, second_proxy, identity, true,
                             &cache, &result));
    first_proxy.radius = 0;
    first.x = NAN;
    CHECK(!ab_shape_distance(first_proxy, identity, second_proxy, identity, true,
                             &cache, &result));
    first.x = FLT_MAX;
    second.x = -FLT_MAX;
    CHECK(!ab_shape_distance(first_proxy, identity, second_proxy, identity, true,
                             &cache, &result));
    CHECK(memcmp(&cache, &old_cache, sizeof(cache)) == 0);
    CHECK(memcmp(&result, &unchanged, sizeof(result)) == 0);
    first.x = second.x = 0;
    cache.count = 4;
    CHECK(!ab_shape_distance(first_proxy, identity, second_proxy, identity, false,
                             &cache, &result));
    cache.count = 1;
    cache.first[0] = 1;
    CHECK(!ab_shape_distance(first_proxy, identity, second_proxy, identity, false,
                             &cache, &result));
    bool overlapping = true;
    CHECK(
        ab_shape_overlap(first_proxy, identity, second_proxy, identity, &overlapping));
    CHECK(!overlapping);
    second_proxy.radius = 0;
    second = (AbVec2){10.0f * FLT_EPSILON, 0};
    CHECK(
        ab_shape_overlap(first_proxy, identity, second_proxy, identity, &overlapping));
    CHECK(!overlapping);
    second.x = from_bits(0x359fffff);
    CHECK(
        ab_shape_overlap(first_proxy, identity, second_proxy, identity, &overlapping));
    CHECK(overlapping);
    first_proxy.count = 0;
    CHECK(
        !ab_shape_overlap(first_proxy, identity, second_proxy, identity, &overlapping));
    CHECK(overlapping);
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
                    "Distance sample %zu %s field %zu: %08" PRIx32 " != %08" PRIx32
                    "\n",
                    sample, section, field, actual[field], expected[field]);
            return false;
        }
    }
    return true;
}

static AbRigidTransform read_transform(const uint32_t words[4]) {
    return (AbRigidTransform){{from_bits(words[0]), from_bits(words[1])},
                              from_bits(words[2]),
                              from_bits(words[3])};
}

static bool native_checks(FILE *file) {
    char header[128];
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB_ANDROID_DISTANCE_V1 4096\n") == 0);
    size_t iterations = 0, simplices[4] = {0};
    for (size_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[45], cache_words[3], expected[6], expected_cache[3], actual[6];
        uint32_t expected_overlap;
        CHECK(read_words(file, input, 45) && read_words(file, cache_words, 3) &&
              read_words(file, &expected_overlap, 1) && read_words(file, expected, 6) &&
              read_words(file, expected_cache, 3));
        CHECK(input[0] > 0 && input[0] <= 8 && input[1] > 0 && input[1] <= 8);
        AbVec2 vertices[2][8];
        for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
            for (size_t vertex = 0; vertex < input[endpoint]; ++vertex) {
                vertices[endpoint][vertex] =
                    (AbVec2){from_bits(input[13 + endpoint * 16 + vertex * 2]),
                             from_bits(input[14 + endpoint * 16 + vertex * 2])};
            }
        }
        AbDistanceProxy first = {vertices[0], input[0], from_bits(input[2])};
        AbDistanceProxy second = {vertices[1], input[1], from_bits(input[3])};
        AbDistanceCache cache;
        memcpy(&cache, cache_words, sizeof(cache));
        AbDistanceResult result;
        CHECK(ab_shape_distance(first, read_transform(input + 5), second,
                                read_transform(input + 9), input[4] != 0, &cache,
                                &result));
        memcpy(actual, &result, sizeof(actual));
        memcpy(cache_words, &cache, sizeof(cache));
        CHECK(compare_words(actual, expected, 6, sample, "output"));
        CHECK(compare_words(cache_words, expected_cache, 3, sample, "cache"));
        bool overlapping;
        CHECK(ab_shape_overlap(first, read_transform(input + 5), second,
                               read_transform(input + 9), &overlapping));
        uint32_t actual_overlap = (uint32_t)overlapping;
        CHECK(compare_words(&actual_overlap, &expected_overlap, 1, sample, "overlap"));
        CHECK(cache.count <= 3);
        iterations += result.iterations;
        ++simplices[cache.count];
    }
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB distance reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 distance queries match: %zu iterations; simplex counts "
           "%zu/%zu/%zu.\n",
           iterations, simplices[1], simplices[2], simplices[3]);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_distance_tests [NATIVE_DISTANCE_TRACE]\n");
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
