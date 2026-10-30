#ifndef AB_TEST_GAME_JOINT_LIMITS_H
#define AB_TEST_GAME_JOINT_LIMITS_H

#include "game_joint_limit_cases.h"

static bool limit_coordinate_edges(void) {
    AbJointConstraint joint = {.kind = AB_JOINT_WELD};
    AbJointBodyFrame frames[] = {{{{NAN, NAN}, NAN, NAN}, 2},
                                 {{{NAN, NAN}, NAN, NAN}, 4}};
    float value = 7.25f;
    CHECK(!ab_joint_constraint_coordinate(&joint, frames, &value, NULL) &&
          value == 7.25f);
    joint.kind = AB_JOINT_REVOLUTE;
    joint.definition.revolute.reference_angle = 1;
    CHECK(ab_joint_constraint_coordinate(&joint, frames, &value, NULL) && value == 1);
    frames[0].angle = NAN;
    CHECK(!ab_joint_constraint_coordinate(&joint, frames, &value, NULL) && value == 1);
    joint = (AbJointConstraint){
        .kind = AB_JOINT_PRISMATIC,
        .definition.prismatic.local = {.axis = {1, 0}, .perpendicular = {NAN, NAN}},
        .definition.prismatic.reference_angle = NAN};
    frames[0] = (AbJointBodyFrame){{.cosine = 1}, NAN};
    frames[1] = (AbJointBodyFrame){{.position = {2, 3}, .cosine = 1}, NAN};
    CHECK(ab_joint_constraint_coordinate(&joint, frames, &value, NULL) && value == 2);
    joint.definition.prismatic.local.axis.x = NAN;
    CHECK(!ab_joint_constraint_coordinate(&joint, frames, &value, NULL) && value == 2);
    return true;
}

static bool limit_edge_case(AbPhysicsWorld *world, WorldStorage *memory) {
    AbWorldStorage storage = prepare_storage(memory, 2, 0);
    AbBodyDefinition body = {.type = AB_BODY_DYNAMIC, .active = true};
    for (size_t index = 0; index < 2; ++index) {
        CHECK(ab_body_initialize(&body, &memory->bodies[index].motion));
        memory->bodies[index].motion.sleep_time = 0.25f;
    }
    CHECK(ab_world_open(world, default_definition(), storage, NULL));
    AbClassicJointRecord records[2];
    AbClassicJoints joints;
    CHECK(ab_classic_joints_open(&joints, world,
                                 (AbClassicJointStorage){records, 2, NULL, 0}, NULL));
    AbClassicJoint definition = {
        .type = AB_CLASSIC_JOINT_REVOLUTE,
        .joint = {.first_body = 0,
                  .second_body = 1,
                  .constraint = {.kind = AB_JOINT_REVOLUTE,
                                 .definition.revolute = {.upper_angle = 1,
                                                         .motor_speed = 1,
                                                         .enable_motor = true,
                                                         .enable_limit = true}}},
        .name = key("joint").string};
    size_t first, second = 0;
    CHECK(ab_classic_joints_create(&joints, &definition, &first, NULL) &&
          ab_classic_joints_create(&joints, &definition, &second, NULL));
    CHECK(ab_classic_joints_check_limits(&joints, key("absent").string, NULL) &&
          !memory->bodies[0].motion.awake);
    CHECK(!ab_world_set_joint_motor_speed(world, SIZE_MAX, 1, NULL) &&
          !ab_world_set_joint_motor_speed(world, first, NAN, NULL) &&
          !memory->bodies[0].motion.awake);
    world->locked = true;
    CHECK(ab_world_set_joint_motor_speed(world, first, 1, NULL) &&
          memory->bodies[0].motion.awake && memory->bodies[0].motion.sleep_time == 0);
    world->locked = false;
    CHECK(ab_world_set_transform(world, 1, (AbVec2){0, 0}, 2, NULL));
    for (size_t index = 0; index < 2; ++index) {
        memory->bodies[index].motion.awake = false;
        memory->bodies[index].motion.sleep_time = 0.25f;
    }
    CHECK(ab_classic_joints_check_limits(&joints, definition.name, NULL) &&
          memory->joints[first].constraint.definition.revolute.motor_speed == -1 &&
          memory->joints[second].constraint.definition.revolute.motor_speed == -1 &&
          memory->bodies[0].motion.awake && memory->bodies[1].motion.awake &&
          !records[0].metadata_visible &&
          records[0].definition.joint.constraint.definition.revolute.motor_speed == 1 &&
          records[1].definition.joint.constraint.definition.revolute.motor_speed == 1);
    return true;
}

