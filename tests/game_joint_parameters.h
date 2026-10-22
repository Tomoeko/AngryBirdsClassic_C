#ifndef AB_TEST_GAME_JOINT_PARAMETERS_H
#define AB_TEST_GAME_JOINT_PARAMETERS_H

#include "game_joint_parameter_cases.h"

static const char *const parameter_keys[] = {"motor",     "motorSpeed",  "maxTorque",
                                             "limit",     "lowerLimit",  "upperLimit",
                                             "frequency", "dampingRatio"};

static bool parameter_metadata(FILE *file, const AbClassicJoints *joints,
                               uint32_t sample) {
    const AbClassicJointRecord *record =
        ab_classic_joints_metadata(joints, key("joint").string);
    CHECK(record);
    uint32_t definition[20], metadata[42], expected[40];
    project_definition(&record->definition, definition);
    project_metadata(&record->definition, definition, metadata);
    const size_t positions[] = {9, 10, 11, 12, 13, 14, 6, 7};
    for (size_t field = 0; field < 8; ++field) {
        AbDataValue value = ab_classic_joint_parameter(record, 1u << field);
        size_t position = positions[field] * 2;
        metadata[position] = value.kind == AB_DATA_NUMBER    ? 3u
                             : value.kind == AB_DATA_BOOLEAN ? 1u
                                                             : 0;
        metadata[position + 1] = value.bits;
    }
    for (size_t field = 0; field < 20; ++field) {
        CHECK(read_words(file, expected + field * 2, 2));
    }
    return compare(metadata, expected, 40, sample, "updated saved metadata");
}

static void parameter_seed(AbJointConstraint *joint, uint32_t sample) {
    const AbVec3 impulse = {0.125f, -0.25f, 0.5f};
    switch (joint->kind) {
        case AB_JOINT_DISTANCE:
            joint->state.distance.impulse = impulse.x;
            break;
        case AB_JOINT_WELD:
            joint->state.weld.impulse = impulse;
            break;
        case AB_JOINT_REVOLUTE:
            joint->state.revolute.impulse = impulse;
            joint->state.revolute.motor_impulse = -0.75f;
            joint->state.revolute.limit = (AbJointLimitState)(sample % 4);
            break;
        case AB_JOINT_PRISMATIC:
            joint->state.prismatic.impulse = impulse;
            joint->state.prismatic.motor_impulse = -0.75f;
            joint->state.prismatic.limit = (AbJointLimitState)(sample % 4);
            break;
    }
}

static bool parameter_state_check(FILE *file, const AbJointConstraint *joint,
                                  uint32_t sample) {
    uint32_t actual[5] = {0}, expected[5];
    if (joint) {
        const AbVec3 *impulse = NULL;
        switch (joint->kind) {
            case AB_JOINT_DISTANCE:
                actual[0] = bits(joint->state.distance.impulse);
                break;
            case AB_JOINT_WELD:
                impulse = &joint->state.weld.impulse;
                break;
            case AB_JOINT_REVOLUTE:
                impulse = &joint->state.revolute.impulse;
                actual[3] = bits(joint->state.revolute.motor_impulse);
                actual[4] = (uint32_t)joint->state.revolute.limit;
                break;
            case AB_JOINT_PRISMATIC:
                impulse = &joint->state.prismatic.impulse;
                actual[3] = bits(joint->state.prismatic.motor_impulse);
                actual[4] = (uint32_t)joint->state.prismatic.limit;
                break;
        }
        if (impulse) {
            actual[0] = bits(impulse->x);
            actual[1] = bits(impulse->y);
            actual[2] = bits(impulse->z);
        }
    }
    CHECK(read_words(file, expected, 5));
    return compare(actual, expected, 5, sample, "parameter solver state");
}

static bool parameter_definition_check(FILE *file, const AbWorldSolveJoint *joint,
                                       uint32_t sample) {
    uint32_t actual[20] = {0}, expected[20];
    if (joint) {
        project_definition(&(AbClassicJoint){.joint = *joint}, actual);
    }
    CHECK(read_words(file, expected, 20));
    return compare(actual, expected, 20, sample, "parameter definition");
}

