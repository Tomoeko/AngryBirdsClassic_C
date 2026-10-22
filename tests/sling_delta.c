#include "ab/slingshot_control.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Sling delta failed at line %d: %s\n", __LINE__,           \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float number(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t word(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool edge_checks(void) {
    AbError error = {0};
    AbCoordinateCamera camera = {.top_left = {NAN, NAN}, .zoom = {1, NAN}};
    AbVec2 delta = {-0.0f, NAN}, before = delta;
    CHECK(ab_slingshot_delta_update(&delta, false, (AbVec2){NAN, NAN}, NULL, NAN, NAN,
                                    &error));
    CHECK(memcmp(&delta, &before, sizeof(delta)) == 0);
    CHECK(ab_slingshot_delta_reset(&delta, false));
    CHECK(memcmp(&delta, &before, sizeof(delta)) == 0);
    CHECK(ab_slingshot_delta_reset(&delta, true));
    CHECK(word(delta.x) == 0 && word(delta.y) == 0);
    CHECK(!ab_slingshot_delta_reset(NULL, false));
    CHECK(ab_slingshot_delta_update(&delta, true, (AbVec2){20, -40}, &camera, 1, 4,
                                    &error));
    CHECK(delta.x == -1 && delta.y == 2);
    before = delta;
    CHECK(!ab_slingshot_delta_update(&delta, true, (AbVec2){0}, NULL, 1, 4, &error));
    CHECK(memcmp(&delta, &before, sizeof(delta)) == 0);
    CHECK(
        !ab_slingshot_delta_update(&delta, true, (AbVec2){0}, &camera, 1, -1, &error));
    CHECK(memcmp(&delta, &before, sizeof(delta)) == 0);
    CHECK(!ab_slingshot_delta_update(&delta, true, (AbVec2){FLT_MAX, FLT_MAX}, &camera,
                                     FLT_MAX, 1, &error));
    CHECK(memcmp(&delta, &before, sizeof(delta)) == 0);
    delta = (AbVec2){100, 0};
    CHECK(ab_slingshot_delta_update(&delta, true, (AbVec2){0}, &camera, 1, 0, &error));
    CHECK(delta.x == 3.2f && delta.y == 0);
    CHECK(!ab_slingshot_delta_update(NULL, false, (AbVec2){0}, NULL, 0, 0, &error));
    return true;
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%8" SCNx32, words + index) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool native_checks(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB_ANDROID_SLING_DELTA_V1 4096\n") == 0);
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[10], expected[8];
        CHECK(read_words(file, input, 10));
        CHECK(read_words(file, expected, 8));
        bool enabled = (input[8] & 1) != 0;
        bool reset = (input[8] & 2) != 0;
        AbVec2 delta = {number(input[0]), number(input[1])};
        AbVec2 displacement = {number(input[2]), number(input[3])};
        AbCoordinateCamera camera = {.top_left = {number(input[6]), number(input[9])},
                                     .zoom = {number(input[4]), NAN}};
        AbError error = {0};
        CHECK(ab_slingshot_delta_update(&delta, enabled, displacement, &camera,
                                        number(input[5]), number(input[7]), &error));
        if (word(delta.x) != expected[2] || word(delta.y) != expected[3]) {
            fprintf(stderr,
                    "Native delta case %zu: %08" PRIx32 " %08" PRIx32
                    " expected %08" PRIx32 " %08" PRIx32 "\n",
                    item, word(delta.x), word(delta.y), expected[2], expected[3]);
            return false;
        }
        CHECK(expected[0] == (enabled ? 5u : 0u) && expected[1] == enabled);
        CHECK(ab_slingshot_delta_reset(&delta, reset));
        CHECK(word(delta.x) == expected[5] && word(delta.y) == expected[6]);
        CHECK(expected[4] == reset && expected[7] == (enabled ? 3u : 0u));
        for (size_t event = 0; event < expected[7]; ++event) {
            uint32_t recorded[6];
            CHECK(read_words(file, recorded, 6));
            CHECK(recorded[0] == event + 1 && recorded[5] == 1);
            if (event < 2) {
                float converted;
                CHECK(recorded[1] == input[2 + event]);
                CHECK(ab_coordinate_screen_scale(&camera, number(recorded[1]),
                                                 &converted));
                CHECK(word(converted) == recorded[2]);
            } else {
                CHECK(recorded[1] == 0 && recorded[2] == input[7]);
            }
        }
    }
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB sling delta reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    puts("All 4096 original sling delta updates/resets match; coordinate results, "
         "disabled preservation and native return/identity conditions verified.");
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        return 2;
    }
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = native_checks(file);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
