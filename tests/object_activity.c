#include "ab/object_activity.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Object activity failed at %d: %s\n", __LINE__,            \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float number(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool edge_checks(void) {
    AbObjectActivityInput input = {.collider = 3,
                                   .velocity = {NAN, NAN},
                                   .angular_velocity = NAN,
                                   .pink_timer_present = true};
    AbObjectActivityPlan plan;
    CHECK(ab_object_activity_plan(&input, &plan, NULL) && plan.moving &&
          !plan.has_awake && !plan.above_tolerance && !plan.rolling_request);
    input.ignored_is_boolean = true;
    CHECK(ab_object_activity_plan(&input, &plan, NULL) && !plan.moving);
    input = (AbObjectActivityInput){.awake = true,
                                    .velocity = {3, 0},
                                    .angular_velocity = 0.1f,
                                    .mass = 400,
                                    .rolling_coefficient = 1};
    CHECK(ab_object_activity_plan(&input, &plan, NULL) && plan.has_awake &&
          !plan.moving && plan.above_tolerance && plan.rolling_request &&
          bits(plan.rolling_volume) == bits(0.1f));
    input.velocity.x = 0;
    input.angular_velocity = -1;
    CHECK(ab_object_activity_plan(&input, &plan, NULL) && !plan.moving &&
          !plan.above_tolerance && plan.rolling_volume == 1);
    input.angular_velocity = 0.1f;
    input.previous_rolling_volume = 0.75f;
    CHECK(ab_object_activity_plan(&input, &plan, NULL) && plan.rolling_volume == 0.75f);
    input.angular_velocity = -0.0f;
    input.previous_rolling_volume = -1;
    CHECK(ab_object_activity_plan(&input, &plan, NULL) &&
          bits(plan.rolling_volume) == UINT32_C(0x80000000));
    input.shape = 1;
    input.mass = input.rolling_coefficient = input.previous_rolling_volume = NAN;
    input.angular_velocity = 1;
    input.has_joints = true;
    CHECK(ab_object_activity_plan(&input, &plan, NULL) && !plan.moving &&
          !plan.rolling_request);
    input.bubble_is_boolean = true;
    input.bubble = true;
    CHECK(ab_object_activity_plan(&input, &plan, NULL) && plan.moving);
    input.bubble_is_boolean = false;
    CHECK(ab_object_activity_plan(&input, &plan, NULL) && !plan.moving);
    input.velocity.x = 4;
    input.ignored_is_boolean = true;
    CHECK(ab_object_activity_plan(&input, &plan, NULL) && !plan.moving &&
          !plan.above_tolerance && plan.has_awake);
    input.ignored_is_boolean = false;
    CHECK(ab_object_activity_plan(&input, &plan, NULL) && plan.moving);
    input.angular_velocity = NAN;
    input.velocity.x = FLT_MAX;
    CHECK(ab_object_activity_plan(&input, &plan, NULL) && plan.moving);
    input.velocity.x = 0;
    AbObjectActivityPlan prior = plan;
    CHECK(!ab_object_activity_plan(&input, &plan, NULL) &&
          memcmp(&plan, &prior, sizeof(plan)) == 0 &&
          !ab_object_activity_plan(NULL, &plan, NULL) &&
          !ab_object_activity_plan(&input, NULL, NULL));
    input = (AbObjectActivityInput){.previous_awake = true,
                                    .angular_velocity = FLT_MAX,
                                    .mass = FLT_MAX,
                                    .rolling_coefficient = FLT_MAX};
    CHECK(ab_object_activity_plan(&input, &plan, NULL) && plan.rolling_volume == 1);
    input.mass = 0;
    input.previous_rolling_volume = 0.25f;
    CHECK(ab_object_activity_plan(&input, &plan, NULL) && plan.rolling_volume == 0.25f);
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static AbObjectActivityInput from_words(const uint32_t words[15]) {
    return (AbObjectActivityInput){.collider = words[0],
                                   .shape = words[1],
                                   .awake = words[2] != 0,
                                   .previous_awake = words[3] != 0,
                                   .has_joints = words[4] != 0,
                                   .pink_timer_present = (words[5] & 1) != 0,
                                   .bomb_timer_present = (words[5] & 2) != 0,
                                   .shoot_delay_present = (words[5] & 4) != 0,
                                   .bubble_is_boolean = words[6] != 0,
                                   .bubble = words[7] != 0,
                                   .ignored_is_boolean = words[8] != 0,
                                   .velocity = {number(words[9]), number(words[10])},
                                   .angular_velocity = number(words[11]),
                                   .mass = number(words[12]),
                                   .rolling_coefficient = number(words[13]),
                                   .previous_rolling_volume = number(words[14])};
}

static bool native_checks(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file) &&
          strcmp(marker, "AB_NATIVE_OBJECT_ACTIVITY_V1 64 3 23\n") == 0);
    size_t processed = 0, rolling = 0;
    for (size_t sample = 0; sample < 64; ++sample) {
        uint32_t raw[15], expected[5];
        CHECK(words(file, raw, 15) && words(file, expected, 5));
        AbObjectActivityInput input = from_words(raw);
        AbObjectActivityPlan plan;
        CHECK(ab_object_activity_plan(&input, &plan, NULL));
        uint32_t actual[] = {plan.has_awake, plan.moving, plan.above_tolerance,
                             plan.rolling_request, bits(plan.rolling_volume)};
        for (size_t field = 0; field < 5; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Activity sample %zu word %zu: %08" PRIx32 " != %08" PRIx32
                        "\n",
                        sample, field, actual[field], expected[field]);
                return false;
            }
        }
        processed += plan.has_awake;
        rolling += plan.rolling_request;
    }
    for (size_t frame = 0; frame < 3; ++frame) {
        uint32_t actual[3] = {0}, expected[3];
        for (size_t object = 0; object < 23; ++object) {
            uint32_t raw[15];
            CHECK(words(file, raw, 15));
            AbObjectActivityInput input = from_words(raw);
            AbObjectActivityPlan plan;
            CHECK(ab_object_activity_plan(&input, &plan, NULL));
            actual[0] |= plan.has_awake;
            actual[1] |= plan.moving;
            actual[2] |= plan.above_tolerance;
        }
        CHECK(words(file, expected, 3) &&
              memcmp(actual, expected, sizeof(actual)) == 0);
    }
    CHECK(fgets(marker, sizeof(marker), file) &&
          strcmp(marker, "AB native object activity complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Matched 64 controlled native activity prefixes, %zu processed objects, "
           "%zu rolling requests and aggregate flags over 69 live object visits.\n",
           processed, rolling);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        return 2;
    }
    FILE *file = argc == 2 ? fopen(argv[1], "rb") : NULL;
    bool okay = (argc == 1 || file) && edge_checks() && (!file || native_checks(file));
    if (file && fclose(file)) {
        okay = false;
    }
    return okay ? 0 : 1;
}
