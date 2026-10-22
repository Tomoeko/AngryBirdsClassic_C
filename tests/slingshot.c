#include "ab/slingshot.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Slingshot failed at line %d: %s\n", __LINE__,             \
                    #condition);                                                       \
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
    AbSlingLaunchInput input = {.rubber_band = {3, 4},
                                .stretch = 2,
                                .launch_force = 10,
                                .physics_scale = 0.5f,
                                .bird_mass = 2,
                                .slingshot_power = 1,
                                .bird_present = true};
    AbVec2 output;
    CHECK(ab_slingshot_launch_velocity(&input, &output));
    CHECK(output.x == 6 && output.y == 8);
    input.bird_mass = 0;
    CHECK(!ab_slingshot_launch_velocity(&input, &output));
    CHECK(output.x == 6 && output.y == 8);
    input.bird_mass = 2;
    input.rest = input.rubber_band;
    CHECK(!ab_slingshot_launch_velocity(&input, &output));
    input.rest.x = FLT_MAX;
    CHECK(!ab_slingshot_launch_velocity(&input, &output));
    input.rest.x = NAN;
    CHECK(!ab_slingshot_launch_velocity(&input, &output));
    input.bird_present = false;
    CHECK(ab_slingshot_launch_velocity(&input, &output));
    CHECK(to_bits(output.x) == 0 && to_bits(output.y) == 0);
    CHECK(!ab_slingshot_launch_velocity(NULL, &output));
    CHECK(!ab_slingshot_launch_velocity(&input, NULL));
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, output + field) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

#include "sling_transfer.h"
#include "sling_start.h"
#include "sling_drag.h"
#include "sling_impulse.h"

static bool native_checks(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    if (strcmp(marker, "AB_ANDROID_SLING_TRANSFER_V1 4096\n") == 0) {
        return transfer_native_checks(file);
    }
    if (strcmp(marker, "AB_ANDROID_SLING_START_V1 4096\n") == 0) {
        return sling_start_native_checks(file);
    }
    if (strcmp(marker, "AB_ANDROID_SLING_DRAG_V1 4096\n") == 0) {
        return sling_drag_native_checks(file);
    }
    if (strcmp(marker, "AB_ANDROID_SLING_IMPULSE_V1 4096\n") == 0) {
        return sling_impulse_native_checks(file, false);
    }
    if (strcmp(marker, "AB_ANDROID_SLING_ROUTING_V1 4096\n") == 0) {
        return sling_impulse_native_checks(file, true);
    }
    CHECK(strcmp(marker, "AB_ANDROID_SLINGSHOT_V1 4096\n") == 0);
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t data[11], expected[2];
        CHECK(read_words(file, data, 11));
        CHECK(read_words(file, expected, 2));
        AbSlingLaunchInput input = {
            .rest = {from_bits(data[0]), from_bits(data[1])},
            .rubber_band = {from_bits(data[2]), from_bits(data[3])},
            .stretch = from_bits(data[4]),
            .launch_force = from_bits(data[data[10] & 2 ? 5 : 6]),
            .physics_scale = from_bits(data[7]),
            .bird_mass = from_bits(data[8]),
            .slingshot_power = from_bits(data[9]),
            .bird_present = (data[10] & 1) != 0};
        AbVec2 result;
        CHECK(ab_slingshot_launch_velocity(&input, &result));
        uint32_t actual[] = {to_bits(result.x), to_bits(result.y)};
        for (size_t field = 0; field < 2; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Native launch %zu field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
    }
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB slingshot reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    puts("All 4096 original-Lua launch velocities match both float outputs.");
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_slingshot_tests [NATIVE_LAUNCH_TRACE]\n");
        return 2;
    }
    if (!edge_checks() || !transfer_edges() || !sling_start_edges() ||
        !sling_drag_edges() || !sling_impulse_edges()) {
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
