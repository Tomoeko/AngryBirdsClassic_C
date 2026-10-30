#include "ab/game_circle.h"
#include "world_storage.h"
#include "../tools/oracle/reference_create_circle.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Circle construction failed at %d: %s\n", __LINE__,        \
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

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static AbGameCircleInput input_value(const uint32_t words[9]) {
    return (AbGameCircleInput){.name = span("circle"),
                               .position = {number(words[0]), number(words[1])},
                               .radius = number(words[2]),
                               .density = number(words[3]),
                               .friction = number(words[4]),
                               .restitution = number(words[5]),
                               .z_order = number(words[6]),
                               .controllable = words[7] != 0};
}

static bool snapshot(const AbPhysicsWorld *world, const AbGameCircle *circle,
                     uint32_t fields[38], uint32_t state[26]) {
    memset(fields, 0, 38 * sizeof(*fields));
    memset(state, 0, 26 * sizeof(*state));
    CHECK(circle->properties.physical.entries == circle->properties.physical_entries &&
          circle->properties.physical.count == 16 && !circle->properties.script.count &&
          !circle->properties.behaviors.count && !circle->properties.launch_published);
    for (size_t index = 0; index < 19; ++index) {
        const AbDataValue *field =
            ab_data_get(&circle->properties.physical,
                        (AbDataValue){.kind = AB_DATA_STRING,
                                      .string = span(create_circle_fields[index])});
        AbDataValue value = field ? *field : (AbDataValue){0};
        CHECK(value.kind == AB_DATA_NIL || value.kind == AB_DATA_NUMBER ||
              value.kind == AB_DATA_STRING);
        fields[index * 2] = value.kind == AB_DATA_NIL      ? 0
                            : value.kind == AB_DATA_NUMBER ? 3
                                                           : 4;
        if (value.kind == AB_DATA_NUMBER)
            fields[index * 2 + 1] = value.bits;
        else if (value.kind == AB_DATA_STRING) {
            CHECK(!value.string.size ||
                  (value.string.size == 6 && !memcmp(value.string.data, "circle", 6)));
            fields[index * 2 + 1] = value.string.size ? 1 : 2;
        }
    }
    const AbGameFrameObject *frame = &circle->frame;
    const AbBodyMassState *body =
        &world->storage.bodies[circle->physics.body].motion.mass;
    const AbWorldFixture *fixture = world->storage.fixtures + circle->physics.fixture;
    const AbBodyPose poses[] = {frame->history[0], frame->history[1], frame->rendered};
    for (size_t index = 0; index < 3; ++index) {
        state[index * 3] = bits(poses[index].position.x);
        state[index * 3 + 1] = bits(poses[index].position.y);
        state[index * 3 + 2] = bits(poses[index].angle);
    }
    state[9] = (uint32_t)body->type;
    state[10] = bits(body->mass);
    state[11] = bits(body->inverse_mass);
    state[12] = bits(body->inertia);
    state[13] = bits(body->inverse_inertia);
    state[14] = bits(fixture->friction);
    state[15] = bits(fixture->restitution);
    state[16] = bits(fixture->density);
    state[17] = bits(fixture->shape.data.circle.radius);
    state[18] = bits(fixture->shape.data.circle.center.x);
    state[19] = bits(fixture->shape.data.circle.center.y);
    state[20] = circle->physics.collider.bits;
    state[21] = circle->physics.collisions_enabled;
    state[22] = circle->physics.gravity_enabled;
    state[23] = circle->physics.controllable;
    state[24] = bits(body->transform.position.x);
    state[25] = bits(body->transform.position.y);
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index)
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1);
    return true;
}

static bool compare(const uint32_t *actual, const uint32_t *expected, size_t count,
                    unsigned sample, const char *label) {
    for (size_t index = 0; index < count; ++index) {
        if (actual[index] != expected[index]) {
            fprintf(stderr, "Circle %u %s[%zu]: %08" PRIx32 " != %08" PRIx32 "\n",
                    sample, label, index, actual[index], expected[index]);
            return false;
        }
    }
    return true;
}

