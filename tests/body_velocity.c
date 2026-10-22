#include "ab/physics_step.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Velocity failed at line %d: %s\n", __LINE__, #condition); \
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
                                  .transform.position = {3, 1},
                                  .center = {1, 1},
                                  .inverse_mass = 0.5f,
                                  .inverse_inertia = 2},
                         .sleep_time = 7};
    CHECK(ab_body_impulse_apply(&body, (AbVec2){0, 2}, NULL));
    CHECK(body.awake && body.sleep_time == 0 && body.mass.linear_velocity.y == 1 &&
          body.mass.angular_velocity == 8);
    body.awake = false;
    body.sleep_time = 7;
    CHECK(ab_body_velocity_set(&body, (AbVec2){FLT_TRUE_MIN, -FLT_TRUE_MIN}));
    CHECK(!body.awake && body.sleep_time == 7 &&
          to_bits(body.mass.linear_velocity.y) == UINT32_C(0x80000001));
    CHECK(ab_body_velocity_set(&body, (AbVec2){1, 0}));
    CHECK(body.awake && body.sleep_time == 0);
    body.sleep_time = 9;
    CHECK(ab_body_velocity_set(&body, (AbVec2){2, 0}));
    CHECK(body.sleep_time == 9);
    body.awake = false;
    CHECK(ab_body_impulse_apply(&body, (AbVec2){0}, NULL));
    CHECK(body.awake && body.sleep_time == 0);
    body.mass.center = (AbVec2){0};
    body.mass.transform.position = (AbVec2){0};
    body.mass.angular_velocity = -0.0f;
    CHECK(ab_body_impulse_apply(&body, (AbVec2){0}, NULL));
    CHECK(to_bits(body.mass.angular_velocity) == 0);
    body.mass.type = AB_BODY_STATIC;
    CHECK(ab_body_velocity_set(&body, (AbVec2){3, 4}));
    CHECK(body.mass.linear_velocity.x == 2 && body.mass.linear_velocity.y == 0);
    body.mass.type = AB_BODY_KINEMATIC;
    CHECK(ab_body_impulse_apply(&body, (AbVec2){3, 4}, NULL));
    CHECK(body.mass.linear_velocity.x == 2 && body.mass.linear_velocity.y == 0);
    body.mass.type = AB_BODY_DYNAMIC;
    body.awake = false;
    body.sleep_time = 7;
    CHECK(ab_body_angular_velocity_set(&body, -FLT_TRUE_MIN));
    CHECK(!body.awake && body.sleep_time == 7 &&
          to_bits(body.mass.angular_velocity) == UINT32_C(0x80000001));
    CHECK(ab_body_angular_velocity_set(&body, 2));
    CHECK(body.awake && body.sleep_time == 0 && body.mass.angular_velocity == 2);
    body.mass.type = AB_BODY_STATIC;
    CHECK(ab_body_angular_velocity_set(&body, 3));
    CHECK(body.mass.angular_velocity == 2);
    body.mass.type = AB_BODY_KINEMATIC;
    CHECK(ab_body_angular_velocity_set(&body, -0.0f));
    CHECK(to_bits(body.mass.angular_velocity) == UINT32_C(0x80000000));
    body.mass.type = AB_BODY_DYNAMIC;
    body.mass.inverse_mass = -1;
    AbBodyMotion previous = body;
    CHECK(!ab_body_impulse_apply(&body, (AbVec2){1, 2}, NULL));
    CHECK(memcmp(&body, &previous, sizeof(body)) == 0);
    CHECK(!ab_body_velocity_set(&body, (AbVec2){NAN, 0}));
    CHECK(!ab_body_velocity_set(NULL, (AbVec2){0}));
    CHECK(!ab_body_angular_velocity_set(NULL, 0));
    CHECK(!ab_body_angular_velocity_set(&body, NAN));
    CHECK(!ab_body_impulse_apply(NULL, (AbVec2){0}, NULL));
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
    bool angular = strcmp(marker, "AB_ANDROID_BODY_ANGULAR_V1 4096\n") == 0;
    CHECK(angular || strcmp(marker, "AB_ANDROID_BODY_VELOCITY_V1 4096\n") == 0);
    size_t kinds[4] = {0}, wakes = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t data[17], expected[5];
        CHECK(read_words(file, data, 17) && data[0] <= 3 && data[1] <= 2 &&
              data[2] <= 1 && (angular ? data[0] == 3 : data[0] <= 2));
        AbBodyMotion body = {
            .mass = {.type = (AbBodyType)data[1],
                     .linear_velocity = {from_bits(data[3]), from_bits(data[4])},
                     .angular_velocity = from_bits(data[5]),
                     .inverse_mass = from_bits(data[7]),
                     .inverse_inertia = from_bits(data[8]),
                     .center = {from_bits(data[9]), from_bits(data[10])},
                     .transform.position = {from_bits(data[11]), from_bits(data[12])}},
            .awake = data[2] != 0,
            .sleep_time = from_bits(data[6]),
        };
        AbVec2 value = {from_bits(data[13]), from_bits(data[14])};
        AbVec2 point = {from_bits(data[15]), from_bits(data[16])};
        CHECK(data[0] == 3 ? ab_body_angular_velocity_set(&body, value.x)
              : data[0]
                  ? ab_body_impulse_apply(&body, value, data[0] == 2 ? &point : NULL)
                  : ab_body_velocity_set(&body, value));
        uint32_t actual[] = {
            36 | (body.awake ? 2u : 0u), to_bits(body.mass.linear_velocity.x),
            to_bits(body.mass.linear_velocity.y), to_bits(body.mass.angular_velocity),
            to_bits(body.sleep_time)};
        CHECK(read_words(file, expected, 5));
        for (size_t field = 0; field < 5; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Native velocity %zu field %zu: %08" PRIx32 " != %08" PRIx32
                        "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
        ++kinds[data[0]];
        wakes += !data[2] && body.awake;
    }
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, angular ? "AB body angular reference complete\n"
                                 : "AB body velocity reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 native body operations match: %zu velocity assignments, %zu/%zu "
           "origin/explicit impulses, %zu angular assignments, %zu wakes.\n",
           kinds[0], kinds[1], kinds[2], kinds[3], wakes);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_body_velocity_tests [NATIVE_BODY_VELOCITY_TRACE]\n");
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
