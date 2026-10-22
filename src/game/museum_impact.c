#include "ab/museum_impact.h"
#include "ab/game_collision.h"
#include "ab/score.h"

#include <math.h>
#include <string.h>

static AbSpan field_name(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static const AbDataValue *table_field(const AbDataTable *table, const char *name) {
    return table ? ab_data_get(table, (AbDataValue){.kind = AB_DATA_STRING,
                                                    .string = field_name(name)})
                 : NULL;
}

static float value_number(const AbDataValue *value) {
    float result;
    memcpy(&result, &value->bits, sizeof(result));
    return result;
}

static bool damage_factor(const AbMuseumImpact *state, size_t bird, size_t target,
                          const char *field, float *output, AbError *error) {
    AbDataValue factors = ab_object_property(&state->objects[bird].definition,
                                             field_name("damageFactors"));
    AbDataValue material = ab_object_property(&state->objects[target].definition,
                                              field_name("materialName"));
    const AbDataValue *all = table_field(state->catalog, "damageFactors");
    if (factors.kind != AB_DATA_STRING || material.kind != AB_DATA_STRING || !all ||
        all->kind != AB_DATA_TABLE) {
        return ab_fail(error, "Missing museum impact damage-factor metadata");
    }
    const AbDataValue *entry = ab_data_get(all->table, factors);
    const AbDataValue *values =
        entry && entry->kind == AB_DATA_TABLE ? table_field(entry->table, field) : NULL;
    if (!values || values->kind != AB_DATA_TABLE) {
        return ab_fail(error, "Missing museum impact multiplier table");
    }
    const AbDataValue *value = ab_data_get(values->table, material);
    if (value && value->kind != AB_DATA_NUMBER &&
        !(value->kind == AB_DATA_BOOLEAN && !value->bits)) {
        return ab_fail(error, "Unsupported museum impact multiplier value");
    }
    *output = value && value->kind == AB_DATA_NUMBER ? value_number(value) : 1;
    return true;
}

static bool add_damage_score(AbMuseumImpact *state, float damage, AbError *error) {
    if (damage > 0) {
        float score;
        if (!ab_damage_score(damage, &score)) {
            return ab_fail(error, "Unsupported museum impact damage score");
        }
        state->damage_score = state->damage_score + score;
        if (!isfinite(state->damage_score)) {
            return ab_fail(error, "Museum impact damage score overflow");
        }
    }
    return true;
}

static bool bird_contact(AbMuseumImpact *state, AbPhysicsWorld *world, size_t contact,
                         size_t bird, size_t target, AbError *error) {
    AbMuseumImpactObject *source = state->objects + bird;
    AbMuseumImpactObject *other = state->objects + target;
    float damage_multiplier, velocity_multiplier;
    if (!damage_factor(state, bird, target, "damageMultiplier", &damage_multiplier,
                       error) ||
        !damage_factor(state, bird, target, "velocityMultiplier", &velocity_multiplier,
                       error)) {
        return false;
    }
    AbDataValue legacy =
        ab_object_property(&source->definition, field_name("useLegacyCollisionPath"));
    AbBirdDamageInput input = {
        .velocity = source->damage.velocity,
        .mass = source->damage.mass,
        .target_strength = other->damage.strength,
        .target_defence = other->damage.defence,
        .damage_multiplier = damage_multiplier,
        .velocity_multiplier = velocity_multiplier,
        .velocity_modifier = source->velocity_modifier,
        .has_target_strength = other->damage.has_strength,
        .bird_script_present = source->script_present,
        .target_script_present = other->script_present,
        .target_ignores_damage = other->damage.ignore_all_damage,
        .legacy_velocity = legacy.kind != AB_DATA_NIL &&
                           (legacy.kind != AB_DATA_BOOLEAN || legacy.bits != 0)};
    AbBirdDamage result;
    if (!ab_bird_collision_damage(&input, &result)) {
        return ab_fail(error, "Unsupported museum bird impact");
    }
    other->damage.strength = result.remaining_strength;
    other->dead = other->dead || result.mark_dead;
    if (result.disable_contact) {
        world->storage.contacts[contact].state.flags &= ~(uint32_t)AB_CONTACT_ENABLED;
    }
    if (result.velocity_change == AB_BIRD_VELOCITY_IMMEDIATE) {
        if (!ab_body_velocity_set(&world->storage.bodies[source->physics.body].motion,
                                  result.velocity)) {
            return ab_fail(error, "Cannot assign museum impact velocity");
        }
    } else if (result.velocity_change == AB_BIRD_VELOCITY_DEFERRED) {
        if (!ab_collision_velocity_offer(&source->deferred, result.velocity)) {
            return ab_fail(error, "Cannot retain museum impact velocity");
        }
    }
    if (state->bird_collision) {
        const AbWorldSolveContact *entry = world->storage.contacts + contact;
        if (!source->script_present || !other->script_present ||
            !entry->state.manifold.point_count)
            return ab_fail(error, "Unsupported museum bird callback binding");
        AbWorldManifold manifold = {0};
        ab_world_manifold(
            &manifold, &entry->state.manifold,
            world->storage.bodies[entry->first_body].motion.mass.transform,
            entry->first_radius,
            world->storage.bodies[entry->second_body].motion.mass.transform,
            entry->second_radius);
        AbMuseumImpactBirdCall call = {.bird = bird,
                                       .target = target,
                                       .force = result.collision_force,
                                       .damage = result.callback_damage};
        AbMuseumCollisionKind kind = entry->first_body == source->physics.body
                                         ? AB_MUSEUM_FIRST_BIRD_COLLISION
                                         : AB_MUSEUM_SECOND_BIRD_COLLISION;
        if (!ab_museum_callback_contact(kind, manifold.normal, manifold.points[0],
                                        &call.contact))
            return ab_fail(error, "Unsupported museum bird callback geometry");
        if (!state->bird_collision(state->context, &call, error))
            return false;
    }
    source->collided = true;
    return add_damage_score(state, result.callback_damage, error);
}

bool ab_museum_impact_contact(AbMuseumImpact *state, AbPhysicsWorld *world,
                              size_t contact, size_t first, size_t second,
                              AbError *error) {
    if (!state || !world || world->failed || !state->objects || !state->catalog ||
        first >= state->count || second >= state->count || first == second ||
        contact >= world->contact_count || !isfinite(state->damage_score)) {
        return ab_fail(error, "Invalid museum impact contact");
    }
    AbMuseumImpactObject *a = state->objects + first;
    AbMuseumImpactObject *b = state->objects + second;
    const AbWorldSolveContact *entry = world->storage.contacts + contact;
    if (!ab_world_has_body(world, a->physics.body) ||
        !ab_world_has_body(world, b->physics.body) ||
        entry->first_body != a->physics.body || entry->second_body != b->physics.body ||
        a->physics.collider.kind != AB_DATA_NUMBER ||
        b->physics.collider.kind != AB_DATA_NUMBER) {
        return ab_fail(error, "Museum impact object/contact order mismatch");
    }
    float colliders[] = {value_number(&a->physics.collider),
                         value_number(&b->physics.collider)};
    if (!isfinite(colliders[0]) || !isfinite(colliders[1]) || colliders[0] < 0 ||
        colliders[1] < 0 || colliders[0] >= AB_GAME_COLLIDER_TYPES ||
        colliders[1] >= AB_GAME_COLLIDER_TYPES ||
        floorf(colliders[0]) != colliders[0] || floorf(colliders[1]) != colliders[1]) {
        return ab_fail(error, "Unsupported museum impact collider");
    }
    AbMuseumCollisionKind kind;
    if (!ab_museum_collision_kind((uint32_t)colliders[0], (uint32_t)colliders[1],
                                  &kind) ||
        kind == AB_MUSEUM_BIRD_PAIR_COLLISION) {
        return ab_fail(error, "Unsupported museum impact dispatch");
    }
    if (kind == AB_MUSEUM_FIRST_BIRD_COLLISION) {
        return bird_contact(state, world, contact, first, second, error);
    }
    if (kind == AB_MUSEUM_SECOND_BIRD_COLLISION) {
        return bird_contact(state, world, contact, second, first, error);
    }
    AbLegacyCollisionBody input[] = {a->damage, b->damage};
    AbLegacyBlockDamage result;
    if (!ab_legacy_block_damage(input, &result)) {
        return ab_fail(error, "Unsupported museum block impact");
    }
    a->damage.strength = result.remaining_strength[0];
    b->damage.strength = result.remaining_strength[1];
    a->dead = a->dead || result.mark_dead[0];
    b->dead = b->dead || result.mark_dead[1];
    return add_damage_score(state, result.callback_damage, error);
}