static bool edge_checks(AbPhysicsWorld *world) {
    uint32_t words[9];
    reference_create_circle(2, words);
    AbGameCircleInput input = input_value(words);
    AbGameCircle output = {.initial_mass = 17}, prior = output;
    CHECK(!ab_game_circle_create(NULL, input, 0, &output, NULL) &&
          !ab_game_circle_create(world, input, 0, NULL, NULL) &&
          !memcmp(&output, &prior, sizeof(output)) && !world->body_count);
    input.radius = -1;
    CHECK(!ab_game_circle_create(world, input, 0, &output, NULL));
    input = input_value(words);
    input.position.x = NAN;
    CHECK(!ab_game_circle_create(world, input, 0, &output, NULL));
    input = input_value(words);
    input.density = INFINITY;
    CHECK(!ab_game_circle_create(world, input, 0, &output, NULL));
    input = input_value(words);
    input.z_order = NAN;
    CHECK(!ab_game_circle_create(world, input, 0, &output, NULL));
    input = input_value(words);
    input.name.data = NULL;
    CHECK(!ab_game_circle_create(world, input, 0, &output, NULL));
    input = input_value(words);
    world->locked = true;
    bool locked = ab_game_circle_create(world, input, 0, &output, NULL);
    world->locked = false;
    CHECK(!locked && !world->body_count && !world->fixture_count &&
          !memcmp(&output, &prior, sizeof(output)));
    return true;
}

#if defined(AB_TEST_RESIZE_CIRCLE)
static bool resize_edges(AbPhysicsWorld *world) {
    uint32_t words[9];
    reference_create_circle(2, words);
    AbGameCircleInput input = input_value(words);
    input.radius = input.density = 1;
    AbGameCircle circle = {0}, other = {0};
    CHECK(ab_game_circle_create(world, input, (uintptr_t)&circle, &circle, NULL));
    input.name = span("other");
    input.position.x = 1;
    input.density = 0;
    CHECK(ab_game_circle_create(world, input, (uintptr_t)&other, &other, NULL));
    size_t joint;
    AbWorldSolveJoint definition = {.first_body = circle.physics.body,
                                    .second_body = other.physics.body,
                                    .constraint.kind = AB_JOINT_WELD,
                                    .collide_connected = true};
    CHECK(ab_world_create_joint(world, &definition, &joint, NULL));
    CHECK(ab_world_step(world, 0, 8, 3, NULL) && world->contact_count);
    size_t body = circle.physics.body;
    AbBodyMotion *motion = &world->storage.bodies[body].motion;
    CHECK(ab_body_velocity_set(motion, (AbVec2){7, -3}) &&
          ab_body_angular_velocity_set(motion, 2));
    AbDataTable *physical = &circle.properties.physical;
    AbObjectBody prior = circle.physics;
    world->locked = true;
    bool resized = ab_game_circle_resize(world, &circle, 2, NULL);
    world->locked = false;
    CHECK(!resized && !memcmp(&prior, &circle.physics, sizeof(prior)) &&
          !ab_game_circle_resize(world, &circle, NAN, NULL) &&
          !ab_game_circle_resize(world, &circle, -1, NULL));
    CHECK(ab_world_set_fixture_filter(world, circle.physics.fixture,
                                      (AbCollisionFilter){8, 8, -2}, NULL) &&
          ab_world_set_fixture_sensor(world, circle.physics.fixture, true));
    CHECK(ab_game_circle_resize(world, &circle, 2, NULL));
    const AbWorldFixture *fixture = world->storage.fixtures + circle.physics.fixture;
    CHECK(world->body_count == 2 && circle.physics.body == body &&
          ab_world_has_joint(world, joint) && world->joint_count == 1 &&
          !world->contact_count && !fixture->sensor && fixture->filter.category == 1 &&
          fixture->filter.mask == 0xffff && fixture->filter.group == 0 &&
          fixture->user_data == (uintptr_t)&circle &&
          &circle.properties.physical == physical &&
          motion->mass.linear_velocity.x == 7 && motion->mass.linear_velocity.y == -3 &&
          motion->mass.angular_velocity == 2);
    float mass = motion->mass.mass;
    CHECK(ab_game_circle_resize(world, &circle, 1, NULL) &&
          motion->mass.mass == mass * .25f && ab_world_has_joint(world, joint));
    CHECK(ab_world_destroy_body(world, body, NULL) &&
          ab_world_destroy_body(world, other.physics.body, NULL));
    return true;
}
#endif