static bool parameter_update_case(FILE *file, uint32_t sample, uint32_t pass,
                                  AbClassicJoints *joints, size_t first,
                                  size_t second) {
    uint32_t types[8], values[8], generated_types[8], generated_values[8];
    CHECK(read_words(file, types, 8) && read_words(file, values, 8));
    game_joint_parameter_case(sample, pass, generated_types, generated_values);
    CHECK(memcmp(types, generated_types, sizeof(types)) == 0 &&
          memcmp(values, generated_values, sizeof(values)) == 0);
    AbPhysicsWorld *world = joints->world;
    AbJointConstraint *constraint = &world->storage.joints[first].constraint;
    parameter_seed(constraint, sample);
    AbJointConstraint before = *constraint;
    AbJointConstraint *duplicate =
        second == SIZE_MAX ? NULL : &world->storage.joints[second].constraint;
    AbJointConstraint before_duplicate = {0};
    if (duplicate) {
        parameter_seed(duplicate, sample);
        before_duplicate = *duplicate;
    }
    for (size_t index = 0; index < 2; ++index) {
        AbBodyMotion *body = &world->storage.bodies[index].motion;
        body->awake = ((sample >> (4 + index)) & 1u) != 0;
        body->sleep_time = index ? 0.125f : 0.25f;
    }
    AbDataEntry entries[8];
    AbDataTable table = {.entries = entries, .capacity = 8};
    for (size_t field = 0; field < 8; ++field) {
        if (types[field]) {
            AbDataValue value =
                types[field] == 4
                    ? key("invalid")
                    : (AbDataValue){.kind = types[field] == 1 ? AB_DATA_BOOLEAN
                                                              : AB_DATA_NUMBER,
                                    .bits = values[field]};
            add(&table, parameter_keys[field], value);
        }
    }
    world->locked = (sample & 256u) != 0;
    CHECK(ab_classic_joints_set_parameters(
        joints, key(sample & 2048u ? "absent" : "joint").string, &table, NULL));
    world->locked = false;
    CHECK(parameter_definition_check(file, world->storage.joints + first, sample) &&
          parameter_definition_check(
              file, duplicate ? world->storage.joints + second : NULL, sample) &&
          limit_bodies(file, world, sample) &&
          parameter_state_check(file, constraint, sample) &&
          parameter_state_check(file, duplicate, sample) &&
          parameter_metadata(file, joints, sample));
    if (constraint->kind == AB_JOINT_REVOLUTE) {
        before.state.revolute.impulse.z = constraint->state.revolute.impulse.z;
    } else if (constraint->kind == AB_JOINT_PRISMATIC) {
        before.state.prismatic.impulse.z = constraint->state.prismatic.impulse.z;
    }
    CHECK(
        memcmp(&before.state, &constraint->state, sizeof(before.state)) == 0 &&
        (!duplicate || memcmp(&before_duplicate, duplicate, sizeof(*duplicate)) == 0));
    return true;
}

static bool parameter_case(FILE *file, uint32_t sample, WorldStorage *memory,
                           AbPhysicsWorld *world) {
    uint32_t input[28], generated[28];
    CHECK(read_words(file, input, 28));
    game_joint_case(sample, generated);
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
    AbWorldStorage storage = prepare_storage(memory, 2, 0);
    for (size_t index = 0; index < 2; ++index) {
        memory->bodies[index].motion = bodies[index];
    }
    CHECK(ab_world_open(world, default_definition(), storage, NULL));
    AbClassicJointRecord records[2];
    AbClassicJoints joints;
    CHECK(ab_classic_joints_open(&joints, world,
                                 (AbClassicJointStorage){records, 2, NULL, 0}, NULL));
    AbClassicJoint definition;
    size_t first, second = SIZE_MAX;
    CHECK(ab_classic_joint_read(&source, &definition, NULL) &&
          ab_classic_joints_create(&joints, &definition, &first, NULL) &&
          parameter_definition_check(file, memory->joints + first, sample));
    uint32_t second_type;
    CHECK(read_words(file, &second_type, 1));
    CHECK(second_type ==
          ((sample & 512u) ? (sample & 1024u ? input[1] % 4 + 1 : input[1]) : 0));
    if (second_type) {
        input[1] = second_type;
        prepare_table(input, &table);
        CHECK(ab_classic_joint_read(&source, &definition, NULL) &&
              ab_classic_joints_create(&joints, &definition, &second, NULL));
    }
    CHECK(parameter_definition_check(file, second_type ? memory->joints + second : NULL,
                                     sample));
    for (uint32_t pass = 0; pass < 2; ++pass) {
        CHECK(parameter_update_case(file, sample, pass, &joints, first, second));
    }
    return parameter_metadata(file, &joints, sample);
}

