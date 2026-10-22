#include "ab/object_properties.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Object properties failed at %d: %s\n", __LINE__,          \
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
    AbBodyMotion body = {.mass = {.mass = NAN}};
    AbGameFrameObject frame = {.rendered = {{NAN, NAN}, NAN}};
    AbObjectPropertyPlan plan;
    CHECK(ab_object_properties_plan(&body, &frame, false, -10, 10, &plan, NULL) &&
          !plan.publish);
    body.mass = (AbBodyMassState){.mass = 1, .transform = {.sine = -0.0f, .cosine = 1}};
    frame.rendered.position = (AbVec2){-10, 20};
    CHECK(ab_object_properties_plan(&body, &frame, true, -10, 10, &plan, NULL) &&
          plan.publish && plan.values.sleeping && !plan.values.outside_boundaries &&
          bits(plan.values.pose.angle) == UINT32_C(0x80000000));
    frame.rendered.position.x = 10;
    CHECK(ab_object_properties_plan(&body, &frame, true, -10, 10, &plan, NULL) &&
          !plan.values.outside_boundaries);
    frame.rendered.position.y = nextafterf(20, INFINITY);
    CHECK(ab_object_properties_plan(&body, &frame, true, -10, 10, &plan, NULL) &&
          plan.values.outside_boundaries);
    frame.rendered.position.y = 20;
    frame.rendered.position.x = nextafterf(10, INFINITY);
    CHECK(ab_object_properties_plan(&body, &frame, true, -10, 10, &plan, NULL) &&
          plan.values.outside_boundaries);
    frame.rendered.position.x = nextafterf(-10, -INFINITY);
    CHECK(ab_object_properties_plan(&body, &frame, true, -10, 10, &plan, NULL) &&
          plan.values.outside_boundaries);
    body.mass.mass = 0;
    CHECK(ab_object_properties_plan(&body, &frame, true, -10, 10, &plan, NULL) &&
          !plan.values.outside_boundaries);
    body.mass.mass = 1;
    body.awake = true;
    frame.rendered.position.x = 0x1p31f;
    CHECK(ab_object_properties_plan(&body, &frame, false, INT32_MAX, INT32_MAX, &plan,
                                    NULL) &&
          !plan.values.sleeping && !plan.values.outside_boundaries);
    frame.rendered.position.x = nextafterf(0x1p31f, -INFINITY);
    CHECK(ab_object_properties_plan(&body, &frame, false, INT32_MAX, INT32_MAX, &plan,
                                    NULL) &&
          plan.values.outside_boundaries);
    AbObjectPropertyPlan prior = plan;
    body.mass.transform.sine = NAN;
    CHECK(!ab_object_properties_plan(&body, &frame, false, -10, 10, &plan, NULL) &&
          memcmp(&plan, &prior, sizeof(plan)) == 0 &&
          !ab_object_properties_plan(NULL, &frame, false, 0, 0, &plan, NULL) &&
          !ab_object_properties_plan(&body, NULL, false, 0, 0, &plan, NULL) &&
          !ab_object_properties_plan(&body, &frame, false, 0, 0, NULL, NULL));
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool native_checks(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file) &&
          strcmp(marker, "AB_LIVE_OBJECT_PROPERTIES_V1 3 23\n") == 0);
    size_t calls = 0;
    for (size_t frame_index = 0; frame_index < 3; ++frame_index) {
        uint32_t bounds[2];
        int32_t minimum, maximum;
        CHECK(words(file, bounds, 2));
        memcpy(&minimum, bounds, sizeof(minimum));
        memcpy(&maximum, bounds + 1, sizeof(maximum));
        for (size_t object = 0; object < 23; ++object) {
            char name[128];
            uint32_t input[9], expected[8];
            CHECK(fscanf(file, "%127s\n", name) == 1 && words(file, input, 9) &&
                  words(file, expected, 8) && input[0] <= 1 && input[1] <= 1);
            AbBodyMotion body = {
                .mass = {.mass = number(input[2]),
                         .transform = {.sine = number(input[3]),
                                       .cosine = number(input[4])},
                         .linear_velocity = {number(input[5]), number(input[6])}},
                .awake = input[1] != 0};
            AbGameFrameObject frame = {
                .rendered = {{number(input[7]), number(input[8])}, NAN}};
            AbObjectPropertyPlan plan;
            CHECK(ab_object_properties_plan(&body, &frame, input[0] != 0, minimum,
                                            maximum, &plan, NULL));
            uint32_t actual[8] = {plan.publish};
            if (plan.publish) {
                actual[1] = bits(plan.values.pose.position.x);
                actual[2] = bits(plan.values.pose.position.y);
                actual[3] = bits(plan.values.velocity.x);
                actual[4] = bits(plan.values.velocity.y);
                actual[5] = bits(plan.values.pose.angle);
                actual[6] = plan.values.sleeping;
                actual[7] = plan.values.outside_boundaries;
            }
            for (size_t index = 0; index < 8; ++index) {
                if (actual[index] != expected[index]) {
                    fprintf(stderr,
                            "Native properties frame %zu object %s word %zu: "
                            "%08" PRIx32 " != %08" PRIx32 "\n",
                            frame_index, name, index, actual[index], expected[index]);
                    return false;
                }
            }
            calls += plan.publish;
        }
    }
    CHECK(fgets(marker, sizeof(marker), file) &&
          strcmp(marker, "AB live object properties complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Matched 69 original object-property visits and all %zu published argument "
           "sets across three live native frames.\n",
           calls);
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
