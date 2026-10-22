#include "ab/bird_retirement.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Bird retirement failed at line %d: %s\n", __LINE__,       \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float number(uint32_t bits) {
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

static uint32_t word(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool edge_checks(void) {
    AbBirdRetirementState state = {.ready_timer_present = true};
    AbBirdRetirementInput input = {.controllable = true,
                                   .shot_literal_true = true,
                                   .speed_threshold = 0.0025f,
                                   .reset_timer = 1};
    AbBirdRetirementFrame output;
    CHECK(ab_bird_retirement_frame(&state, &input, &output));
    CHECK(!output.remove_bird && !output.query_camera_target);
    input.seconds = 0.1f;
    CHECK(ab_bird_retirement_frame(&state, &input, &output));
    CHECK(output.remove_bird && output.query_camera_target);
    input.camera_targets_bird = true;
    CHECK(ab_bird_retirement_frame(&state, &input, &output));
    CHECK(!output.remove_bird && output.query_camera_target);
    input.speed_squared = NAN;
    output.next.ready_timer = 19;
    CHECK(!ab_bird_retirement_frame(&state, &input, &output));
    CHECK(output.next.ready_timer == 19);
    input.speed_squared = 0;
    input.seconds = FLT_MAX;
    state.ready_timer = -FLT_MAX;
    CHECK(!ab_bird_retirement_frame(&state, &input, &output));
    CHECK(!ab_bird_retirement_frame(NULL, &input, &output));
    CHECK(!ab_bird_retirement_frame(&state, NULL, &output));
    CHECK(!ab_bird_retirement_frame(&state, &input, NULL));
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, output + field) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool native_checks(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB_ANDROID_BIRD_RETIREMENT_V1 4096\n") == 0);
    size_t definitions = 0, speeds = 0, cameras = 0, removals = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[16], expected[6], count;
        CHECK(read_words(file, input, 16) && read_words(file, expected, 6));
        CHECK(input[1] <= 1 && input[3] <= 2 && input[15] <= 1);
        for (size_t field = 8; field < 15; ++field) {
            CHECK(input[field] <= 4);
        }
        AbBirdRetirementState state = {.ready_timer = number(input[0]),
                                       .water_timer = number(input[2]),
                                       .ready_timer_present = input[1] != 0,
                                       .water_timer_present = input[3] == 2};
        AbBirdRetirementInput parameters = {.seconds = number(input[4]),
                                            .speed_squared = number(input[5]),
                                            .speed_threshold = number(input[6]),
                                            .reset_timer = number(input[7]),
                                            .controllable = input[8] >= 2,
                                            .mighty_bait = input[9] >= 2,
                                            .mighty_eagle = input[10] >= 2,
                                            .shot_literal_true = input[11] == 2,
                                            .manually_removed = input[12] >= 2,
                                            .water_active = input[13] >= 2,
                                            .outside_boundaries = input[14] >= 2,
                                            .camera_targets_bird = input[15] != 0};
        AbBirdRetirementFrame result;
        CHECK(ab_bird_retirement_frame(&state, &parameters, &result));
        uint32_t actual[] = {
            result.next.ready_timer_present ? word(result.next.ready_timer) : 0,
            result.next.ready_timer_present,
            result.next.water_timer_present ? word(result.next.water_timer) : 0,
            result.next.water_timer_present,
            !result.remove_bird,
            result.remove_bird};
        for (size_t field = 0; field < 6; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Native retirement %zu field %zu: %08" PRIx32 " != %08" PRIx32
                        "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
        uint32_t events[4];
        size_t event_count = 0;
        if (result.query_definition) {
            events[event_count++] = 1;
        }
        if (result.query_speed_squared) {
            events[event_count++] = 2;
        }
        if (result.query_camera_target) {
            events[event_count++] = 3;
        }
        if (result.remove_bird) {
            events[event_count++] = 4;
        }
        CHECK(read_words(file, &count, 1) && count == event_count);
        for (size_t event = 0; event < event_count; ++event) {
            uint32_t native;
            CHECK(read_words(file, &native, 1) && native == events[event]);
        }
        definitions += result.query_definition;
        speeds += result.query_speed_squared;
        cameras += result.query_camera_target;
        removals += result.remove_bird;
    }
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB bird retirement reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 original ordinary bird-retirement cases match: %zu definition "
           "queries, %zu speed queries, %zu camera queries, %zu removals.\n",
           definitions, speeds, cameras, removals);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_bird_retirement_tests [NATIVE_RETIREMENT_TRACE]\n");
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
