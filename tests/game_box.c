#include "ab/game_box.h"
#include "world_storage.h"
#include "../tools/oracle/reference_create_box.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Box construction failed at %d: %s\n", __LINE__,           \
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

static AbGameBoxInput input_value(const uint32_t words[11]) {
    return (AbGameBoxInput){.name = span("box"),
                            .position = {number(words[0]), number(words[1])},
                            .size = {number(words[2]), number(words[3])},
                            .density = number(words[4]),
                            .friction = number(words[5]),
                            .restitution = number(words[6]),
                            .z_order = number(words[7]),
                            .collision = words[8] != 0};
}

static bool snapshot(const AbPhysicsWorld *world, const AbGameBox *box,
                     uint32_t fields[38], uint32_t state[43]) {
    memset(fields, 0, 38 * sizeof(*fields));
    memset(state, 0, 43 * sizeof(*state));
    CHECK(box->properties.physical.entries == box->properties.physical_entries &&
          box->properties.physical.count == 16 && !box->properties.script.count &&
          !box->properties.behaviors.count && !box->properties.launch_published);
    for (size_t index = 0; index < 19; ++index) {
        const AbDataValue *field =
            ab_data_get(&box->properties.physical,
                        (AbDataValue){.kind = AB_DATA_STRING,
                                      .string = span(create_box_fields[index])});
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
                  (value.string.size == 3 && !memcmp(value.string.data, "box", 3)));
            fields[index * 2 + 1] = value.string.size ? 1 : 2;
        }
    }
    const AbGameFrameObject *frame = &box->frame;
    const AbBodyMassState *body = &world->storage.bodies[box->physics.body].motion.mass;
    const AbWorldFixture *fixture = world->storage.fixtures + box->physics.fixture;
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
    state[17] = bits(fixture->shape.data.polygon.radius);
    state[18] = bits(fixture->shape.data.polygon.centroid.x);
    state[19] = bits(fixture->shape.data.polygon.centroid.y);
    state[20] = box->physics.collider.bits;
    state[21] = box->physics.collisions_enabled;
    state[22] = box->physics.gravity_enabled;
    state[23] = box->physics.controllable;
    state[24] = bits(body->transform.position.x);
    state[25] = bits(body->transform.position.y);
    const AbPolygon *shape = &fixture->shape.data.polygon;
    state[26] = (uint32_t)shape->count;
    for (size_t index = 0; index < 4; ++index) {
        state[27 + index * 2] = bits(shape->vertices[index].x);
        state[28 + index * 2] = bits(shape->vertices[index].y);
        state[35 + index * 2] = bits(shape->normals[index].x);
        state[36 + index * 2] = bits(shape->normals[index].y);
    }
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
            fprintf(stderr, "Box %u %s[%zu]: %08" PRIx32 " != %08" PRIx32 "\n", sample,
                    label, index, actual[index], expected[index]);
            return false;
        }
    }
    return true;
}

static bool edge_checks(AbPhysicsWorld *world) {
    uint32_t words[11];
    reference_create_box(2, words);
    AbGameBoxInput input = input_value(words);
    AbGameBox output = {.initial_mass = 17}, prior = output;
    CHECK(!ab_game_box_create(NULL, input, 0, &output, NULL) &&
          !ab_game_box_create(world, input, 0, NULL, NULL) &&
          !memcmp(&output, &prior, sizeof(output)) && !world->body_count);
    input.size.x = -1;
    CHECK(!ab_game_box_create(world, input, 0, &output, NULL));
    input = input_value(words);
    input.position.x = NAN;
    CHECK(!ab_game_box_create(world, input, 0, &output, NULL));
    input = input_value(words);
    input.density = INFINITY;
    CHECK(!ab_game_box_create(world, input, 0, &output, NULL));
    input = input_value(words);
    input.z_order = NAN;
    CHECK(!ab_game_box_create(world, input, 0, &output, NULL));
    input = input_value(words);
    input.name.data = NULL;
    CHECK(!ab_game_box_create(world, input, 0, &output, NULL));
    input = input_value(words);
    world->locked = true;
    bool locked = ab_game_box_create(world, input, 0, &output, NULL);
    world->locked = false;
    CHECK(!locked && !world->body_count && !world->fixture_count &&
          !memcmp(&output, &prior, sizeof(output)));
    return true;
}

static bool run(FILE *reference) {
    WorldStorage storage;
    AbPhysicsWorld world = {0};
    CHECK(ab_world_open(&world, default_definition(), prepare_storage(&storage, 0, 0),
                        NULL));
    bool okay = edge_checks(&world);
    if (reference) {
        char header[128];
        okay = okay && fgets(header, sizeof(header), reference) &&
               !strcmp(header, "AB_ANDROID_CREATE_BOX_V1 256\n");
    }
    for (unsigned sample = 0; okay && sample < CREATE_BOX_CASES; ++sample) {
        uint32_t words[11], expected_fields[38], expected_state[43];
        reference_create_box(sample, words);
        if (reference) {
            uint32_t native_input[11];
            okay = read_words(reference, native_input, 11) &&
                   compare(words, native_input, 11, sample, "input") &&
                   read_words(reference, expected_fields, 38) &&
                   read_words(reference, expected_state, 43);
        }
        if (!okay)
            break;
        AbGameBox box = {0};
        AbError error = {0};
        uint32_t fields[38], state[43];
        okay = ab_game_box_create(&world, input_value(words), (uintptr_t)&box, &box,
                                  &error) &&
               snapshot(&world, &box, fields, state);
        if (!okay)
            fprintf(stderr, "Box %u: %s\n", sample, error.message);
        if (okay && reference)
            okay = compare(fields, expected_fields, 38, sample, "field") &&
                   compare(state, expected_state, 43, sample, "state");
        if (okay)
            okay = box.physics.collisions_enabled == input_value(words).collision &&
                   box.physics.gravity_enabled &&
                   box.physics.dimensions.width == input_value(words).size.x &&
                   box.physics.dimensions.height == input_value(words).size.y &&
                   box.physics.dimensions.radius == -1 &&
                   storage.fixtures[box.physics.fixture].user_data == (uintptr_t)&box &&
                   bits(box.initial_mass) == state[10];
        if (world.body_count)
            okay = ab_world_destroy_body(&world, box.physics.body, NULL) && okay;
    }
    if (okay && reference) {
        char completion[80];
        okay = fscanf(reference, " ") == 0 &&
               fgets(completion, sizeof(completion), reference) &&
               strcmp(completion, "AB create box reference complete\n") == 0 &&
               fgetc(reference) == EOF && !ferror(reference);
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
    if (reference && fclose(reference) != 0)
        okay = false;
    if (okay)
        puts("Box construction: 256 cases pass");
    return okay ? 0 : 1;
}
