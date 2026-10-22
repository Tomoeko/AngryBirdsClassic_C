#include "ab/classic_joint.h"

#include "game_joint_cases.h"
#include "game_joint_zero_cases.h"

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

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static float number(uint32_t value) {
    float result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, words + field) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool compare(const uint32_t *actual, const uint32_t *expected, size_t count,
                    uint32_t sample, const char *section) {
    for (size_t field = 0; field < count; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr,
                    "Game joint %" PRIu32 " %s field %zu: %08" PRIx32 " != %08" PRIx32
                    "\n",
                    sample, section, field, actual[field], expected[field]);
            return false;
        }
    }
    return true;
}

static AbDataValue key(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static void add(AbDataTable *table, const char *name, AbDataValue value) {
    table->entries[table->count++] = (AbDataEntry){key(name), value};
}

static void prepare_table(const uint32_t input[28], AbDataTable *table) {
    table->count = 0;
    add(table, "name", key("joint"));
    add(table, "end1", key("first"));
    add(table, "end2", key("second"));
    add(table, "type",
        (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits((float)input[1])});
    const char *const anchors[] = {"x1", "y1", "x2", "y2"};
    for (size_t index = 0; index < 4; ++index) {
        add(table, anchors[index],
            (AbDataValue){.kind = AB_DATA_NUMBER, .bits = input[10 + index]});
    }
    const char *const names[] = {
        "coordType",  "frequency",  "dampingRatio", "motorSpeed", "maxTorque",
        "lowerLimit", "upperLimit", "worldAxisX",   "worldAxisY", "breakForce"};
    const unsigned presence[] = {0, 1, 2, 6, 7, 8, 9, 11, 12, 14};
    const size_t positions[] = {3, 14, 15, 16, 17, 18, 19, 20, 21, 22};
    for (size_t index = 0; index < 10; ++index) {
        if (input[2] & (1u << presence[index])) {
            add(table, names[index],
                (AbDataValue){.kind = AB_DATA_NUMBER, .bits = input[positions[index]]});
        }
    }
    const char *const flags[] = {"collideConnected", "motor", "limit", "backAndForth",
                                 "breakable"};
    const unsigned flag_presence[] = {3, 4, 5, 10, 13};
    for (size_t index = 0; index < 5; ++index) {
        if (input[2] & (1u << flag_presence[index])) {
            add(table, flags[index],
                (AbDataValue){.kind = AB_DATA_BOOLEAN,
                              .bits = (input[23] >> index) & 1u});
        }
    }
    if (input[1] == 5 && (input[2] & 2u)) {
        add(table, "destroyTimer",
            (AbDataValue){.kind = AB_DATA_NUMBER, .bits = input[14]});
    }
}

static void project_definition(const AbClassicJoint *input, uint32_t output[20]) {
    const AbJointConstraint *joint = &input->joint.constraint;
    memset(output, 0, 20 * sizeof(uint32_t));
    output[0] = (uint32_t)joint->kind;
    output[1] = input->joint.collide_connected ? 1u : 0u;
    const AbVec2 *anchors;
    switch (joint->kind) {
        case AB_JOINT_DISTANCE:
            anchors = joint->definition.distance.local_anchors;
            output[6] = bits(joint->definition.distance.length);
            output[7] = bits(joint->definition.distance.frequency);
            output[8] = bits(joint->definition.distance.damping_ratio);
            break;
        case AB_JOINT_WELD:
            anchors = joint->definition.weld.local_anchors;
            output[6] = bits(joint->definition.weld.reference_angle);
            output[7] = bits(joint->definition.weld.frequency);
            output[8] = bits(joint->definition.weld.damping_ratio);
            break;
        case AB_JOINT_REVOLUTE:
            anchors = joint->definition.revolute.local_anchors;
            output[6] = bits(joint->definition.revolute.reference_angle);
            output[9] = bits(joint->definition.revolute.lower_angle);
            output[10] = bits(joint->definition.revolute.upper_angle);
            output[11] = bits(joint->definition.revolute.motor_speed);
            output[12] = bits(joint->definition.revolute.maximum_motor_torque);
            output[13] = joint->definition.revolute.enable_motor;
            output[14] = joint->definition.revolute.enable_limit;
            break;
        default:
            anchors = joint->definition.prismatic.local_anchors;
            output[6] = bits(joint->definition.prismatic.reference_angle);
            output[9] = bits(joint->definition.prismatic.lower_translation);
            output[10] = bits(joint->definition.prismatic.upper_translation);
            output[11] = bits(joint->definition.prismatic.motor_speed);
            output[12] = bits(joint->definition.prismatic.maximum_motor_force);
            output[13] = joint->definition.prismatic.enable_motor;
            output[14] = joint->definition.prismatic.enable_limit;
            output[15] = bits(joint->definition.prismatic.local.axis.x);
            output[16] = bits(joint->definition.prismatic.local.axis.y);
            output[17] = bits(joint->definition.prismatic.local.perpendicular.x);
            output[18] = bits(joint->definition.prismatic.local.perpendicular.y);
            break;
    }
    for (size_t index = 0; index < 2; ++index) {
        output[2 + index * 2] = bits(anchors[index].x);
        output[3 + index * 2] = bits(anchors[index].y);
    }
}

static void metadata_float(uint32_t output[42], size_t field, float value) {
    output[field * 2] = 3;
    output[field * 2 + 1] = bits(value);
}

static void metadata_flag(uint32_t output[42], size_t field, bool value) {
    output[field * 2] = 1;
    output[field * 2 + 1] = value ? 1u : 0u;
}

static void project_metadata(const AbClassicJoint *joint, const uint32_t definition[20],
                             uint32_t output[42]) {
    memset(output, 0, 42 * sizeof(uint32_t));
    metadata_float(output, 0, (float)joint->type);
    metadata_float(output, 1, (float)joint->coordinate_type);
    for (size_t index = 0; index < 2; ++index) {
        metadata_float(output, 2 + index * 2, joint->coordinates[index].x);
        metadata_float(output, 3 + index * 2, joint->coordinates[index].y);
    }
    if (joint->type == AB_CLASSIC_JOINT_GLUE) {
        metadata_float(output, 20, joint->destroy_timer);
    } else {
        metadata_flag(output, 8, joint->joint.collide_connected);
    }
    if (joint->type != AB_CLASSIC_JOINT_GLUE && definition[0] == AB_JOINT_DISTANCE) {
        metadata_float(output, 6, number(definition[7]));
        metadata_float(output, 7, number(definition[8]));
    } else if (joint->type != AB_CLASSIC_JOINT_GLUE &&
               definition[0] >= AB_JOINT_REVOLUTE) {
        metadata_flag(output, 9, definition[13] != 0);
        metadata_float(output, 10, number(definition[11]));
        metadata_float(output, 11, number(definition[12]));
        metadata_flag(output, 12, definition[14] != 0);
        metadata_float(output, 13, number(definition[9]));
        metadata_float(output, 14, number(definition[10]));
        metadata_flag(output, 15, joint->back_and_forth);
        if (definition[0] == AB_JOINT_PRISMATIC) {
            metadata_float(output, 16, joint->world_axis.x);
            metadata_float(output, 17, joint->world_axis.y);
        }
    }
    if (joint->breakable.kind == AB_DATA_BOOLEAN) {
        metadata_flag(output, 18, joint->breakable.bits != 0);
    }
    if (joint->break_force.kind == AB_DATA_NUMBER) {
        metadata_float(output, 19, number(joint->break_force.bits));
    }
}

static bool read_frames(FILE *file, const uint32_t input[28],
                        AbClassicJointSource *source, AbBodyMotion bodies[2],
                        uint32_t sample) {
    for (size_t index = 0; index < 2; ++index) {
        uint32_t expected[5];
        CHECK(read_words(file, expected, 5));
        AbBodyDefinition definition = {
            .type = AB_BODY_DYNAMIC,
            .position = {number(input[4 + index * 3]), number(input[5 + index * 3])},
            .angle = number(input[6 + index * 3]),
            .sleep_allowed = true,
            .gravity_scale = 1,
            .awake = true,
            .active = true};
        AbBodyMotion *body = bodies + index;
        CHECK(ab_body_initialize(&definition, body));
        source->bodies[index] =
            (AbJointBodyFrame){body->mass.transform, body->mass.angle};
        const uint32_t actual[] = {
            bits(body->mass.transform.position.x),
            bits(body->mass.transform.position.y), bits(body->mass.transform.sine),
            bits(body->mass.transform.cosine), bits(body->mass.angle)};
        CHECK(compare(actual, expected, 5, sample, "body frame"));
    }
    return true;
}

static bool edges(void) {
    uint32_t input[28];
    game_joint_case(3, input);
    AbDataEntry entries[24];
    AbDataTable table = {.entries = entries, .capacity = 24};
    prepare_table(input, &table);
    AbClassicJointSource source = {.table = &table,
                                   .body_indices = {9, 12},
                                   .bodies = {{{.cosine = 1}, 0}, {{.cosine = 1}, 0}},
                                   .legacy_world_anchors = {{NAN, NAN}, {NAN, NAN}}};
    AbClassicJoint output;
    CHECK(ab_classic_joint_read(&source, &output, NULL) &&
          output.joint.first_body == 9 && output.joint.second_body == 12);
    AbClassicJoint before = output;
    CHECK(!ab_classic_joint_read(NULL, &output, NULL) &&
          !ab_classic_joint_read(&source, NULL, NULL));
    source.body_indices[1] = 9;
    CHECK(!ab_classic_joint_read(&source, &output, NULL));
    source.body_indices[1] = 12;
    source.bodies[1].transform.position.x = INFINITY;
    CHECK(!ab_classic_joint_read(&source, &output, NULL));
    source.bodies[1].transform.position.x = 0;
    entries[3].value.bits = bits(6);
    CHECK(!ab_classic_joint_read(&source, &output, NULL));
    entries[3].value.bits = bits(NAN);
    CHECK(!ab_classic_joint_read(&source, &output, NULL));
    entries[3].value.bits = bits(4);
    entries[4].value.bits = bits(NAN);
    CHECK(!ab_classic_joint_read(&source, &output, NULL));
    CHECK(memcmp(&before, &output, sizeof(output)) == 0);
    game_joint_case(0, input);
    input[2] = 1;
    input[3] = bits(1);
    prepare_table(input, &table);
    CHECK(!ab_classic_joint_read(&source, &output, NULL) &&
          memcmp(&before, &output, sizeof(output)) == 0);
    input[1] = 5;
    prepare_table(input, &table);
    source.body_indices[1] = source.body_indices[0];
    source.bodies[0].transform.cosine = NAN;
    CHECK(ab_classic_joint_read(&source, &output, NULL) &&
          output.type == AB_CLASSIC_JOINT_GLUE && output.destroy_timer == 1);
    AbWorldSolveJoint definition = before.joint, original = definition;
    CHECK(!ab_classic_glue_definition(NULL, source.body_indices, &definition, NULL) &&
          !ab_classic_glue_definition(source.bodies, NULL, &definition, NULL) &&
          !ab_classic_glue_definition(source.bodies, source.body_indices, &definition,
                                      NULL) &&
          memcmp(&definition, &original, sizeof(definition)) == 0);
    return true;
}

static bool native_checks(FILE *file, bool glue, bool zeros) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, glue    ? "AB_ANDROID_GAME_GLUE_V1 4096\n"
                       : zeros ? "AB_ANDROID_GAME_JOINT_ZEROS_V1 4096\n"
                               : "AB_ANDROID_GAME_JOINT_V1 4096\n") == 0);
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[28], generated[28];
        CHECK(read_words(file, input, 28));
        if (zeros) {
            game_joint_zero_case(sample, generated);
        } else {
            game_joint_case(sample, generated);
        }
        if (glue) {
            generated[1] = 5;
        }
        CHECK(memcmp(input, generated, sizeof(input)) == 0);
        AbDataEntry entries[24];
        AbDataTable table = {.entries = entries, .capacity = 24};
        prepare_table(input, &table);
        AbClassicJointSource source = {
            .table = &table,
            .body_indices = {0, 1},
            .legacy_world_anchors = {{number(input[24]), number(input[25])},
                                     {number(input[26]), number(input[27])}}};
        AbBodyMotion bodies[2];
        CHECK(read_frames(file, input, &source, bodies, sample));
        AbClassicJoint joint;
        AbError error = {0};
        if (!ab_classic_joint_read(&source, &joint, &error)) {
            fprintf(stderr, "Game joint %" PRIu32 ": %s\n", sample, error.message);
            return false;
        }
        if (glue) {
            CHECK(ab_classic_glue_definition(source.bodies, source.body_indices,
                                             &joint.joint, &error));
        }
        uint32_t expected[20], actual[20], metadata[42], expected_metadata[42];
        CHECK(read_words(file, expected, 20));
        project_definition(&joint, actual);
        CHECK(compare(actual, expected, 20, sample, "definition"));
        size_t fields = glue ? 21 : 20;
        for (size_t field = 0; field < fields; ++field) {
            CHECK(read_words(file, expected_metadata + field * 2, 2));
        }
        project_metadata(&joint, actual, metadata);
        CHECK(compare(metadata, expected_metadata, fields * 2, sample, "metadata"));
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, glue    ? "AB game glue reference complete\n"
                       : zeros ? "AB game joint zeros reference complete\n"
                               : "AB game joint reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Compared 4,096 original game %s constructors: %zu definition/metadata "
           "words and 40,960 body-frame words\n",
           glue    ? "glue"
           : zeros ? "joint signed-zero"
                   : "joint",
           (size_t)4096 * (glue ? 62u : 60u));
    return true;
}

#include "game_glue_policy.h"
#include "game_joint_limits.h"
#include "game_joint_parameters.h"

int main(int argc, char **argv) {
    if (!edges() || !registry_edges() || !limit_edges() || !parameter_edges()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    bool glue = argc == 3 && strcmp(argv[1], "--glue") == 0;
    bool policy = argc == 3 && strcmp(argv[1], "--glue-policy") == 0;
    bool zeros = argc == 3 && strcmp(argv[1], "--zeros") == 0;
    bool limits = argc == 3 && strcmp(argv[1], "--limits") == 0;
    bool parameters = argc == 3 && strcmp(argv[1], "--parameters") == 0;
    if (argc != 2 && !glue && !policy && !zeros && !limits && !parameters) {
        fputs("Usage: ab_game_joint_tests [--glue|--glue-policy|--zeros|--limits|"
              "--parameters] "
              "NATIVE_CORPUS\n",
              stderr);
        return 2;
    }
    FILE *file =
        fopen(argv[glue || policy || zeros || limits || parameters ? 2 : 1], "rb");
    bool okay = file && (parameters ? parameters_native_checks(file)
                         : limits   ? limits_native_checks(file)
                         : policy   ? policy_native_checks(file)
                                    : native_checks(file, glue, zeros));
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
