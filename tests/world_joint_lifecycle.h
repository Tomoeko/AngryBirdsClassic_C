#ifndef AB_TEST_WORLD_JOINT_LIFECYCLE_H
#define AB_TEST_WORLD_JOINT_LIFECYCLE_H

static void capture_joint_destruction(void *context, AbPhysicsWorld *world,
                                      size_t index) {
    WorldEvents *events = context;
    if (events->count >= 128 || !ab_world_has_joint(world, index)) {
        events->invalid = true;
        return;
    }
    const AbWorldSolveJoint *joint = world->storage.joints + index;
    uint32_t *output = events->records[events->count++];
    memset(output, 0, 36 * sizeof(uint32_t));
    output[0] = 6;
    output[1] = (uint32_t)joint->user_data;
    output[2] = (uint32_t)world->storage.bodies[joint->first_body].motion.user_data;
    output[3] = (uint32_t)world->storage.bodies[joint->second_body].motion.user_data;
    output[4] = (uint32_t)world->joint_count;
}

static bool joint_lifecycle_edges(AbPhysicsWorld *world) {
    WorldStorage memory;
    AbWorldStorage storage = prepare_storage(&memory, 2, 2);
    AbBodyDefinition body = {
        .type = AB_BODY_DYNAMIC, .active = true, .awake = true, .gravity_scale = 1};
    for (size_t index = 0; index < 2; ++index) {
        body.user_data = index + 1;
        CHECK(ab_body_initialize(&body, &memory.bodies[index].motion));
        memory.fixtures[index] = (AbWorldFixture){
            .body = index,
            .shape = {.kind = AB_SHAPE_CIRCLE, .data.circle = {1, {0, 0}}},
            .density = 1,
            .filter = {1, 0xffff, 0},
            .user_data = index + 1};
    }
    storage.body_capacity = 5;
    storage.fixture_capacity = 6;
    AbError error = {0};
    CHECK(ab_world_open(world, default_definition(), storage, &error) &&
          ab_world_step(world, 0, 0, 0, &error) && world->contact_count == 1);
    AbWorldSolveJoint joint = {.first_body = 0,
                               .second_body = 1,
                               .constraint.kind = AB_JOINT_REVOLUTE,
                               .island = true,
                               .user_data = 17};
    joint.constraint.state.revolute.impulse.x = 99;
    joint.constraint.state.revolute.limit = AB_JOINT_LIMIT_UPPER;
    memory.bodies[0].motion.awake = false;
    memory.bodies[0].motion.sleep_time = 0.25f;
    memory.bodies[1].motion.sleep_time = 0.125f;
    size_t slot = SIZE_MAX, moves = world->broadphase.move_count;
    CHECK(!ab_world_create_joint(NULL, &joint, &slot, &error) &&
          !ab_world_create_joint(world, NULL, &slot, &error) &&
          !ab_world_create_joint(world, &joint, NULL, &error) && slot == SIZE_MAX);
    world->locked = true;
    CHECK(!ab_world_create_joint(world, &joint, &slot, &error));
    world->locked = false;
    CHECK(ab_world_create_joint(world, &joint, &slot, &error) && slot == 0 &&
          ab_world_has_joint(world, slot) && !memory.bodies[0].motion.awake &&
          memory.bodies[0].motion.sleep_time == 0.25f &&
          memory.bodies[1].motion.sleep_time == 0.125f && !memory.joints[slot].island &&
          memory.joints[slot].constraint.state.revolute.impulse.x == 0 &&
          memory.joints[slot].constraint.state.revolute.limit ==
              AB_JOINT_LIMIT_INACTIVE &&
          (memory.contacts[0].state.flags & AB_CONTACT_FILTER) &&
          world->broadphase.move_count == moves);
    world->locked = true;
    CHECK(!ab_world_destroy_joint(world, slot, &error) && world->joint_count == 1);
    world->locked = false;
    CHECK(ab_world_destroy_joint(world, slot, &error) &&
          !ab_world_has_joint(world, slot) && world->joint_count == 0 &&
          memory.bodies[0].motion.awake && memory.bodies[0].motion.sleep_time == 0 &&
          memory.bodies[1].motion.sleep_time == 0.125f &&
          world->broadphase.move_count == moves &&
          !ab_world_destroy_joint(world, slot, &error));
    CHECK(ab_world_create_joint(world, &joint, &slot, &error) && slot == 0);
    joint.user_data = 18;
    joint.collide_connected = true;
    CHECK(ab_world_create_joint(world, &joint, &slot, &error) && slot == 1 &&
          world->joint_order[0] == 1 && world->joint_order[1] == 0);
    size_t filtered = 0;
    world->listener = (AbWorldListener){.context = &filtered, .filter = reject_pair};
    CHECK(ab_world_step(world, 0, 0, 0, &error) && world->contact_count == 0 &&
          filtered == 0);
    CHECK(ab_world_destroy_joint(world, 0, &error) &&
          ab_world_step(world, 0, 0, 0, &error) && world->contact_count == 0);
    /* No proxy touch on removal: a previously suppressed pair stays absent. */
    CHECK(ab_world_set_fixture_filter(world, 0, memory.fixtures[0].filter, &error) &&
          ab_world_step(world, 1.0f / 60.0f, 0, 0, &error) && filtered == 1);
    WorldEvents events = {0};
    world->listener = (AbWorldListener){.context = &events,
                                        .destroy_joint = capture_joint_destruction,
                                        .destroy_fixture = capture_fixture_destruction};
    joint.user_data = 19;
    CHECK(ab_world_create_joint(world, &joint, &slot, &error) && slot == 0 &&
          ab_world_destroy_body(world, 0, &error) && !events.invalid &&
          events.count == 3 && events.records[0][0] == 6 &&
          events.records[0][1] == 19 && events.records[0][4] == 2 &&
          events.records[1][0] == 6 && events.records[1][1] == 18 &&
          events.records[1][4] == 1 && events.records[2][0] == 5 &&
          world->joint_count == 0);
    CHECK(!ab_world_create_joint(world, &joint, &slot, &error));
    ab_world_close(world);
    CHECK(!ab_world_has_joint(world, 0));
    storage = prepare_storage(&memory, 2, 0);
    for (size_t index = 0; index < 2; ++index) {
        CHECK(ab_body_initialize(&body, &memory.bodies[index].motion));
    }
    storage.joint_count = 1;
    memory.joints[0] = joint;
    CHECK(ab_world_open(world, default_definition(), storage, &error) &&
          world->joint_order[0] == 0 && !memory.joints[0].island &&
          memory.joints[0].constraint.state.revolute.impulse.x == 0);
    ab_world_close(world);
    memory.joints[0].constraint.definition.revolute.maximum_motor_torque = NAN;
    CHECK(!ab_world_open(world, default_definition(), storage, &error));
    ab_world_close(world);
    memory.joints[0] = joint;
    memory.solve.joint_capacity = 0;
    CHECK(!ab_world_open(world, default_definition(), storage, &error));
    ab_world_close(world);
    return true;
}

#endif