static bool parameters_native_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_GAME_JOINT_PARAMETERS_V1 4096\n") == 0);
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        WorldStorage memory;
        AbPhysicsWorld world = {0};
        bool okay = parameter_case(file, sample, &memory, &world);
        ab_world_close(&world);
        if (!okay) {
            return false;
        }
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB game joint parameters reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("Compared 8,192 original parameter updates: typed/ignored keys, repeated "
         "assignments, signed-zero bounds, duplicate names, locked worlds, saved "
         "metadata and retained impulses");
    return true;
}

static bool parameter_edge_case(AbPhysicsWorld *world, WorldStorage *memory) {
    AbWorldStorage storage = prepare_storage(memory, 2, 0);
    AbBodyDefinition body = {.type = AB_BODY_DYNAMIC, .active = true};
    for (size_t index = 0; index < 2; ++index) {
        CHECK(ab_body_initialize(&body, &memory->bodies[index].motion));
    }
    CHECK(ab_world_open(world, default_definition(), storage, NULL));
    AbClassicJointRecord records[2];
    AbClassicJoints joints;
    CHECK(ab_classic_joints_open(&joints, world,
                                 (AbClassicJointStorage){records, 2, NULL, 0}, NULL));
    AbClassicJoint definition = {
        .type = AB_CLASSIC_JOINT_PRISMATIC,
        .name = key("same").string,
        .joint = {.first_body = 0,
                  .second_body = 1,
                  .constraint = {.kind = AB_JOINT_PRISMATIC,
                                 .definition.prismatic = {.upper_translation = 1}}}};
    size_t first, second;
    CHECK(ab_classic_joints_create(&joints, &definition, &first, NULL));
    definition.type = AB_CLASSIC_JOINT_WELD;
    definition.joint.constraint = (AbJointConstraint){.kind = AB_JOINT_WELD};
    CHECK(ab_classic_joints_create(&joints, &definition, &second, NULL));
    AbDataEntry entries[3];
    AbDataTable table = {.entries = entries, .capacity = 3};
    add(&table, "lowerLimit",
        (AbDataValue){.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x80000000)});
    add(&table, "frequency", (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(NAN)});
    AbJointConstraint *joint = &memory->joints[first].constraint;
    joint->state.prismatic.impulse.z = 3;
    world->locked = true;
    CHECK(ab_classic_joints_set_parameters(&joints, definition.name, &table, NULL) &&
          !memory->bodies[0].motion.awake &&
          bits(joint->definition.prismatic.lower_translation) == 0 &&
          joint->state.prismatic.impulse.z == 3 &&
          ab_classic_joint_parameter(records + 1, AB_JOINT_SET_LOWER).bits ==
              UINT32_C(0x80000000) &&
          ab_classic_joint_parameter(records, AB_JOINT_SET_LOWER).bits == 0 &&
          ab_classic_joint_parameter(NULL, AB_JOINT_SET_SPEED).kind == AB_DATA_NIL);
    AbJointConstraint before = *joint;
    AbJointParameters invalid = {.present = AB_JOINT_SET_SPEED, .speed = NAN};
    CHECK(!ab_world_set_joint_parameters(world, first, &invalid, NULL) &&
          memcmp(&before, joint, sizeof(before)) == 0);
    add(&table, "motor", (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1});
    CHECK(ab_classic_joints_set_parameters(&joints, definition.name, &table, NULL) &&
          joint->definition.prismatic.enable_motor && memory->bodies[0].motion.awake &&
          joint->state.prismatic.impulse.z == 3 &&
          ab_classic_joint_parameter(records + 1, AB_JOINT_SET_MOTOR).bits == 1 &&
          memory->joints[second].constraint.definition.weld.frequency == 0);
    world->locked = false;
    return true;
}

static bool parameter_edges(void) {
    WorldStorage memory;
    AbPhysicsWorld world = {0};
    bool okay = parameter_edge_case(&world, &memory);
    ab_world_close(&world);
    return okay;
}

#endif
