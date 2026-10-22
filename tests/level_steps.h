#ifndef AB_TEST_LEVEL_STEPS_H
#define AB_TEST_LEVEL_STEPS_H

#include "ab/game_collision.h"

typedef struct {
    ObjectBodyStorage objects;
    AbWorldSolveContact contacts[128];
    AbFixturePair pairs[128];
    AbBodyMotion island_bodies[32];
    AbIslandContact island_contacts[128];
    size_t body_order[32], contact_order[128], stack[32], heads[32], edges[256];
    AbPositionState positions[32];
    AbVelocity velocities[32];
    AbVelocityContact velocity_contacts[128];
    AbPositionConstraint position_contacts[128];
    AbGameCollisionMatrix matrix;
} LevelStepStorage;

static bool level_pair_allowed(void *context, const AbPhysicsWorld *world, size_t first,
                               size_t second) {
    const AbWorldFixture *a = world->storage.fixtures + first;
    const AbWorldFixture *b = world->storage.fixtures + second;
    const AbLevelObject *object_a = (const void *)a->user_data;
    const AbLevelObject *object_b = (const void *)b->user_data;
    AbGameCollisionObject metadata_a = {
        (uint32_t)from_bits(object_a->physics.collider.bits),
        object_a->physics.collisions_enabled};
    AbGameCollisionObject metadata_b = {
        (uint32_t)from_bits(object_b->physics.collider.bits),
        object_b->physics.collisions_enabled};
    return ab_game_collision_allowed(a->filter, b->filter, &metadata_a, &metadata_b,
                                     context);
}

static AbWorldDefinition level_world_definition(const uint32_t controls[8]) {
    return (AbWorldDefinition){
        .gravity = {from_bits(controls[0]), from_bits(controls[1])},
        .broadphase_limits = {128, 1024, 1024},
        .maximum_translation = 2,
        .maximum_translation_squared = 4,
        .allow_sleep = controls[5] != 0,
        .warm_start = controls[6] != 0,
        .clear_forces = true,
        .continuous = controls[7] != 0};
}

static bool open_level_step_world(LevelStepStorage *memory, AbPhysicsWorld *world,
                                  const uint32_t controls[8], AbError *error) {
    *memory = (LevelStepStorage){0};
    memory->objects.solve = (AbWorldSolveWorkspace){
        .bodies = memory->island_bodies,
        .contacts = memory->island_contacts,
        .body_order = memory->body_order,
        .contact_order = memory->contact_order,
        .stack = memory->stack,
        .body_heads = memory->heads,
        .edge_next = memory->edges,
        .body_capacity = 32,
        .contact_capacity = 128,
        .solver = {memory->positions, memory->velocities, memory->velocity_contacts,
                   memory->position_contacts, 32, 128},
    };
    AbWorldStorage storage = {.bodies = memory->objects.bodies,
                              .fixtures = memory->objects.fixtures,
                              .contacts = memory->contacts,
                              .pairs = memory->pairs,
                              .contact_capacity = 128,
                              .mass_scratch = memory->objects.mass,
                              .mass_capacity = 32,
                              .solve = &memory->objects.solve,
                              .body_capacity = 32,
                              .fixture_capacity = 32};
    AbWorldDefinition definition = level_world_definition(controls);
    CHECK(ab_game_collision_initialize(&memory->matrix));
    CHECK(ab_world_open(world, definition, storage, error));
    world->listener =
        (AbWorldListener){.context = &memory->matrix, .filter = level_pair_allowed};
    return true;
}

static bool compare_level_words(const uint32_t *actual, const uint32_t *expected,
                                size_t count, size_t tick, const char *phase,
                                const char *section, size_t item) {
    bool matches = true;
    for (size_t field = 0; field < count; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr,
                    "Level step %zu %s %s %zu field %zu: %08" PRIx32 " != %08" PRIx32
                    "\n",
                    tick, phase, section, item, field, actual[field], expected[field]);
            matches = false;
        }
    }
    return matches;
}

