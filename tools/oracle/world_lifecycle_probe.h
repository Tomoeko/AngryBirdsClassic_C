#ifndef AB_ORACLE_WORLD_LIFECYCLE_H
#define AB_ORACLE_WORLD_LIFECYCLE_H

typedef struct {
    void *(*create_body)(void *, const void *);
    void *(*create_fixture)(void *, const void *);
    void (*destroy_body)(void *, void *);
    void (*destroy_fixture)(void *, void *);
    void (*set_box)(void *, float, float);
    void (*set_transform)(void *, const void *, float);
    void (*set_active)(void *, bool);
    const uint32_t *circle_vtable;
    const uint32_t *polygon_vtable;
} LifecycleFunctions;

static uint32_t lifecycle_bodies[5], lifecycle_fixtures[6];

static void lifecycle_goodbye(void *listener, const void *fixture_pointer) {
    (void)listener;
    if (world_event_count >= 128) {
        fail("Native lifecycle event capacity exceeded\n");
    }
    const uint32_t *fixture = fixture_pointer;
    const uint32_t *body = (const uint32_t *)(uintptr_t)fixture[2];
    uint32_t *output = world_events[world_event_count++];
    for (size_t field = 0; field < 36; ++field) {
        output[field] = 0;
    }
    output[0] = 5;
    output[1] = fixture[10];
    output[2] = body[37];
    output[3] = fixture[7];
}

static void lifecycle_operation(uint32_t output[3], size_t frame) {
    /* Kind, logical slot, owning body (fixture creation only). */
    static const uint8_t operations[33][3] = {
        {1, 1, 0}, {2, 1, 1}, {1, 2, 0}, {2, 2, 2}, {1, 3, 0}, {2, 3, 3}, {1, 4, 0},
        {2, 4, 4}, {2, 5, 2}, {1, 5, 0}, {2, 6, 5}, {5, 4, 0}, {4, 2, 0}, {2, 2, 3},
        {3, 3, 0}, {1, 3, 0}, {2, 3, 3}, {2, 2, 3}, {3, 2, 0}, {1, 2, 0}, {2, 5, 2},
        {4, 5, 0}, {2, 5, 2}, {3, 4, 0}, {1, 4, 0}, {2, 4, 4}, {6, 4, 0}, {4, 3, 0},
        {3, 5, 0}, {3, 2, 0}, {3, 3, 0}, {3, 4, 0}, {3, 1, 0},
    };
    for (size_t field = 0; field < 3; ++field) {
        output[field] = frame % 64 < 33 ? operations[frame % 64][field] : 0;
    }
}

static void lifecycle_create_fixture(LifecycleFunctions *functions,
                                     const uint32_t specification[13]) {
    uint32_t shape[38] = {0};
    shape[0] = (uint32_t)(uintptr_t)((specification[1] ? functions->polygon_vtable
                                                       : functions->circle_vtable) +
                                     2);
    shape[1] = specification[1];
    shape[2] = specification[2];
    if (specification[1]) {
        FloatBits width = {.bits = specification[3]};
        FloatBits height = {.bits = specification[4]};
        functions->set_box(shape, width.number, height.number);
    } else {
        shape[3] = specification[3];
        shape[4] = specification[4];
    }
    uint32_t definition[] = {
        (uint32_t)(uintptr_t)shape,
        specification[12],
        specification[5],
        specification[6],
        specification[7],
        specification[8] | (specification[9] << 16),
        specification[10] | (specification[11] << 16),
    };
    uint32_t *body = (uint32_t *)(uintptr_t)lifecycle_bodies[specification[0] - 1];
    void *fixture = functions->create_fixture(body, definition);
    if (!fixture) {
        fail("Native lifecycle fixture creation failed\n");
    }
    lifecycle_fixtures[specification[12] - 1] = (uint32_t)(uintptr_t)fixture;
}

