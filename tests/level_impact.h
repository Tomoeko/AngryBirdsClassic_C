#ifndef AB_TEST_LEVEL_IMPACT_H
#define AB_TEST_LEVEL_IMPACT_H

#include "ab/museum_impact.h"
#include "ab/score.h"
#include "ab/block_removal_pass.h"
#include "ab/bird_remove.h"
#include "ab/level_effect.h"
#include "ab/game_object_removal.h"
#include "ab/classic_level.h"
#include "ab/object_property_access.h"

enum {
    IMPACT_DEAD_MAP,
    IMPACT_GOAL_MAP,
    IMPACT_PHYSICAL_MAP,
    IMPACT_ROLLING_MAP,
    IMPACT_TELEPORTER_MAP,
    IMPACT_MAP_COUNT,
};

typedef struct {
    AbClassicLevel *owner;
    AbStringMap *maps[IMPACT_MAP_COUNT];
} LevelImpactResources;

enum {
    IMPACT_NAME,
    IMPACT_STRENGTH,
    IMPACT_DEFINITION,
    IMPACT_GOAL,
    IMPACT_X,
    IMPACT_Y,
    IMPACT_SPECIALTY,
    IMPACT_SILENT,
    IMPACT_GIFT,
    IMPACT_ROCKET,
    IMPACT_SCRIPT,
    IMPACT_BUBBLE,
    IMPACT_FIELD_COUNT,
};

typedef struct {
    AbDataEntry body_fields[23][IMPACT_FIELD_COUNT];
    AbDataEntry world_fields[23], dynamic_fields[23], container_fields[2];
    AbDataTable bodies[23], world, dynamic, container;
    AbDataTable scripts[23], bird_table, other_bird_table, hats, camera, effect;
    AbDataEntry bird_fields[23], other_bird_fields[23];
    AbBirdRemoveState bird_state;
    AbBirdRemoveHost bird_host;
    AbDataValue values[23];
    AbGameObjectCore physical[23], *birds[23];
    AbGameObjectCoreRegistry registry;
    AbGameObjectCoreHost object_host;
    AbGameObjectRemoval native;
    AbBlockRemovalState state;
    AbBlockRemovalScores scores;
    float individual[23];
    AbBlockRemovalScoreHost scoring;
    AbObjectPropertyAccessHost property_host;
    AbBlockRemovalEffectsHost effects;
    AbBlockRemovalParticlesHost particles;
    AbBlockRemovalDetachHost detach;
    AbBlockRemovalCleanupState cleanup_state;
    AbBlockRemovalCleanupHost cleanup;
    AbBlockRemovalPassHost pass;
    bool removed[23];
    size_t random_requests, floating_requests;
} LevelImpactRemoval;

/* Ordinary first-level impact projection. Captured initial Lua scalars and
 * queued-bird poses/types are explicit inputs. Presentation, script dispatch
 * and achievements are outside this physics/logical-state comparison. */
typedef AbMuseumImpactObject LevelImpactObject;

typedef struct {
    const NativeObjectBody *records;
    const AbDataTable *catalog;
    AbGameCollisionMatrix matrix;
    LevelImpactObject objects[23];
    AbMuseumImpact projection;
    AbDataEntry ground_material;
    AbDataTable ground_definition;
    LevelImpactResources *resources;
    LevelImpactRemoval removal;
    size_t callbacks;
    size_t removals;
    float score;
    bool failed;
} LevelImpact;

#include "level_impact_removal.h"

static size_t impact_body_index(const LevelImpact *impact, size_t body) {
    for (size_t index = 0; index < 23; ++index) {
        if (impact->records[index].instance.body == body) {
            return index;
        }
    }
    return SIZE_MAX;
}

static bool impact_pair_allowed(void *context, const AbPhysicsWorld *world,
                                size_t first, size_t second) {
    LevelImpact *impact = context;
    return level_pair_allowed(&impact->matrix, world, first, second);
}

static bool impact_mark_dead(LevelImpact *impact, size_t index) {
    impact->objects[index].dead = true;
    return ab_string_map_set(impact->resources->maps[IMPACT_DEAD_MAP],
                             span(impact->records[index].name),
                             (uintptr_t)(impact->removal.values + index), true, NULL);
}

static bool impact_contact(LevelImpact *impact, AbPhysicsWorld *world, size_t contact) {
    const AbWorldSolveContact *entry = world->storage.contacts + contact;
    size_t first = impact_body_index(impact, entry->first_body);
    size_t second = impact_body_index(impact, entry->second_body);
    CHECK(first < 23 && second < 23);
    bool was_dead[] = {impact->objects[first].dead, impact->objects[second].dead};
    float previous_score = impact->projection.damage_score;
    AbError error = {0};
    if (!ab_museum_impact_contact(&impact->projection, world, contact, first, second,
                                  &error)) {
        fprintf(stderr, "Impact projection: %s\n", error.message);
        return false;
    }
    impact->score = impact->score + (impact->projection.damage_score - previous_score);
    size_t indices[] = {first, second};
    for (size_t side = 0; side < 2; ++side) {
        if (!was_dead[side] && impact->objects[indices[side]].dead) {
            CHECK(impact_mark_dead(impact, indices[side]));
        }
    }
    ++impact->callbacks;
    return true;
}

