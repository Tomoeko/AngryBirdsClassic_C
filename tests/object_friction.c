#include "ab/object_friction.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Object friction failed at line %d: %s\n", __LINE__,       \
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
    AbBodyMotion body = {.mass = {.type = AB_BODY_DYNAMIC,
                                  .linear_velocity = {2, -3},
                                  .transform.cosine = 1},
                         .force = {1, 2},
                         .torque = -0.0f,
                         .sleep_time = 7};
    AbObjectFriction settings = {.air_resistance = 1, .relative = {NAN, NAN}};
    AbError error = {0};
    CHECK(ab_object_friction_apply(&body, &settings, &error));
    CHECK(body.awake && body.sleep_time == 0 && body.force.x == -1 &&
          body.force.y == 5 && to_bits(body.torque) == 0);
    body = (AbBodyMotion){.mass = {.type = AB_BODY_DYNAMIC,
                                   .linear_velocity = {2, 0},
                                   .transform.cosine = 1},
                          .sleep_time = 9};
    settings = (AbObjectFriction){.relative = {1, 0}};
    AbBodyMotion saved = body;
    CHECK(ab_object_friction_apply(&body, &settings, &error));
    CHECK(memcmp(&body, &saved, sizeof(body)) == 0);
    settings.relative.x = -1;
    CHECK(ab_object_friction_apply(&body, &settings, &error));
    CHECK(body.awake && body.sleep_time == 0 && body.force.x == -4);
    body.mass.type = AB_BODY_STATIC;
    body.mass.linear_velocity.x = NAN;
    saved = body;
    CHECK(ab_object_friction_apply(&body, &settings, &error));
    CHECK(memcmp(&body, &saved, sizeof(body)) == 0);
    body.mass.type = AB_BODY_DYNAMIC;
    saved = body;
    CHECK(!ab_object_friction_apply(&body, &settings, &error));
    CHECK(memcmp(&body, &saved, sizeof(body)) == 0);
    body.mass.linear_velocity.x = FLT_MAX;
    settings.air_resistance = FLT_MAX;
    saved = body;
    CHECK(!ab_object_friction_apply(&body, &settings, &error));
    CHECK(memcmp(&body, &saved, sizeof(body)) == 0);
    CHECK(!ab_object_friction_apply(NULL, &settings, &error));
    CHECK(!ab_object_friction_apply(&body, NULL, &error));
    body = (AbBodyMotion){.mass.type = AB_BODY_DYNAMIC, .sleep_time = 7};
    settings = (AbObjectFriction){.air_resistance = 1};
    CHECK(ab_object_friction_apply(&body, &settings, &error));
    CHECK(body.awake && body.sleep_time == 0 && body.force.x == 0);
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
    CHECK(strcmp(marker, "AB_ANDROID_OBJECT_FRICTION_V1 4096\n") == 0);
    size_t changed = 0, wakes = 0, air = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[17], expected[5];
        CHECK(read_words(file, input, 17) && read_words(file, expected, 5));
        CHECK(input[0] <= 2 && input[1] <= 1 && input[16] <= 7);
        AbBodyMotion body = {
            .mass = {.type = (AbBodyType)input[0],
                     .transform = {.sine = from_bits(input[6]),
                                   .cosine = from_bits(input[7])},
                     .linear_velocity = {from_bits(input[8]), from_bits(input[9])},
                     .center = {from_bits(input[10]), from_bits(input[11])}},
            .force = {from_bits(input[12]), from_bits(input[13])},
            .torque = from_bits(input[14]),
            .sleep_time = from_bits(input[2]),
            .awake = input[1] != 0,
        };
        AbBodyMotion before = body;
        AbObjectFriction settings = {
            .air_resistance = input[16] & 1 ? from_bits(input[3]) : 0,
            .relative = {input[16] & 2 ? from_bits(input[4]) : 0,
                         input[16] & 4 ? from_bits(input[5]) : 0},
        };
        air += settings.air_resistance != 0;
        AbError error = {0};
        CHECK(ab_object_friction_apply(&body, &settings, &error));
        uint32_t actual[] = {36 | (body.awake ? 2u : 0u), to_bits(body.force.x),
                             to_bits(body.force.y), to_bits(body.torque),
                             to_bits(body.sleep_time)};
        for (size_t field = 0; field < 5; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Native case %zu field %zu: %08" PRIx32 " instead of %08" PRIx32
                        "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
        changed += memcmp(&body.force, &before.force, sizeof(body.force)) != 0;
        wakes += body.awake && !before.awake;
        before.force = body.force;
        before.torque = body.torque;
        before.awake = body.awake;
        before.sleep_time = body.sleep_time;
        CHECK(memcmp(&before, &body, sizeof(body)) == 0);
    }
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB object friction reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    CHECK(changed > 0 && wakes > 0 && air > 0);
    printf("All 4096 native object-friction cases match: %zu force changes, "
           "%zu wakes, %zu nonzero air settings.\n",
           changed, wakes, air);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fputs("Usage: ab_object_friction_tests [NATIVE_TRACE]\n", stderr);
        return 2;
    }
    bool okay = edge_checks();
    if (okay && argc == 2) {
        FILE *file = fopen(argv[1], "rb");
        if (!file) {
            perror(argv[1]);
            return 1;
        }
        okay = native_checks(file);
        if (fclose(file) != 0) {
            okay = false;
        }
    }
    return okay ? 0 : 1;
}
