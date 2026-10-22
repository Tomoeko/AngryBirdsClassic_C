#ifndef AB_TEST_WORLD_RAYCAST_FIXTURE_H
#define AB_TEST_WORLD_RAYCAST_FIXTURE_H

#include "ab/world_raycast.h"
#include "world_raycast_cases.h"
#include <string.h>

typedef struct {
    AbPhysicsWorld world;
    AbWorldSolveBody bodies[WORLD_RAY_BODIES];
    AbWorldFixture fixtures[WORLD_RAY_BODIES];
    AbFixtureMass mass[WORLD_RAY_BODIES];
    AbBodyMotion island_bodies[WORLD_RAY_BODIES];
    size_t order[WORLD_RAY_BODIES], stack[WORLD_RAY_BODIES], heads[WORLD_RAY_BODIES];
    AbPositionState positions[WORLD_RAY_BODIES];
    AbVelocity velocities[WORLD_RAY_BODIES];
    AbWorldSolveWorkspace solve;
    uint32_t input[WORLD_RAY_INPUT_WORDS];
    uint32_t events[WORLD_RAY_BODIES][WORLD_RAY_EVENT_WORDS];
    size_t count;
    AbWorldRaycastHit last;
    bool fail_callback;
} WorldRayFixture;

static float from_bits(uint32_t word) {
    float value;
    memcpy(&value, &word, sizeof(value));
    return value;
}

static uint32_t bits(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

static bool open_fixture(WorldRayFixture *fixture) {
    fixture->solve =
        (AbWorldSolveWorkspace){.bodies = fixture->island_bodies,
                                .body_order = fixture->order,
                                .stack = fixture->stack,
                                .body_heads = fixture->heads,
                                .body_capacity = WORLD_RAY_BODIES,
                                .solver = {.positions = fixture->positions,
                                           .velocities = fixture->velocities,
                                           .body_capacity = WORLD_RAY_BODIES}};
    AbWorldStorage storage = {.bodies = fixture->bodies,
                              .fixtures = fixture->fixtures,
                              .mass_scratch = fixture->mass,
                              .mass_capacity = WORLD_RAY_BODIES,
                              .body_capacity = WORLD_RAY_BODIES,
                              .fixture_capacity = WORLD_RAY_BODIES,
                              .solve = &fixture->solve};
    AbWorldDefinition definition = {.broadphase_limits = {128, 128, 128},
                                    .maximum_translation = 2,
                                    .maximum_translation_squared = 4,
                                    .allow_sleep = true,
                                    .warm_start = true,
                                    .clear_forces = true,
                                    .continuous = true};
    return ab_world_open(&fixture->world, definition, storage, NULL);
}

static bool create_body(WorldRayFixture *fixture, const uint32_t row[13]) {
    AbBodyDefinition definition = {.type = (AbBodyType)row[1],
                                   .position = {from_bits(row[2]), from_bits(row[3])},
                                   .angle = from_bits(row[4]),
                                   .gravity_scale = 1,
                                   .sleep_allowed = true,
                                   .awake = true,
                                   .active = row[5] != 0,
                                   .user_data = row[0]};
    size_t body = SIZE_MAX, created = SIZE_MAX;
    CHECK(ab_world_create_body(&fixture->world, &definition, &body, NULL));
    AbWorldFixture source = {
        .body = body,
        .friction = 0.2f,
        .sensor = row[10] != 0,
        .user_data = row[0],
        .filter = {.category = (uint16_t)row[11], .mask = (uint16_t)row[12]}};
    source.shape.kind = (AbShapeKind)row[6];
    if (source.shape.kind == AB_SHAPE_CIRCLE) {
        source.shape.data.circle =
            (AbCircle){from_bits(row[7]), {from_bits(row[8]), from_bits(row[9])}};
    } else {
        source.shape.data.polygon =
            ab_polygon_box(from_bits(row[8]), from_bits(row[9]), from_bits(row[7]));
    }
    CHECK(ab_world_create_fixture(&fixture->world, &source, &created, NULL));
    CHECK(body == row[0] - 1u && created == body);
    return true;
}

#endif