static void impact_begin(void *context, AbPhysicsWorld *world, size_t contact) {
    LevelImpact *impact = context;
    if (!impact->failed && !impact_contact(impact, world, contact)) {
        impact->failed = true;
    }
}

static bool impact_after_step(LevelImpact *impact, AbPhysicsWorld *world) {
    CHECK(!impact->failed);
    AbError error = {0};
    impact_removal_sync(impact, world);
    impact->removal.scores.total =
        (AbScoreNumber){.value = impact->score, .present = true};
    if (!ab_block_removal_pass(&impact->removal.state, &impact->removal.pass, &error)) {
        fprintf(stderr, "Impact removal pass: %s\n", error.message);
        return false;
    }
    impact->score = impact->removal.scores.total.value;
    CHECK(impact_removal_membership(impact));
    for (size_t index = 0; index < 23; ++index) {
        size_t body = impact->records[index].instance.body;
        if (ab_world_has_body(world, body)) {
            CHECK(ab_collision_velocity_flush(&impact->objects[index].deferred,
                                              &world->storage.bodies[body].motion));
        }
    }
    return true;
}

static bool impact_initialize(FILE *file, AbPhysicsWorld *world,
                              const NativeObjectBody records[23],
                              const AbDataTable *catalog, LevelImpact *impact,
                              LevelImpactResources *resources) {
    *impact =
        (LevelImpact){.records = records, .catalog = catalog, .resources = resources};
    /* Generated ground has this lookup key in the native object metadata; it
     * does not use a named ordinary block definition from the catalog. */
    impact->ground_material =
        (AbDataEntry){string("materialName"), string("staticGround")};
    impact->ground_definition = (AbDataTable){&impact->ground_material, 1, 1, NULL};
    CHECK(ab_game_collision_initialize(&impact->matrix));
    impact->projection =
        (AbMuseumImpact){.objects = impact->objects, .count = 23, .catalog = catalog};
    for (size_t index = 0; index < 23; ++index) {
        uint32_t input[7];
        CHECK(read_body_words(file, input, 7) && input[0] <= 7 && input[6] <= 1);
        LevelImpactObject *object = impact->objects + index;
        object->damage = (AbLegacyCollisionBody){
            .velocity = {from_bits(input[4]), from_bits(input[5])},
            .mass = from_bits(input[3]),
            .strength = from_bits(input[1]),
            .defence = from_bits(input[2]),
            .has_strength = (input[0] & 1) != 0,
            .has_defence = (input[0] & 2) != 0,
            .ground_or_static =
                strcmp(records[index].name, "ground") == 0 ||
                records[index].instance.collider.bits == UINT32_C(0x40000000)};
        object->collided = input[6] != 0;
        object->physics = records[index].instance;
        object->script_present = true;
        object->velocity_modifier = 1;
        if (strcmp(records[index].name, "ground") != 0) {
            AbError error = {0};
            CHECK(ab_object_definition_open(catalog, span(records[index].definition),
                                            NULL, &object->definition, &error));
        } else {
            object->definition.source = &impact->ground_definition;
        }
    }
    CHECK(impact_removal_open(impact, world));
    world->listener = (AbWorldListener){
        .context = impact, .filter = impact_pair_allowed, .begin = impact_begin};
    return true;
}

static void impact_sync_velocity(const AbPhysicsWorld *world, LevelImpact *impact) {
    for (size_t index = 0; index < 23; ++index) {
        size_t body = impact->records[index].instance.body;
        if (ab_world_has_body(world, body)) {
            impact->objects[index].damage.velocity =
                world->storage.bodies[body].motion.mass.linear_velocity;
        }
    }
}

static bool impact_lua_snapshot(FILE *file, const AbPhysicsWorld *world,
                                LevelImpact *impact, size_t frame) {
    uint32_t score[1];
    CHECK(read_body_words(file, score, 1));
    CHECK(compare_level_words((uint32_t[]){number(impact->score).bits}, score, 1, frame,
                              "impact Lua", "score", 0));
    for (size_t index = 0; index < 23; ++index) {
        uint32_t expected[8];
        CHECK(read_body_words(file, expected, 8));
        size_t body = impact->records[index].instance.body;
        LevelImpactObject *object = impact->objects + index;
        bool alive = ab_world_has_body(world, body);
        uint32_t actual[] = {alive,
                             alive ? (object->damage.has_strength ? 1u : 0u) |
                                         (object->damage.has_defence ? 2u : 0u)
                                   : 0,
                             alive ? number(object->damage.strength).bits : 0,
                             alive ? number(object->damage.defence).bits : 0,
                             alive ? number(object->damage.mass).bits : 0,
                             alive ? number(object->damage.velocity.x).bits : 0,
                             alive ? number(object->damage.velocity.y).bits : 0,
                             alive && object->collided};
        CHECK(compare_level_words(actual, expected, 8, frame, "impact Lua",
                                  impact->records[index].name, index));
    }
    return true;
}

#endif