static bool limit_edges(void) {
    WorldStorage memory;
    AbPhysicsWorld world = {0};
    bool okay = limit_coordinate_edges() && limit_edge_case(&world, &memory);
    ab_world_close(&world);
    return okay;
}

static bool limit_bodies(FILE *file, const AbPhysicsWorld *world, uint32_t sample) {
    for (size_t index = 0; index < 2; ++index) {
        const AbWorldSolveBody *body = world->storage.bodies + index;
        const AbBodyMotion *motion = &body->motion;
        const uint32_t actual[] = {
            (body->island ? 1u : 0u) | (motion->awake ? 2u : 0u) |
                (motion->sleep_allowed ? 4u : 0u) | (motion->bullet ? 8u : 0u) |
                (motion->mass.fixed_rotation ? 16u : 0u) | (motion->active ? 32u : 0u),
            bits(motion->sleep_time)};
        uint32_t expected[2];
        CHECK(read_words(file, expected, 2) &&
              compare(actual, expected, 2, sample, "limit body"));
    }
    return true;
}

static bool limit_frames(FILE *file, AbPhysicsWorld *world, const uint32_t input[28]) {
    for (size_t index = 0; index < 2; ++index) {
        float x = number(input[4 + index * 3]), y = number(input[5 + index * 3]);
        float angle = number(input[6 + index * 3]);
        angle += number(input[24 + index]);
        if (index) {
            x += number(input[26]);
            y += number(input[27]);
        }
        CHECK(ab_world_set_transform(world, index, (AbVec2){x, y}, angle, NULL));
        AbBodyMotion *body = &world->storage.bodies[index].motion;
        const uint32_t actual[] = {
            bits(body->mass.transform.position.x),
            bits(body->mass.transform.position.y), bits(body->mass.transform.sine),
            bits(body->mass.transform.cosine), bits(body->mass.angle)};
        uint32_t expected[5];
        CHECK(read_words(file, expected, 5) &&
              compare(actual, expected, 5, input[0], "limit frame"));
        body->awake = ((input[0] >> (4 + index)) & 1u) != 0;
        body->sleep_time = index ? 0.125f : 0.25f;
    }
    return true;
}

static void limit_seed_state(AbJointConstraint *joint, const uint32_t input[28]) {
    AbVec3 impulse = {number(input[14]), number(input[15]), number(input[22])};
    if (joint->kind == AB_JOINT_REVOLUTE) {
        joint->state.revolute.impulse = impulse;
        joint->state.revolute.motor_impulse = number(input[16]);
        joint->state.revolute.limit = (AbJointLimitState)(input[0] % 4);
    } else {
        joint->state.prismatic.impulse = impulse;
        joint->state.prismatic.motor_impulse = number(input[16]);
        joint->state.prismatic.limit = (AbJointLimitState)(input[0] % 4);
    }
}

