#include "ab/physics_filter.h"
#include "ab/physics_step.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition);          \
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
    AbBodyDefinition definition = {.type = AB_BODY_STATIC,
                                   .position = {2, -3},
                                   .linear_velocity = {4, 5},
                                   .angular_velocity = 6,
                                   .gravity_scale = 1,
                                   .fixed_rotation = true,
                                   .user_data = 17};
    AbBodyMotion body;
    CHECK(ab_body_initialize(&definition, &body));
    CHECK(body.mass.mass == 0 && body.mass.inverse_mass == 0);
    CHECK(body.mass.linear_velocity.x == 4 && body.mass.linear_velocity.y == 5 &&
          body.mass.angular_velocity == 6 && body.mass.fixed_rotation);
    CHECK(body.mass.transform.position.x == 2 && body.mass.center.y == -3 &&
          body.mass.transform.cosine == 1 && body.user_data == 17);
    definition.type = AB_BODY_DYNAMIC;
    CHECK(ab_body_initialize(&definition, &body));
    CHECK(body.mass.mass == 1 && body.mass.inverse_mass == 1 &&
          body.mass.inertia == 0 && body.mass.inverse_inertia == 0);
    AbBodyMotion unchanged = body;
    definition.linear_damping = -1;
    CHECK(!ab_body_initialize(&definition, &body));
    CHECK(memcmp(&body, &unchanged, sizeof(body)) == 0);
    definition.linear_damping = 0;
    definition.angle = INFINITY;
    CHECK(!ab_body_initialize(&definition, &body));
    definition.angle = from_bits(0x4dc90fdb);
    CHECK(!ab_body_initialize(&definition, &body));
    definition.angle = 0;
    definition.position.y = NAN;
    CHECK(!ab_body_initialize(&definition, &body));
    definition.position.y = 0;
    definition.type = (AbBodyType)-1;
    CHECK(!ab_body_initialize(&definition, &body));
    CHECK(!ab_body_initialize(NULL, &body));
    CHECK(!ab_body_initialize(&definition, NULL));
    CHECK(memcmp(&body, &unchanged, sizeof(body)) == 0);

    AbJointCollisionRule rules[] = {{2, false}, {1, true}, {1, false}};
    CHECK(!ab_body_can_collide(AB_BODY_STATIC, AB_BODY_KINEMATIC, 1, NULL, 0));
    CHECK(ab_body_can_collide(AB_BODY_STATIC, AB_BODY_DYNAMIC, 1, rules, 2));
    CHECK(!ab_body_can_collide(AB_BODY_STATIC, AB_BODY_DYNAMIC, 1, rules, 3));
    CHECK(!ab_body_can_collide(AB_BODY_DYNAMIC, AB_BODY_DYNAMIC, 1, NULL, 1));
    CHECK(!ab_body_can_collide((AbBodyType)-1, AB_BODY_DYNAMIC, 1, NULL, 0));
    CHECK(ab_fixture_filters_collide((AbCollisionFilter){0, 0, 1},
                                     (AbCollisionFilter){0, 0, 1}));
    CHECK(!ab_fixture_filters_collide((AbCollisionFilter){1, 1, -1},
                                      (AbCollisionFilter){1, 1, -1}));
    CHECK(ab_fixture_filters_collide((AbCollisionFilter){1, 2, -1},
                                     (AbCollisionFilter){2, 1, 1}));
    CHECK(!ab_fixture_filters_collide((AbCollisionFilter){1, 2, 0},
                                      (AbCollisionFilter){2, 0, 0}));
    return true;
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%8" SCNx32, words + index) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static AbBodyDefinition read_definition(const uint32_t words[13]) {
    return (AbBodyDefinition){
        .type = (AbBodyType)words[0],
        .position = {from_bits(words[1]), from_bits(words[2])},
        .angle = from_bits(words[3]),
        .linear_velocity = {from_bits(words[4]), from_bits(words[5])},
        .angular_velocity = from_bits(words[6]),
        .linear_damping = from_bits(words[7]),
        .angular_damping = from_bits(words[8]),
        .sleep_allowed = (words[9] & 0xff) != 0,
        .awake = (words[9] & 0xff00) != 0,
        .fixed_rotation = (words[9] & 0xff0000) != 0,
        .bullet = (words[9] & 0xff000000) != 0,
        .active = (words[10] & 0xff) != 0,
        .user_data = words[11],
        .gravity_scale = from_bits(words[12]),
    };
}