static bool run(FILE *reference) {
    WorldStorage storage;
    AbPhysicsWorld world = {0};
    CHECK(ab_world_open(&world, default_definition(), prepare_storage(&storage, 0, 0),
                        NULL));
    bool okay = edge_checks(&world);
#if defined(AB_TEST_RESIZE_CIRCLE)
    okay = okay && resize_edges(&world);
#endif
    if (reference) {
        char header[128];
        okay = okay && fgets(header, sizeof(header), reference) &&
               !strcmp(header,
#if defined(AB_TEST_RESIZE_CIRCLE)
                       "AB_ANDROID_RESIZE_CIRCLE_V1 128\n"
#else
                       "AB_ANDROID_CREATE_CIRCLE_V1 128\n"
#endif
               );
    }
    for (unsigned sample = 0; okay && sample < CREATE_CIRCLE_CASES; ++sample) {
        uint32_t words[9], expected_fields[38], expected_state[26];
        reference_create_circle(sample, words);
#if defined(AB_TEST_RESIZE_CIRCLE)
        uint32_t radius = reference_resize_circle(sample);
#endif
        if (reference) {
            uint32_t native_input[9];
            okay = read_words(reference, native_input, 9) &&
                   compare(words, native_input, 9, sample, "input");
#if defined(AB_TEST_RESIZE_CIRCLE)
            uint32_t expected_radius;
            okay = okay && read_words(reference, &expected_radius, 1) &&
                   compare(&radius, &expected_radius, 1, sample, "radius");
#endif
            okay = okay && read_words(reference, expected_fields, 38) &&
                   read_words(reference, expected_state, 26);
        }
        if (!okay)
            break;
        AbGameCircle circle = {0};
        AbError error = {0};
        uint32_t fields[38], state[26];
        okay = ab_game_circle_create(&world, input_value(words), (uintptr_t)&circle,
                                     &circle, &error);
#if defined(AB_TEST_RESIZE_CIRCLE)
        okay = okay && ab_game_circle_resize(&world, &circle, number(radius), &error);
#endif
        okay = okay && snapshot(&world, &circle, fields, state);
        if (!okay)
            fprintf(stderr, "Circle %u: %s\n", sample, error.message);
        if (okay && reference)
            okay = compare(fields, expected_fields, 38, sample, "field") &&
                   compare(state, expected_state, 26, sample, "state");
        if (okay)
            okay = circle.physics.collisions_enabled &&
                   circle.physics.gravity_enabled &&
                   circle.physics.dimensions.width == -1 &&
                   circle.physics.dimensions.height == -1 &&
                   storage.fixtures[circle.physics.fixture].user_data ==
                       (uintptr_t)&circle &&
                   bits(circle.initial_mass) == state[10];
        if (world.body_count)
            okay = ab_world_destroy_body(&world, circle.physics.body, NULL) && okay;
    }
    ab_world_close(&world);
    return okay;
}

int main(int argc, char **argv) {
    if (argc > 2)
        return 1;
    FILE *reference = argc == 2 ? fopen(argv[1], "r") : NULL;
    if (argc == 2 && !reference)
        return 1;
    bool okay = run(reference);
    if (reference)
        fclose(reference);
    if (okay)
#if defined(AB_TEST_RESIZE_CIRCLE)
        puts("Native circle fixture recreation: 128 cases pass");
#else
        puts("Native circle construction: 128 cases pass");
#endif
    return okay ? 0 : 1;
}
