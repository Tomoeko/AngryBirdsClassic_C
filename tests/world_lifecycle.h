#ifndef AB_TEST_WORLD_LIFECYCLE_H
#define AB_TEST_WORLD_LIFECYCLE_H

static size_t body_with_id(const AbPhysicsWorld *world, uint32_t identifier) {
    for (size_t cursor = 0; cursor < world->body_count; ++cursor) {
        size_t slot = world->body_order[cursor];
        if (world->storage.bodies[slot].motion.user_data == identifier) {
            return slot;
        }
    }
    return SIZE_MAX;
}

static size_t fixture_with_id(const AbPhysicsWorld *world, uint32_t identifier) {
    for (size_t cursor = 0; cursor < world->fixture_count; ++cursor) {
        size_t slot = world->fixture_order[cursor];
        if (world->storage.fixtures[slot].user_data == identifier) {
            return slot;
        }
    }
    return SIZE_MAX;
}

static void capture_fixture_destruction(void *context, AbPhysicsWorld *world,
                                        size_t fixture_index) {
    WorldEvents *events = context;
    if (events->count >= 128 || !ab_world_has_fixture(world, fixture_index)) {
        events->invalid = true;
        return;
    }
    const AbWorldFixture *fixture = world->storage.fixtures + fixture_index;
    uint32_t *output = events->records[events->count++];
    memset(output, 0, 36 * sizeof(uint32_t));
    output[0] = 5;
    output[1] = (uint32_t)fixture->user_data;
    output[2] = (uint32_t)world->storage.bodies[fixture->body].motion.user_data;
    output[3] = fixture->proxy.active;
}

