#ifndef AB_TEST_GAME_GLUE_POLICY_H
#define AB_TEST_GAME_GLUE_POLICY_H

#include "ab/classic_joints.h"
#include "world_storage.h"

static bool registry_edge_case(AbPhysicsWorld *world, WorldStorage *memory) {
    AbWorldStorage storage = prepare_storage(memory, 2, 0);
    AbBodyDefinition body = {.type = AB_BODY_DYNAMIC, .active = true};
    for (size_t index = 0; index < 2; ++index) {
        CHECK(ab_body_initialize(&body, &memory->bodies[index].motion));
        memory->bodies[index].motion.sleep_time = 0.25f;
    }
    CHECK(ab_world_open(world, default_definition(), storage, NULL));
    AbClassicJointRecord immediate[2], deferred[1];
    AbClassicJoints joints;
    AbClassicJointStorage records = {immediate, 2, deferred, 1};
    CHECK(!ab_classic_joints_open(NULL, world, records, NULL) &&
          !ab_classic_joints_open(&joints, NULL, records, NULL) &&
          ab_classic_joints_open(&joints, world, records, NULL));
    AbClassicJoint definition = {
        .type = AB_CLASSIC_JOINT_WELD,
        .joint = {.first_body = 0, .second_body = 1, .constraint.kind = AB_JOINT_WELD},
        .name = key("same").string};
    size_t first = 0, second, output = 77;
    CHECK(!ab_classic_joints_create(&joints, NULL, &output, NULL) && output == 77 &&
          !ab_classic_joints_create(&joints, &definition, NULL, NULL) &&
          ab_classic_joints_create(&joints, &definition, &first, NULL));
    definition.type = AB_CLASSIC_JOINT_GLUE;
    definition.destroy_timer = 1;
    CHECK(ab_classic_joints_create(&joints, &definition, &output, NULL) &&
          output == SIZE_MAX && !immediate[0].metadata_visible &&
          ab_classic_joints_metadata(&joints, definition.name) == deferred);
    definition.type = AB_CLASSIC_JOINT_WELD;
    CHECK(ab_classic_joints_create(&joints, &definition, &second, NULL) &&
          !deferred[0].metadata_visible &&
          ab_classic_joints_metadata(&joints, definition.name) == immediate + 1);
    output = 77;
    CHECK(!ab_classic_joints_create(&joints, &definition, &output, NULL) &&
          output == 77 && !joints.failed && joints.joint_count == 2);
    CHECK(ab_classic_joints_remove(&joints, first, NULL) &&
          !ab_classic_joints_metadata(&joints, definition.name) &&
          joints.joint_count == 1 && ab_world_has_joint(world, second));
    memory->bodies[0].motion.awake = false;
    memory->bodies[0].motion.sleep_time = 0.25f;
    world->locked = true;
    CHECK(ab_classic_joints_remove(&joints, second, NULL) &&
          ab_world_has_joint(world, second) && joints.joint_count == 0 &&
          !memory->bodies[0].motion.awake &&
          memory->bodies[0].motion.sleep_time == 0.25f);
    CHECK(ab_classic_joints_create(&joints, &definition, &output, NULL) &&
          output == SIZE_MAX && joints.joint_count == 1 && world->joint_count == 1);
    world->locked = false;
    CHECK(!ab_classic_joints_remove(&joints, SIZE_MAX, NULL) &&
          joints.joint_count == 1);
    world->locked = true;
    CHECK(ab_classic_joints_remove(&joints, SIZE_MAX, NULL));
    world->locked = false;
    CHECK(ab_classic_joints_remove(&joints, second, NULL) && world->joint_count == 1);
    AbClassicGluePair pairs[] = {{0, 1}, {0, 9}};
    size_t count = 2;
    CHECK(!ab_classic_joints_glue(&joints, pairs, &count, NULL) && count == 2 &&
          world->joint_count == 1 && !joints.failed);
    count = 0;
    CHECK(ab_classic_joints_glue(&joints, NULL, &count, NULL) &&
          !ab_classic_joints_glue(&joints, pairs, NULL, NULL));
    ab_classic_joints_close(&joints);
    CHECK(!ab_classic_joints_metadata(&joints, definition.name));
    return true;
}

static bool registry_edges(void) {
    WorldStorage memory;
    AbPhysicsWorld world = {0};
    bool okay = registry_edge_case(&world, &memory);
    ab_world_close(&world);
    return okay;
}