static void lifecycle_apply(LifecycleFunctions *functions, uint32_t *world,
                            const uint32_t operation[3], size_t scene, size_t frame) {
    size_t slot = operation[1] ? operation[1] - 1 : 0;
    switch (operation[0]) {
        case 0:
            break;
        case 1: {
            uint32_t definition[13];
            prepare_step_body(definition, scene, slot);
            if (slot) {
                definition[1] = world_step_number((float)(scene % 3) * 0.25f);
                definition[2] = world_step_number(0.49f + (float)(slot - 1) * 0.9f);
                definition[10] = scene % 3 == 0 && slot == 3 ? 0u : 1u;
            }
            print_words(definition, 13);
            uint32_t *created = functions->create_body(world, definition);
            if (!created) {
                fail("Native lifecycle body creation failed\n");
            }
            created[2] = 0; /* Uninitialized island index has no defined input. */
            lifecycle_bodies[slot] = (uint32_t)(uintptr_t)created;
            break;
        }
        case 2: {
            uint32_t specification[13];
            prepare_step_fixture(specification, scene, operation[2] - 1, slot,
                                 operation[1] == 5);
            if (frame % 64 == 22 && scene % 2) {
                specification[7] = 0;
            }
            print_words(specification, 13);
            lifecycle_create_fixture(functions, specification);
            break;
        }
        case 3: {
            uint32_t *body = (uint32_t *)(uintptr_t)lifecycle_bodies[slot];
            for (size_t fixture = 0; fixture < 6; ++fixture) {
                const uint32_t *current =
                    (const uint32_t *)(uintptr_t)lifecycle_fixtures[fixture];
                if (current && current[2] == lifecycle_bodies[slot]) {
                    lifecycle_fixtures[fixture] = 0;
                }
            }
            functions->destroy_body(world, body);
            lifecycle_bodies[slot] = 0;
            break;
        }
        case 4: {
            uint32_t *fixture = (uint32_t *)(uintptr_t)lifecycle_fixtures[slot];
            functions->destroy_fixture((void *)(uintptr_t)fixture[2], fixture);
            lifecycle_fixtures[slot] = 0;
            break;
        }
        case 5: {
            float position[] = {0, 0.5f};
            functions->set_transform((void *)(uintptr_t)lifecycle_bodies[slot],
                                     position, 0.2f);
            break;
        }
        case 6:
            functions->set_active((void *)(uintptr_t)lifecycle_bodies[slot], true);
            break;
        default:
            fail("Invalid native lifecycle operation\n");
    }
}

static void print_lifecycle_fixture(const uint32_t *world, const uint32_t *fixture) {
    const uint32_t *body = (const uint32_t *)(uintptr_t)fixture[2];
    uint32_t output[19] = {fixture[10],
                           body[37],
                           fixture[0],
                           fixture[4],
                           fixture[5],
                           fixture[8] & 0xffff,
                           fixture[8] >> 16,
                           fixture[9] & 0xffff,
                           (fixture[9] >> 16) & 0xff,
                           fixture[7],
                           UINT32_C(0xffffffff)};
    if (fixture[7]) {
        const uint32_t *proxy = (const uint32_t *)(uintptr_t)fixture[6];
        const uint32_t *nodes = (const uint32_t *)(uintptr_t)world[0x191dc / 4];
        const uint32_t *node = nodes + proxy[6] * 9;
        output[10] = proxy[6];
        for (size_t field = 0; field < 4; ++field) {
            output[11 + field] = proxy[field];
            output[15 + field] = node[field];
        }
    }
    print_words(output, 19);
}

static void print_lifecycle_world(const uint32_t *world) {
    const uint32_t *phase = world + 0x191d8 / 4;
    const uint32_t summary[] = {world[0x191d4 / 4], world[0x1924c / 4],
                                phase[0],           phase[7],
                                phase[10],          world[0x19218 / 4]};
    print_words(summary, 6);
    uint32_t fixture_count = 0;
    for (size_t fixture = 0; fixture < 6; ++fixture) {
        fixture_count += lifecycle_fixtures[fixture] != 0;
    }
    uint32_t counts[] = {world[0x19230 / 4], fixture_count};
    print_words(counts, 2);
    const uint32_t *body = (const uint32_t *)(uintptr_t)world[0x19228 / 4];
    size_t visited = 0;
    while (body) {
        if (++visited > 5) {
            fail("Native lifecycle body list exceeded its bound\n");
        }
        print_step_body(body);
        body = (const uint32_t *)(uintptr_t)body[24];
    }
    if (visited != counts[0]) {
        fail("Native lifecycle body count disagrees\n");
    }
    print_step_contacts(world);
    for (size_t fixture = 0; fixture < 6; ++fixture) {
        if (lifecycle_fixtures[fixture]) {
            print_lifecycle_fixture(
                world, (const uint32_t *)(uintptr_t)lifecycle_fixtures[fixture]);
        }
    }
    print_world_events();
}

