#include "ab/object_transform.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Object transform failed at %d: %s\n", __LINE__,           \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbPhysicsWorld world;
    AbWorldSolveBody body;
    AbWorldSolveWorkspace solve;
} TransformTest;

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

static bool open_world(TransformTest *test) {
    test->solve.body_capacity = 1;
    return ab_world_open(&test->world,
                         (AbWorldDefinition){.broadphase_limits = {16, 16, 16},
                                             .maximum_translation = 2,
                                             .maximum_translation_squared = 4},
                         (AbWorldStorage){.bodies = &test->body,
                                          .body_capacity = 1,
                                          .solve = &test->solve},
                         NULL);
}

static bool edge_checks(TransformTest *test) {
    AbBodyDefinition definition = {.type = AB_BODY_DYNAMIC,
                                   .position = {1, 2},
                                   .angle = 0.5f,
                                   .linear_velocity = {3, 4},
                                   .angular_velocity = 5,
                                   .active = true,
                                   .gravity_scale = 1};
    size_t body;
    CHECK(ab_world_create_body(&test->world, &definition, &body, NULL));
    AbGameFrameObject frame = {.body = body,
                               .history = {{{6, 7}, 8}, {{9, 10}, 11}},
                               .rendered = {{12, 13}, 14}};
    AbBodyPose script = {{15, 16}, 17};
    AbGameFrameObject prior = frame;
    CHECK(!ab_object_position_set(NULL, &frame, &script.position, (AbVec2){0}, NULL) &&
          !ab_object_position_set(&test->world, NULL, &script.position, (AbVec2){0},
                                  NULL) &&
          !ab_object_position_set(&test->world, &frame, NULL, (AbVec2){0}, NULL) &&
          !ab_object_rotation_set(&test->world, &frame, NULL, 0, NULL));
    test->world.locked = true;
    CHECK(!ab_object_position_set(&test->world, &frame, &script.position,
                                  (AbVec2){5, 6}, NULL));
    test->world.locked = false;
    CHECK(!ab_object_position_set(&test->world, &frame, &script.position,
                                  (AbVec2){NAN, 0}, NULL) &&
          !ab_object_rotation_set(&test->world, &frame, &script.angle, NAN, NULL) &&
          memcmp(&frame, &prior, sizeof(frame)) == 0 && script.position.x == 15 &&
          script.angle == 17 && test->body.motion.mass.transform.position.x == 1);
    CHECK(ab_object_position_set(&test->world, &frame, &script.position,
                                 (AbVec2){-0.0f, 5}, NULL) &&
          bits(frame.history[0].position.x) == UINT32_C(0x80000000) &&
          frame.history[0].angle == 8 && frame.history[1].angle == 11 &&
          frame.rendered.angle == 14 && script.angle == 17);
    CHECK(ab_object_rotation_set(&test->world, &frame, &script.angle, -0.5f, NULL) &&
          frame.history[0].position.y == 5 && frame.history[1].position.y == 5 &&
          frame.rendered.position.y == 5 && script.position.y == 5 &&
          frame.history[0].angle == script.angle &&
          frame.history[1].angle == script.angle &&
          frame.rendered.angle == script.angle && script.angle > 0);
    CHECK(!test->body.motion.awake && test->body.motion.mass.linear_velocity.x == 3 &&
          test->body.motion.mass.angular_velocity == 5 &&
          ab_world_destroy_body(&test->world, body, NULL));
    prior = frame;
    AbBodyPose previous = script;
    CHECK(ab_object_position_set(&test->world, &frame, &script.position,
                                 (AbVec2){NAN, INFINITY}, NULL) &&
          ab_object_rotation_set(&test->world, &frame, &script.angle, NAN, NULL) &&
          memcmp(&frame, &prior, sizeof(frame)) == 0 &&
          memcmp(&script, &previous, sizeof(script)) == 0);
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static AbBodyPose pose(const uint32_t *input) {
    return (AbBodyPose){{number(input[0]), number(input[1])}, number(input[2])};
}

static bool native_checks(TransformTest *test, FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file) &&
          strcmp(marker, "AB_ANDROID_OBJECT_TRANSFORM_V1 4096\n") == 0);
    size_t operations[2] = {0}, missing = 0;
    for (size_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[24], expected[24];
        CHECK(words(file, input, 24) && words(file, expected, 24) && input[0] <= 1 &&
              input[1] <= 1 && input[2] <= 2 && input[3] <= 1);
        AbBodyDefinition definition = {.type = (AbBodyType)input[2],
                                       .position = {number(input[4]), number(input[5])},
                                       .angle = number(input[6]),
                                       .sleep_allowed = true,
                                       .awake = input[3] != 0,
                                       .active = true,
                                       .gravity_scale = 1};
        size_t body;
        CHECK(ab_world_create_body(&test->world, &definition, &body, NULL));
        test->body.motion.mass.local_center =
            (AbVec2){number(input[7]), number(input[8])};
        AbGameFrameObject frame = {.body = input[1] ? body : SIZE_MAX,
                                   .history = {pose(input + 9), pose(input + 12)},
                                   .rendered = pose(input + 15)};
        AbBodyPose script = pose(input + 18);
        CHECK(input[0] ? ab_object_rotation_set(&test->world, &frame, &script.angle,
                                                number(input[23]), NULL)
                       : ab_object_position_set(
                             &test->world, &frame, &script.position,
                             (AbVec2){number(input[21]), number(input[22])}, NULL));
        const AbBodyMassState *mass = &test->body.motion.mass;
        uint32_t actual[24] = {bits(mass->transform.position.x),
                               bits(mass->transform.position.y),
                               bits(mass->transform.sine),
                               bits(mass->transform.cosine),
                               bits(mass->previous_center.x),
                               bits(mass->previous_center.y),
                               bits(mass->center.x),
                               bits(mass->center.y),
                               bits(mass->previous_angle),
                               bits(mass->angle),
                               (uint32_t)mass->type,
                               test->body.motion.awake};
        const AbBodyPose poses[] = {frame.history[0], frame.history[1], frame.rendered,
                                    script};
        for (size_t index = 0; index < 4; ++index) {
            actual[12 + index * 3] = bits(poses[index].position.x);
            actual[13 + index * 3] = bits(poses[index].position.y);
            actual[14 + index * 3] = bits(poses[index].angle);
        }
        for (size_t field = 0; field < 24; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Transform sample %zu word %zu: %08" PRIx32 " != %08" PRIx32
                        "\n",
                        sample, field, actual[field], expected[field]);
                return false;
            }
        }
        ++operations[input[0]];
        missing += !input[1];
        CHECK(ab_world_destroy_body(&test->world, body, NULL));
    }
    CHECK(fgets(marker, sizeof(marker), file) &&
          strcmp(marker, "AB object transform reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Matched %zu native position and %zu rotation assignments, including %zu "
           "missing-body no-ops and all script/pose/history words.\n",
           operations[0], operations[1], missing);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_object_transform_tests [NATIVE_TRANSFORM_TRACE]\n");
        return 2;
    }
    TransformTest test = {0};
    FILE *file = argc == 2 ? fopen(argv[1], "rb") : NULL;
    bool okay = (argc == 1 || file) && open_world(&test) && edge_checks(&test) &&
                (!file || native_checks(&test, file));
    ab_world_close(&test.world);
    if (file && fclose(file)) {
        okay = false;
    }
    return okay ? 0 : 1;
}