static bool policy_snapshot(FILE *file, const AbClassicJoints *joints, uint32_t sample,
                            size_t *states) {
    const AbPhysicsWorld *world = joints->world;
    uint32_t expected[20];
    const uint32_t counts[] = {
        (uint32_t)world->joint_count, (uint32_t)joints->joint_count,
        (uint32_t)joints->glue_count,
        ab_classic_joints_metadata(joints, key("registered").string) ? 5u : 0u,
        world->locked ? 1u : 0u};
    CHECK(read_words(file, expected, 5) &&
          compare(counts, expected, 5, sample, "policy counts"));
    for (size_t index = 0; index < 2; ++index) {
        const AbWorldSolveBody *body = world->storage.bodies + index;
        const AbBodyMotion *motion = &body->motion;
        const uint32_t fields[] = {
            (body->island ? 1u : 0u) | (motion->awake ? 2u : 0u) |
                (motion->sleep_allowed ? 4u : 0u) | (motion->bullet ? 8u : 0u) |
                (motion->mass.fixed_rotation ? 16u : 0u) | (motion->active ? 32u : 0u),
            bits(motion->sleep_time)};
        CHECK(read_words(file, expected, 2) &&
              compare(fields, expected, 2, sample, "policy body"));
    }
    for (size_t cursor = 0; cursor < world->joint_count; ++cursor) {
        const AbWorldSolveJoint *joint =
            world->storage.joints + world->joint_order[cursor];
        const uint32_t metadata[] = {
            (uint32_t)joint->user_data, (uint32_t)joint->first_body,
            (uint32_t)joint->second_body,
            cursor ? (uint32_t)world->storage.joints[world->joint_order[cursor - 1]]
                         .user_data
                   : UINT32_MAX,
            cursor + 1 < world->joint_count
                ? (uint32_t)world->storage.joints[world->joint_order[cursor + 1]]
                      .user_data
                : UINT32_MAX};
        CHECK(read_words(file, expected, 5) &&
              compare(metadata, expected, 5, sample, "policy joint list"));
        uint32_t actual[20];
        project_definition(&(AbClassicJoint){.joint = *joint}, actual);
        CHECK(read_words(file, expected, 20) &&
              compare(actual, expected, 20, sample, "policy definition"));
        ++*states;
    }
    return true;
}

static bool policy_seed(AbClassicJoints *joints, const uint32_t input[28],
                        AbClassicJointSource source) {
    AbPhysicsWorld *world = joints->world;
    if (input[0] & 1u) {
        uint32_t modified[28];
        memcpy(modified, input, sizeof(modified));
        modified[1] = 2;
        AbDataEntry entries[24];
        AbDataTable table = {.entries = entries, .capacity = 24};
        prepare_table(modified, &table);
        entries[0].value = key("registered");
        source.table = &table;
        AbClassicJoint definition;
        CHECK(ab_classic_joint_read(&source, &definition, NULL));
        definition.joint.user_data = 1;
        size_t slot;
        world->locked = (input[0] & 128u) != 0;
        CHECK(ab_classic_joints_create(joints, &definition, &slot, NULL));
        world->locked = false;
    }
    if (input[0] & 2u) {
        const AbWorldSolveJoint definition = {.first_body = 0,
                                              .second_body = 1,
                                              .constraint.kind = AB_JOINT_WELD,
                                              .user_data = 2};
        size_t slot;
        CHECK(ab_world_create_joint(world, &definition, &slot, NULL));
    }
    for (size_t index = 0; index < 2; ++index) {
        AbBodyMotion *body = &world->storage.bodies[index].motion;
        body->awake = ((input[0] >> (4 + index)) & 1u) != 0;
        body->sleep_time = index ? 0.125f : 0.25f;
    }
    return true;
}

static bool policy_case(FILE *file, uint32_t sample, WorldStorage *memory,
                        AbPhysicsWorld *world, size_t *states) {
    uint32_t input[28], generated[28];
    CHECK(read_words(file, input, 28));
    game_joint_case(sample, generated);
    generated[1] = 5;
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
    AbClassicJointRecord immediate[4], deferred[4];
    AbClassicJoints joints;
    CHECK(ab_classic_joints_open(
        &joints, world, (AbClassicJointStorage){immediate, 4, deferred, 4}, NULL));
    AbClassicJoint definition;
    size_t slot;
    CHECK(ab_classic_joint_read(&source, &definition, NULL) &&
          ab_classic_joints_create(&joints, &definition, &slot, NULL) &&
          slot == SIZE_MAX && policy_seed(&joints, input, source));
    world->locked = (sample & 64u) != 0;
    CHECK(policy_snapshot(file, &joints, sample, states));
    AbClassicGluePair pairs[] = {{0, 1},
                                 {sample & 8u ? 1u : 0u, sample & 8u ? 0u : 1u}};
    size_t count = 1 + ((sample >> 2) & 1u);
    CHECK(ab_classic_joints_glue(&joints, pairs, &count, NULL) && count == 0 &&
          policy_snapshot(file, &joints, sample, states));
    world->locked = false;
    uint32_t actual[20] = {0}, expected[20], metadata[42], expected_metadata[42];
    if (world->joint_count) {
        project_definition(
            &(AbClassicJoint){.joint = world->storage.joints[world->joint_order[0]]},
            actual);
    }
    CHECK(read_words(file, expected, 20) &&
          compare(actual, expected, 20, sample, "policy newest definition"));
    const AbClassicJointRecord *record =
        ab_classic_joints_metadata(&joints, key("joint").string);
    CHECK(record);
    project_metadata(&record->definition, actual, metadata);
    for (size_t field = 0; field < 21; ++field) {
        CHECK(read_words(file, expected_metadata + field * 2, 2));
    }
    CHECK(compare(metadata, expected_metadata, 42, sample, "policy deferred metadata"));
    ab_classic_joints_close(&joints);
    return true;
}

static bool policy_native_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_GAME_GLUE_POLICY_V1 4096\n") == 0);
    size_t states = 0;
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        WorldStorage memory;
        AbPhysicsWorld world = {0};
        bool okay = policy_case(file, sample, &memory, &world, &states);
        ab_world_close(&world);
        if (!okay) {
            return false;
        }
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB game glue policy reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Compared 4,096 native glue registries/queues: %zu joint-list/definition "
           "snapshots, "
           "registered/unregistered/null records, locking and ordered/reversed pairs\n",
           states);
    return true;
}

#endif
