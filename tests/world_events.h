#ifndef AB_TEST_WORLD_EVENTS_H
#define AB_TEST_WORLD_EVENTS_H

typedef struct {
    uint32_t records[128][36];
    size_t count;
    bool invalid;
    bool reject_impacts;
    size_t rejected;
} WorldEvents;

static void normalize_manifold(uint32_t output[16], const AbManifold *manifold) {
    memset(output, 0, 16 * sizeof(uint32_t));
    if (!manifold->point_count) {
        return;
    }
    uint32_t input[16];
    memcpy(input, manifold, sizeof(input));
    memcpy(output, input, manifold->point_count * 5 * sizeof(uint32_t));
    memcpy(output + 10, input + 10, 6 * sizeof(uint32_t));
}

static void capture_world_event(WorldEvents *events, uint32_t kind,
                                AbPhysicsWorld *world, size_t contact,
                                const AbManifold *previous,
                                const AbContactImpulse *impulse) {
    if (events->count >= 128 || contact >= world->contact_count ||
        world->storage.contacts[contact].state.manifold.point_count > 2 ||
        (previous && previous->point_count > 2) || (impulse && impulse->count > 2)) {
        events->invalid = true;
        return;
    }
    uint32_t *output = events->records[events->count++];
    memset(output, 0, 36 * sizeof(uint32_t));
    uint32_t key = contact_key(world, contact);
    output[0] = kind;
    output[1] = key >> 16;
    output[2] = key & 0xffff;
    output[3] = world->storage.contacts[contact].state.flags;
    normalize_manifold(output + 4, &world->storage.contacts[contact].state.manifold);
    if (previous) {
        normalize_manifold(output + 20, previous);
    }
    if (impulse) {
        output[20] = (uint32_t)impulse->count;
        for (size_t point = 0; point < impulse->count; ++point) {
            output[21 + point] = to_bits(impulse->normal[point]);
            output[23 + point] = to_bits(impulse->tangent[point]);
        }
    }
}

static void capture_begin(void *context, AbPhysicsWorld *world, size_t contact) {
    capture_world_event(context, 1, world, contact, NULL, NULL);
}

static void capture_end(void *context, AbPhysicsWorld *world, size_t contact) {
    capture_world_event(context, 2, world, contact, NULL, NULL);
}

static void capture_pre_solve(void *context, AbPhysicsWorld *world, size_t contact,
                              const AbManifold *previous) {
    capture_world_event(context, 3, world, contact, previous, NULL);
    WorldEvents *events = context;
    if (events->reject_impacts && (world->storage.contacts[contact].state.flags & 32)) {
        world->storage.contacts[contact].state.flags &= ~UINT32_C(4);
        ++events->rejected;
    }
}

static void capture_post_solve(void *context, AbPhysicsWorld *world, size_t contact,
                               const AbContactImpulse *impulse) {
    capture_world_event(context, 4, world, contact, NULL, impulse);
}

static bool compare_events(FILE *file, const WorldEvents *events, size_t scene,
                           size_t frame, size_t *counts) {
    uint32_t count;
    CHECK(read_words(file, &count, 1));
    if (events->invalid || count != events->count) {
        fprintf(stderr, "World event count differs at scene %zu frame %zu\n", scene,
                frame);
        return false;
    }
    for (size_t event = 0; event < count; ++event) {
        uint32_t expected[36];
        CHECK(read_words(file, expected, 36));
        CHECK(compare_words(events->records[event], expected, 36, scene, frame, "event",
                            event));
        CHECK(expected[0] >= 1 && expected[0] <= 6);
        ++counts[expected[0]];
    }
    return true;
}

#endif