static void write_body(const AbBodyMotion *body, uint32_t words[64]) {
    const AbBodyMassState *mass = &body->mass;
    words[0] = (uint32_t)mass->type;
    words[1] = (words[1] & 0xffff0000) | (body->awake ? 2u : 0u) |
               (body->sleep_allowed ? 4u : 0u) | (body->bullet ? 8u : 0u) |
               (mass->fixed_rotation ? 16u : 0u) | (body->active ? 32u : 0u);
    words[3] = to_bits(mass->transform.position.x);
    words[4] = to_bits(mass->transform.position.y);
    words[5] = to_bits(mass->transform.sine);
    words[6] = to_bits(mass->transform.cosine);
    words[7] = to_bits(mass->local_center.x);
    words[8] = to_bits(mass->local_center.y);
    words[9] = to_bits(mass->previous_center.x);
    words[10] = to_bits(mass->previous_center.y);
    words[11] = to_bits(mass->center.x);
    words[12] = to_bits(mass->center.y);
    words[13] = to_bits(mass->previous_angle);
    words[14] = to_bits(mass->angle);
    words[15] = 0;
    words[16] = to_bits(mass->linear_velocity.x);
    words[17] = to_bits(mass->linear_velocity.y);
    words[18] = to_bits(mass->angular_velocity);
    words[19] = to_bits(body->force.x);
    words[20] = to_bits(body->force.y);
    words[21] = to_bits(body->torque);
    words[22] = 0x520abc00;
    for (size_t index = 23; index <= 28; ++index) {
        words[index] = 0;
    }
    words[29] = to_bits(mass->mass);
    words[30] = to_bits(mass->inverse_mass);
    words[31] = to_bits(mass->inertia);
    words[32] = to_bits(mass->inverse_inertia);
    words[33] = to_bits(body->linear_damping);
    words[34] = to_bits(body->angular_damping);
    words[35] = to_bits(body->gravity_scale);
    words[36] = to_bits(body->sleep_time);
    words[37] = (uint32_t)body->user_data;
}

static bool compare_body(FILE *file, size_t sample) {
    uint32_t definition_words[13], actual[64], expected[64];
    CHECK(read_words(file, definition_words, 13));
    CHECK(read_words(file, actual, 64) && read_words(file, expected, 64));
    AbBodyDefinition definition = read_definition(definition_words);
    AbBodyMotion body;
    CHECK(ab_body_initialize(&definition, &body));
    write_body(&body, actual);
    for (size_t field = 0; field < 64; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr,
                    "Body setup sample %zu field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    sample, field, actual[field], expected[field]);
            return false;
        }
    }
    return true;
}

static AbCollisionFilter read_filter(const uint32_t words[3]) {
    uint16_t group_bits = (uint16_t)words[2];
    int16_t group;
    memcpy(&group, &group_bits, sizeof(group));
    return (AbCollisionFilter){(uint16_t)words[0], (uint16_t)words[1], group};
}

static bool compare_filters(FILE *file, size_t sample) {
    uint32_t inputs[12], expected;
    for (size_t index = 0; index < 4; ++index) {
        CHECK(fscanf(file, "%8" SCNx32, inputs + index) == 1);
    }
    CHECK(inputs[2] <= 4);
    for (size_t index = 0; index < 2 * inputs[2]; ++index) {
        CHECK(fscanf(file, "%8" SCNx32, inputs + 4 + index) == 1);
    }
    CHECK(fgetc(file) == '\n');
    CHECK(read_words(file, &expected, 1));
    AbJointCollisionRule rules[4];
    for (size_t index = 0; index < inputs[2]; ++index) {
        rules[index] =
            (AbJointCollisionRule){inputs[4 + 2 * index], inputs[5 + 2 * index] != 0};
    }
    bool actual = ab_body_can_collide((AbBodyType)inputs[0], (AbBodyType)inputs[1],
                                      inputs[3], rules, inputs[2]);
    if ((uint32_t)actual != expected) {
        fprintf(stderr, "Body eligibility differs at sample %zu\n", sample);
        return false;
    }
    CHECK(read_words(file, inputs, 6) && read_words(file, &expected, 1));
    actual = ab_fixture_filters_collide(read_filter(inputs), read_filter(inputs + 3));
    if ((uint32_t)actual != expected) {
        fprintf(stderr, "Fixture filtering differs at sample %zu\n", sample);
        return false;
    }
    return true;
}

static bool native_checks(FILE *file) {
    char header[128];
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB_ANDROID_BODY_SETUP_V1 4096\n") == 0);
    for (size_t sample = 0; sample < 4096; ++sample) {
        CHECK(compare_body(file, sample) && compare_filters(file, sample));
    }
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB body setup reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    puts("All 4096 body images, eligibility results, and fixture filters match.");
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_body_setup_tests [NATIVE_BODY_SETUP_TRACE]\n");
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