static void level_contact_words(const AbPhysicsWorld *world, size_t index,
                                uint32_t output[24]) {
    const AbWorldSolveContact *contact = world->storage.contacts + index;
    const AbContactState *state = &contact->state;
    memset(output, 0, 24 * sizeof(*output));
    output[0] = (uint32_t)world->storage.pairs[index].first;
    output[1] = (uint32_t)world->storage.pairs[index].second;
    output[2] = state->flags;
    uint32_t manifold[16];
    memcpy(manifold, &state->manifold, sizeof(manifold));
    if (state->manifold.point_count) {
        memcpy(output + 3, manifold,
               state->manifold.point_count * 5 * sizeof(uint32_t));
        memcpy(output + 13, manifold + 10, 6 * sizeof(uint32_t));
    }
    output[19] = number(state->friction).bits;
    output[20] = number(state->restitution).bits;
    output[21] = number(state->tangent_speed).bits;
    output[22] = contact->impact_count;
    output[23] = state->flags & 32 ? number(contact->impact_fraction).bits : 0;
}

typedef enum {
    LEVEL_ALL_BOUNDS,
    LEVEL_SKIP_LEADING_FAT,
    LEVEL_SKIP_BIRD_FAT,
    LEVEL_IMPACT_BOUNDS,
} LevelBoundsScope;

static bool compare_level_snapshot(FILE *file, const AbPhysicsWorld *world,
                                   const NativeObjectBody records[23], size_t tick,
                                   const char *phase, LevelBoundsScope bounds_scope) {
    uint32_t expected[30], actual[30];
    CHECK(read_body_words(file, expected, 6));
    const uint32_t summary[] = {
        (world->new_fixtures ? 1u : 0u) | (world->locked ? 2u : 0u) |
            (world->definition.clear_forces ? 4u : 0u),
        number(world->inverse_previous_step).bits,
        (uint32_t)world->broadphase.proxy_count,
        (uint32_t)world->contact_count,
        (uint32_t)world->body_count,
        (uint32_t)world->fixture_count,
    };
    CHECK(compare_level_words(summary, expected, 6, tick, phase, "world", 0));
    for (size_t index = 0; index < 23; ++index) {
        const AbObjectBody *object = &records[index].instance;
        if (bounds_scope == LEVEL_IMPACT_BOUNDS) {
            CHECK(read_body_words(file, expected, 1));
            CHECK(expected[0] == ab_world_has_body(world, object->body));
            if (!expected[0]) {
                continue;
            }
        }
        CHECK(read_body_words(file, expected, 30));
        write_object_body(world->storage.bodies + object->body, actual);
        CHECK(compare_level_words(actual, expected, 30, tick, phase,
                                  records[index].name, index));
        CHECK(read_body_words(file, expected, 9));
        const AbFixtureProxy *proxy = &world->storage.fixtures[object->fixture].proxy;
        AbAabb fat = world->broadphase.tree.nodes[proxy->proxy].bounds;
        actual[0] = (uint32_t)proxy->proxy;
        const float bounds[] = {proxy->bounds.lower.x, proxy->bounds.lower.y,
                                proxy->bounds.upper.x, proxy->bounds.upper.y,
                                fat.lower.x,           fat.lower.y,
                                fat.upper.x,           fat.upper.y};
        for (size_t field = 0; field < 8; ++field) {
            bool omit = (bounds_scope == LEVEL_SKIP_LEADING_FAT &&
                         records[index].leading_bird) ||
                        (bounds_scope == LEVEL_SKIP_BIRD_FAT &&
                         object->collider.bits == UINT32_C(0x40400000)) ||
                        (bounds_scope == LEVEL_IMPACT_BOUNDS &&
                         object->collider.bits == UINT32_C(0x40400000));
            actual[field + 1] = field >= 4 && omit ? 0 : number(bounds[field]).bits;
        }
        CHECK(compare_level_words(actual, expected, 9, tick, phase, "proxy", index));
    }
    for (size_t index = 0; index < world->contact_count; ++index) {
        CHECK(read_body_words(file, expected, 24));
        level_contact_words(world, index, actual);
        CHECK(compare_level_words(actual, expected, 24, tick, phase, "contact", index));
    }
    return true;
}

