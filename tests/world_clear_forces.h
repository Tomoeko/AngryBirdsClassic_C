#ifndef AB_TEST_WORLD_CLEAR_FORCES_H
#define AB_TEST_WORLD_CLEAR_FORCES_H

static bool clear_force_edge_checks(AbPhysicsWorld *world) {
    WorldStorage memory;
    AbWorldStorage storage = prepare_storage(&memory, 2, 0);
    AbBodyDefinition definition = {.type = AB_BODY_DYNAMIC};
    CHECK(ab_body_initialize(&definition, &memory.bodies[0].motion));
    CHECK(ab_body_initialize(&definition, &memory.bodies[1].motion));
    AbError error = {0};
    CHECK(!ab_world_clear_forces(NULL, &error));
    CHECK(!ab_world_clear_forces(world, &error));
    CHECK(ab_world_open(world, default_definition(), storage, &error));
    CHECK(ab_world_destroy_body(world, 1, &error));
    memory.bodies[0].motion.force = (AbVec2){INFINITY, NAN};
    memory.bodies[0].motion.torque = -0.0f;
    memory.bodies[1].motion.force = (AbVec2){3, 4};
    memory.bodies[1].motion.torque = 5;
    AbWorldSolveBody expected[2];
    memcpy(expected, memory.bodies, sizeof(expected));
    expected[0].motion.force = (AbVec2){0, 0};
    expected[0].motion.torque = 0;
    world->locked = true;
    CHECK(ab_world_clear_forces(world, &error));
    CHECK(memcmp(expected, memory.bodies, sizeof(expected)) == 0);
    CHECK(world->locked);
    world->locked = false;
    memory.bodies[0].motion.force.x = 7;
    size_t saved = world->body_order[0];
    world->body_order[0] = 1;
    CHECK(!ab_world_clear_forces(world, &error));
    CHECK(memory.bodies[0].motion.force.x == 7);
    world->body_order[0] = saved;
    world->failed = true;
    CHECK(!ab_world_clear_forces(world, &error));
    CHECK(memory.bodies[0].motion.force.x == 7);
    world->failed = false;
    CHECK(ab_world_destroy_body(world, 0, &error));
    CHECK(ab_world_clear_forces(world, &error));
    ab_world_close(world);
    return true;
}

static bool clear_force_native_checks(FILE *file, AbPhysicsWorld *world) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB_ANDROID_WORLD_CLEAR_V1 4096\n") == 0);
    size_t live_total = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[17], output[12];
        CHECK(read_words(file, input, 17) && input[0] < 16);
        unsigned seen = 0;
        for (size_t index = 1; index < 5; ++index) {
            CHECK(input[index] < 4 && !(seen & (1u << input[index])));
            seen |= 1u << input[index];
        }
        WorldStorage memory;
        AbWorldStorage storage = prepare_storage(&memory, 4, 0);
        for (size_t body = 0; body < 4; ++body) {
            AbBodyDefinition definition = {.type = (AbBodyType)(body % 3),
                                           .awake = (item & (1u << body)) != 0,
                                           .active = (item & (2u << body)) != 0};
            CHECK(ab_body_initialize(&definition, &memory.bodies[body].motion));
        }
        AbError error = {0};
        CHECK(ab_world_open(world, default_definition(), storage, &error));
        for (size_t body = 0; body < 4; ++body) {
            if (!(input[0] & (1u << body))) {
                CHECK(ab_world_destroy_body(world, body, &error));
            }
            AbBodyMotion *motion = &memory.bodies[body].motion;
            memcpy(&motion->force.x, input + 5 + body * 3, sizeof(float));
            memcpy(&motion->force.y, input + 6 + body * 3, sizeof(float));
            memcpy(&motion->torque, input + 7 + body * 3, sizeof(float));
        }
        size_t count = 0;
        for (size_t index = 1; index < 5; ++index) {
            if (input[0] & (1u << input[index])) {
                world->body_order[count++] = input[index];
            }
        }
        CHECK(count == world->body_count);
        live_total += count;
        AbWorldSolveBody expected[4];
        memcpy(expected, memory.bodies, sizeof(expected));
        for (size_t body = 0; body < 4; ++body) {
            if (input[0] & (1u << body)) {
                expected[body].motion.force = (AbVec2){0, 0};
                expected[body].motion.torque = 0;
            }
        }
        world->locked = (item & 1) != 0;
        CHECK(ab_world_clear_forces(world, &error));
        CHECK(memcmp(expected, memory.bodies, sizeof(expected)) == 0);
        CHECK(read_words(file, output, 12));
        for (size_t body = 0; body < 4; ++body) {
            const AbBodyMotion *motion = &memory.bodies[body].motion;
            CHECK(to_bits(motion->force.x) == output[body * 3]);
            CHECK(to_bits(motion->force.y) == output[body * 3 + 1]);
            CHECK(to_bits(motion->torque) == output[body * 3 + 2]);
        }
        world->locked = false;
        ab_world_close(world);
    }
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB world clear reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    CHECK(live_total == 8192);
    puts("All 4096 force-clear cases match, including 8192 live bodies.");
    return true;
}

#endif