static void probe_world_lifecycle(void) {
    LifecycleFunctions functions = {
        .create_body = (void *(*)(void *, const void *))find_function(
            "_ZN7b2World10CreateBodyEPK9b2BodyDef"),
        .create_fixture = (void *(*)(void *, const void *))find_function(
            "_ZN6b2Body13CreateFixtureEPK12b2FixtureDef"),
        .destroy_body = (void (*)(void *, void *))find_function(
            "_ZN7b2World11DestroyBodyEP6b2Body"),
        .destroy_fixture = (void (*)(void *, void *))find_function(
            "_ZN6b2Body14DestroyFixtureEP9b2Fixture"),
        .set_box = (void (*)(void *, float, float))find_function(
            "_ZN14b2PolygonShape8SetAsBoxEff"),
        .set_transform = (void (*)(void *, const void *, float))find_function(
            "_ZN6b2Body12SetTransformERK6b2Vec2f"),
        .set_active = (void (*)(void *, bool))find_function("_ZN6b2Body9SetActiveEb"),
        .circle_vtable = (const uint32_t *)find_symbol("_ZTV13b2CircleShape", 1),
        .polygon_vtable = (const uint32_t *)find_symbol("_ZTV14b2PolygonShape", 1),
    };
    void (*construct)(void *, const void *) =
        (void (*)(void *, const void *))find_function("_ZN7b2WorldC1ERK6b2Vec2");
    void (*close_world)(void *) = (void (*)(void *))find_function("_ZN7b2WorldD1Ev");
    void (*step)(void *, float, int, int) =
        (void (*)(void *, float, int, int))find_function("_ZN7b2World4StepEfii");
    uint32_t listener_vtable[] = {0,
                                  0,
                                  (uint32_t)(uintptr_t)world_begin_event,
                                  (uint32_t)(uintptr_t)world_end_event,
                                  (uint32_t)(uintptr_t)world_pre_event,
                                  (uint32_t)(uintptr_t)world_post_event};
    uint32_t listener[] = {(uint32_t)(uintptr_t)listener_vtable};
    uint32_t destruction_vtable[] = {0, 0, 0, (uint32_t)(uintptr_t)lifecycle_goodbye};
    uint32_t destruction[] = {(uint32_t)(uintptr_t)destruction_vtable};
    static uint64_t world_storage[16384];
    uint32_t *world = (uint32_t *)world_storage;
    semihost(4, "AB_ANDROID_WORLD_LIFECYCLE_V1 16 256\n");
    for (size_t scene = 0; scene < 16; ++scene) {
        uint32_t controls[] = {5,
                               6,
                               (uint32_t)(scene % 7 != 0),
                               (uint32_t)(scene % 3 != 0),
                               (uint32_t)(scene % 4 != 0),
                               scene >= 8 ? 0x3f000000u : 0,
                               0xc1200000};
        construct(world, controls + 5);
        world[0x19220 / 4] = (uint32_t)(uintptr_t)listener;
        world[0x19244 / 4] = (uint32_t)(uintptr_t)destruction;
        ((uint8_t *)world)[0x19240] = (uint8_t)controls[2];
        ((uint8_t *)world)[0x19250] = (uint8_t)controls[3];
        if (!controls[4]) {
            world[0x191d4 / 4] &= ~UINT32_C(4);
        }
        for (size_t body = 0; body < 5; ++body) {
            lifecycle_bodies[body] = 0;
        }
        for (size_t fixture = 0; fixture < 6; ++fixture) {
            lifecycle_fixtures[fixture] = 0;
        }
        world_event_count = 0;
        print_words(controls, 7);
        print_lifecycle_world(world);
        for (size_t frame = 0; frame < 256; ++frame) {
            uint32_t operation[3];
            lifecycle_operation(operation, frame);
            print_words(operation, 3);
            world_event_count = 0;
            lifecycle_apply(&functions, world, operation, scene, frame);
            print_lifecycle_world(world);
            uint32_t step_controls[] = {frame % 53 == 0 ? 0u
                                        : frame >= 128  ? 0x3c888889u
                                                        : 0x3d088889u,
                                        10, 10};
            print_words(step_controls, 3);
            world_event_count = 0;
            FloatBits seconds = {.bits = step_controls[0]};
            step(world, seconds.number, 10, 10);
            print_lifecycle_world(world);
        }
        world[0x19244 / 4] = 0;
        close_world(world);
    }
    semihost(4, "AB world lifecycle reference complete\n");
}

#endif