static bool limit_case(FILE *file, uint32_t sample, WorldStorage *memory,
                       AbPhysicsWorld *world, size_t *reversals) {
    uint32_t input[28], generated[28];
    CHECK(read_words(file, input, 28));
    game_joint_limit_case(sample, generated);
    CHECK(memcmp(input, generated, sizeof(input)) == 0);
    AbDataEntry entries[24];
    AbDataTable table = {.entries = entries, .capacity = 24};
    prepare_table(input, &table);
    AbClassicJointSource source = {.table = &table, .body_indices = {0, 1}};
    AbBodyMotion bodies[2];
    CHECK(read_frames(file, input, &source, bodies, sample));
    AbWorldStorage storage = prepare_storage(memory, 2, 0);
    for (size_t index = 0; index < 2; ++index) {
        memory->bodies[index].motion = bodies[index];
    }
    CHECK(ab_world_open(world, default_definition(), storage, NULL));
    AbClassicJointRecord record[1];
    AbClassicJoints joints;
    CHECK(ab_classic_joints_open(&joints, world,
                                 (AbClassicJointStorage){record, 1, NULL, 0}, NULL));
    AbClassicJoint definition;
    size_t slot = 0;
    CHECK(ab_classic_joint_read(&source, &definition, NULL) &&
          ab_classic_joints_create(&joints, &definition, &slot, NULL));
    AbWorldSolveJoint *joint = world->storage.joints + slot;
    uint32_t expected[20], initial[20], actual[20];
    project_definition(&(AbClassicJoint){.joint = *joint}, initial);
    CHECK(read_words(file, expected, 20) &&
          compare(initial, expected, 20, sample, "limit initial definition") &&
          limit_frames(file, world, input));
    limit_seed_state(&joint->constraint, input);
    uint8_t state[sizeof(joint->constraint.state)];
    memcpy(state, &joint->constraint.state, sizeof(state));
    AbJointBodyFrame frames[2];
    for (size_t index = 0; index < 2; ++index) {
        const AbBodyMassState *body = &world->storage.bodies[index].motion.mass;
        frames[index] = (AbJointBodyFrame){body->transform, body->angle};
    }
    float coordinate;
    CHECK(
        ab_joint_constraint_coordinate(&joint->constraint, frames, &coordinate, NULL));
    uint32_t measured = bits(coordinate), expected_coordinate;
    CHECK(read_words(file, &expected_coordinate, 1) &&
          compare(&measured, &expected_coordinate, 1, sample, "joint coordinate") &&
          limit_bodies(file, world, sample));
    world->locked = (sample & 1024u) != 0;
    if (sample & 512u) {
        CHECK(ab_world_set_joint_motor_speed(world, slot, number(initial[11]), NULL));
    }
    CHECK(limit_bodies(file, world, sample) &&
          ab_classic_joints_check_limits(&joints, key("joint").string, NULL));
    project_definition(&(AbClassicJoint){.joint = *joint}, actual);
    CHECK(read_words(file, expected, 20) &&
          compare(actual, expected, 20, sample, "limit checked definition") &&
          limit_bodies(file, world, sample) &&
          memcmp(state, &joint->constraint.state, sizeof(state)) == 0);
    *reversals += initial[11] != actual[11];
    world->locked = false;
    uint32_t metadata[42], expected_metadata[40], saved[20];
    project_definition(&record[0].definition, saved);
    project_metadata(&record[0].definition, saved, metadata);
    for (size_t field = 0; field < 20; ++field) {
        CHECK(read_words(file, expected_metadata + field * 2, 2));
    }
    CHECK(compare(metadata, expected_metadata, 40, sample, "unchanged motor metadata"));
    return true;
}

static bool limits_native_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_GAME_JOINT_LIMITS_V1 4096\n") == 0);
    size_t reversals = 0;
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        WorldStorage memory;
        AbPhysicsWorld world = {0};
        bool okay = limit_case(file, sample, &memory, &world, &reversals);
        ab_world_close(&world);
        if (!okay) {
            return false;
        }
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB game joint limits reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Compared 4,096 original motor limit checks: %zu reversals, 2,048 "
           "unchanged-speed assignments, "
           "2,048 locked calls, retained solver state and unchanged saved metadata\n",
           reversals);
    return true;
}

#endif