static bool replay_level_steps(FILE *file, AbPhysicsWorld *world,
                               const NativeObjectBody records[23],
                               const uint32_t controls[8]) {
    size_t bird = SIZE_MAX;
    for (size_t index = 0; index < 23; ++index) {
        if (records[index].leading_bird) {
            CHECK(bird == SIZE_MAX);
            bird = records[index].instance.body;
        }
    }
    CHECK(bird != SIZE_MAX);
    for (size_t tick = 0; tick < 8; ++tick) {
        uint32_t pose[4];
        CHECK(read_body_words(file, pose, 4) && pose[0] <= 1);
        AbError error = {0};
        if (pose[0]) {
            CHECK(ab_world_set_transform(
                world, bird, (AbVec2){from_bits(pose[1]), from_bits(pose[2])},
                from_bits(pose[3]), &error));
        }
        CHECK(compare_level_snapshot(file, world, records, tick, "before",
                                     LEVEL_SKIP_LEADING_FAT));
        if (!ab_world_step(world, from_bits(controls[2]), controls[3], controls[4],
                           &error)) {
            fprintf(stderr, "Level step %zu: %s\n", tick, error.message);
            return false;
        }
        CHECK(compare_level_snapshot(file, world, records, tick, "after",
                                     LEVEL_SKIP_LEADING_FAT));
    }
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB level native steps complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    puts("All 8 native level steps match: 23 body states, ordered contacts and fixture "
         "bounds. Sling poses are inputs; leading-bird fat bounds excluded.");
    return true;
}

#include "level_frames.h"
#include "level_impact.h"
#include "level_reload.h"
#include "level_idle.h"
#include "level_results.h"
#include "level_camera.h"
#include "level_flight.h"
#include "level_retry.h"

static bool level_sequence_checks(const char *blocks_path, const char *level_path,
                                  const char *bodies_path, const char *steps_path,
                                  bool frames, const char *flight_path) {
    AbBuffer inputs[2] = {0};
    AbLuaChunk chunks[2] = {0};
    AbLuaData data[2] = {0};
    AbClassicLevel *owned_level = NULL;
    NativeObjectBody records[23] = {0};
    const char *paths[] = {blocks_path, level_path};
    FILE *bodies = NULL, *steps = NULL, *flight = NULL;
    AbError error = {0};
    bool okay = false;
    for (size_t index = 0; index < 2; ++index) {
        if (!ab_file_read(paths[index], 8 * 1024 * 1024, inputs + index, &error) ||
            !ab_lua_read((AbSpan){inputs[index].data, inputs[index].size},
                         8 * 1024 * 1024, chunks + index, &error) ||
            !ab_lua_data(chunks + index, 8 * 1024 * 1024, data + index, &error)) {
            goto release_data;
        }
    }
    bodies = fopen(bodies_path, "rb");
    steps = fopen(steps_path, "rb");
    if (!bodies || !steps) {
        perror("Level reference");
        goto release_data;
    }
    char header[80];
    uint32_t controls[8];
    bool header_read = fgets(header, sizeof(header), steps) != NULL;
    bool extended = frames && header_read &&
                    strcmp(header, "AB_LEVEL_NATIVE_FRAMES_V2 12 23\n") == 0;
    const char *expected_header = extended ? "AB_LEVEL_NATIVE_FRAMES_V2 12 23\n"
                                  : frames ? "AB_LEVEL_NATIVE_FRAMES_V1 10 23\n"
                                           : "AB_LEVEL_NATIVE_STEPS_V1 8 23\n";
    okay = header_read && strcmp(header, expected_header) == 0 &&
           read_body_records(bodies, records) && read_body_words(steps, controls, 8) &&
           open_native_classic_level(data[0].globals, data + 1, records,
                                     level_world_definition(controls), &owned_level,
                                     &error);
    AbClassicLevelView view = ab_classic_level_view(owned_level);
    if (okay) {
        okay = frames ? replay_level_frames(steps, view.world, records, controls,
                                            extended, view.frame, NULL, NULL)
                      : replay_level_steps(steps, view.world, records, controls);
    }
    if (okay && flight_path) {
        flight = fopen(flight_path, "rb");
        okay = flight &&
               replay_level_flight(flight, view.world, records, controls,
                                   data[0].globals, view.bodies->objects, owned_level);
    }
release_data:
    if (!okay && error.message[0]) {
        fprintf(stderr, "Level replay: %s\n", error.message);
    }
    if (bodies && fclose(bodies) != 0) {
        okay = false;
    }
    if (steps && fclose(steps) != 0) {
        okay = false;
    }
    if (flight && fclose(flight) != 0) {
        okay = false;
    }
    ab_classic_level_close(owned_level);
    for (size_t index = 0; index < 2; ++index) {
        ab_lua_data_close(data + index);
        ab_lua_close(chunks + index);
        ab_buffer_free(inputs + index);
    }
    return okay;
}

#endif