static bool lifecycle_edge_checks(AbPhysicsWorld *world) {
    WorldStorage memory;
    AbWorldStorage storage = prepare_storage(&memory, 0, 0);
    storage.body_capacity = 5;
    storage.fixture_capacity = 6;
    AbError error = {0};
    CHECK(ab_world_open(world, default_definition(), storage, &error));
    CHECK(world->body_count == 0 && world->fixture_count == 0);
    CHECK(!ab_world_has_body(world, 0) && !ab_world_has_fixture(world, 0));
    CHECK(ab_world_step(world, 0.1f, 10, 10, &error));
    AbBodyDefinition definition = {
        .type = AB_BODY_DYNAMIC, .awake = true, .active = true, .gravity_scale = 1};
    size_t body = SIZE_MAX;
    CHECK(!ab_world_create_body(NULL, &definition, &body, &error));
    CHECK(!ab_world_create_body(world, NULL, &body, &error));
    CHECK(!ab_world_create_body(world, &definition, NULL, &error));
    CHECK(body == SIZE_MAX && !world->failed);
    for (size_t identifier = 0; identifier < 5; ++identifier) {
        definition.user_data = identifier + 1;
        CHECK(ab_world_create_body(world, &definition, &body, &error));
        CHECK(body == identifier && world->body_order[0] == identifier);
    }
    CHECK(!ab_world_create_body(world, &definition, &body, &error));
    CHECK(world->body_count == 5 && !world->failed);
    CHECK(!ab_world_destroy_body(world, 5, &error));
    AbWorldFixture fixture = {
        .body = 2,
        .shape = {.kind = AB_SHAPE_CIRCLE, .data.circle = {1, {0, 0}}},
        .density = 1,
        .filter = {1, 0xffff, 0},
    };
    size_t created = SIZE_MAX;
    CHECK(!ab_world_create_fixture(world, NULL, &created, &error));
    CHECK(!ab_world_create_fixture(world, &fixture, NULL, &error));
    fixture.friction = NAN;
    CHECK(!ab_world_create_fixture(world, &fixture, &created, &error));
    CHECK(created == SIZE_MAX && !world->failed);
    fixture.friction = 0.3f;
    for (size_t identifier = 0; identifier < 6; ++identifier) {
        fixture.user_data = identifier + 1;
        CHECK(ab_world_create_fixture(world, &fixture, &created, &error));
        CHECK(created == identifier);
    }
    CHECK(!ab_world_create_fixture(world, &fixture, &created, &error));
    CHECK(!world->failed && world->fixture_count == 6);
    world->locked = true;
    CHECK(!ab_world_destroy_fixture(world, 0, &error));
    CHECK(!ab_world_destroy_body(world, 2, &error));
    CHECK(!ab_world_create_fixture(world, &fixture, &created, &error));
    CHECK(!ab_world_create_body(world, &definition, &body, &error));
    world->locked = false;
    CHECK(ab_world_destroy_fixture(world, 1, &error));
    CHECK(!ab_world_has_fixture(world, 1));
    CHECK(!ab_world_destroy_fixture(world, 1, &error));
    CHECK(!ab_world_set_fixture_sensor(world, 1, true));
    CHECK(!ab_world_set_fixture_filter(world, 1, (AbCollisionFilter){0}, &error));
    CHECK(ab_world_create_fixture(world, &fixture, &created, &error));
    CHECK(created == 1 && world->fixture_order[world->fixture_count - 1] == 1);
    WorldEvents events = {0};
    world->listener = (AbWorldListener){.context = &events,
                                        .destroy_fixture = capture_fixture_destruction};
    CHECK(ab_world_destroy_body(world, 2, &error));
    CHECK(world->body_count == 4 && world->fixture_count == 0);
    CHECK(world->broadphase.proxy_count == 0 && events.count == 6 && !events.invalid);
    CHECK(events.records[0][1] == 6 && events.records[1][1] == 6);
    CHECK(!ab_world_has_body(world, 2));
    CHECK(!ab_world_apply_force(world, 2, (AbVec2){0, 0}, 0));
    CHECK(!ab_world_set_active(world, 2, true, &error));
    CHECK(!ab_world_set_transform(world, 2, (AbVec2){0, 0}, 0, &error));
    CHECK(!ab_world_create_fixture(world, &fixture, &created, &error));
    CHECK(ab_world_create_body(world, &definition, &body, &error));
    CHECK(body == 2 && world->body_order[0] == 2);
    CHECK(ab_world_step(world, 0.1f, 10, 10, &error));
    ab_world_close(world);
    CHECK(!ab_world_has_body(world, 0) && !ab_world_has_fixture(world, 0));
    storage.body_capacity = 6;
    CHECK(!ab_world_open(world, default_definition(), storage, &error));
    ab_world_close(world);
    return true;
}

static bool read_lifecycle_fixture(FILE *file, AbPhysicsWorld *world) {
    uint32_t data[13];
    CHECK(read_words(file, data, 13));
    uint16_t group_bits = (uint16_t)data[11];
    int16_t group;
    memcpy(&group, &group_bits, sizeof(group));
    AbWorldFixture fixture = {
        .body = body_with_id(world, data[0]),
        .density = from_bits(data[7]),
        .friction = from_bits(data[5]),
        .restitution = from_bits(data[6]),
        .sensor = data[8] != 0,
        .filter = {(uint16_t)data[9], (uint16_t)data[10], group},
        .user_data = data[12],
    };
    fixture.shape.kind = (AbShapeKind)data[1];
    if (data[1] == AB_SHAPE_CIRCLE) {
        fixture.shape.data.circle =
            (AbCircle){from_bits(data[2]), {from_bits(data[3]), from_bits(data[4])}};
    } else {
        CHECK(data[1] == AB_SHAPE_POLYGON);
        fixture.shape.data.polygon =
            ab_polygon_box(from_bits(data[3]), from_bits(data[4]), from_bits(data[2]));
    }
    size_t slot;
    AbError error = {0};
    CHECK(ab_world_create_fixture(world, &fixture, &slot, &error));
    return true;
}

static bool read_lifecycle_operation(FILE *file, AbPhysicsWorld *world,
                                     size_t counts[7]) {
    uint32_t operation[3];
    CHECK(read_words(file, operation, 3));
    CHECK(operation[0] <= 6);
    ++counts[operation[0]];
    AbError error = {0};
    size_t body = body_with_id(world, operation[1]);
    switch (operation[0]) {
        case 0:
            return true;
        case 1: {
            uint32_t words[13];
            CHECK(read_words(file, words, 13));
            AbBodyDefinition definition = read_definition(words);
            CHECK(definition.user_data == operation[1]);
            return ab_world_create_body(world, &definition, &body, &error);
        }
        case 2:
            return read_lifecycle_fixture(file, world);
        case 3:
            return ab_world_destroy_body(world, body, &error);
        case 4:
            return ab_world_destroy_fixture(world, fixture_with_id(world, operation[1]),
                                            &error);
        case 5:
            return ab_world_set_transform(world, body, (AbVec2){0, 0.5f}, 0.2f, &error);
        case 6:
            return ab_world_set_active(world, body, true, &error);
        default:
            return false;
    }
}

static bool native_lifecycle_checks(FILE *file, AbPhysicsWorld *world) {
    size_t event_counts[7] = {0}, operation_counts[7] = {0};
    for (size_t scene = 0; scene < 16; ++scene) {
        uint32_t controls[7];
        CHECK(read_words(file, controls, 7) && controls[0] == 5 && controls[1] == 6);
        WorldStorage memory;
        AbWorldStorage storage = prepare_storage(&memory, 0, 0);
        storage.body_capacity = controls[0];
        storage.fixture_capacity = controls[1];
        AbWorldDefinition definition = default_definition();
        definition.gravity = (AbVec2){from_bits(controls[5]), from_bits(controls[6])};
        definition.allow_sleep = controls[2] != 0;
        definition.warm_start = controls[3] != 0;
        definition.clear_forces = controls[4] != 0;
        definition.continuous = true;
        AbError error = {0};
        CHECK(ab_world_open(world, definition, storage, &error));
        WorldEvents events = {0};
        world->listener =
            (AbWorldListener){.context = &events,
                              .begin = capture_begin,
                              .end = capture_end,
                              .pre_solve = capture_pre_solve,
                              .post_solve = capture_post_solve,
                              .destroy_fixture = capture_fixture_destruction};
        CHECK(compare_snapshot(file, world, scene, SIZE_MAX, true, true));
        CHECK(compare_events(file, &events, scene, SIZE_MAX, event_counts));
        for (size_t frame = 0; frame < 256; ++frame) {
            events.count = 0;
            CHECK(read_lifecycle_operation(file, world, operation_counts));
            CHECK(compare_snapshot(file, world, scene, frame, true, true));
            CHECK(compare_events(file, &events, scene, frame, event_counts));
            uint32_t step[3];
            CHECK(read_words(file, step, 3));
            events.count = 0;
            CHECK(ab_world_step(world, from_bits(step[0]), step[1], step[2], &error));
            CHECK(compare_snapshot(file, world, scene, frame, true, true));
            CHECK(compare_events(file, &events, scene, frame, event_counts));
        }
        ab_world_close(world);
    }
    char completion[80];
    CHECK(fgets(completion, sizeof(completion), file));
    CHECK(strcmp(completion, "AB world lifecycle reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 lifecycle operations and steps match: %zu body creations, "
           "%zu fixture creations, %zu body removals, %zu fixture removals.\n",
           operation_counts[1], operation_counts[2], operation_counts[3],
           operation_counts[4]);
    printf("Lifecycle events match: %zu begin, %zu end, %zu pre-solve, "
           "%zu post-solve, %zu implicit fixture destructions.\n",
           event_counts[1], event_counts[2], event_counts[3], event_counts[4],
           event_counts[5]);
    return true;
}

#endif
