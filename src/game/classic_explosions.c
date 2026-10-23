#include "ab/classic_explosions.h"

#include <math.h>
#include <string.h>

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(name));
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Missing numeric explosion field");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite explosion field");
}

static bool read(const AbDataTable *table, const char *name, float *output,
                 AbError *error) {
    return numeric(field(table, name), output, error);
}

static bool write(AbClassicLevelView level, size_t object, const char *name,
                  AbDataValue value, AbError *error) {
    return ab_level_property_set(level.properties, object, span(name), value, error);
}

static bool dead(AbClassicExplosions *owner, AbClassicLevelView level, size_t object,
                 AbError *error) {
    if (!owner->physical_values || object >= owner->physical_count ||
        owner->physical_values[object].kind != AB_DATA_TABLE ||
        owner->physical_values[object].table !=
            &level.properties->objects[object].physical)
        return ab_fail(error, "Missing stable explosion death-map identity");
    return ab_string_map_set(owner->dead_blocks, level.bodies->objects[object].name,
                             (uintptr_t)&owner->physical_values[object], true, error);
}

static bool inflict(AbClassicExplosions *owner, AbClassicLevelView level, size_t object,
                    float damage, AbError *error) {
    AbDataTable *physical = &level.properties->objects[object].physical;
    float strength = 0;
    if (!read(physical, "strength", &strength, error))
        return false;
    strength = strength - damage;
    if (!write(level, object, "strength", number(strength), error))
        return false;
    level.character_objects[object].goal.strength = strength;
    AbDamageSpriteNames names;
    if (!ab_classic_visuals_update_damage(owner->visuals, object, owner->workspace,
                                          &names, error) ||
        !write(level, object, "damageSprite", names.sprite, error) ||
        !write(level, object, "blinkSprite", names.blink, error) ||
        !write(level, object, "smileSprite", names.smile, error))
        return false;
    if (strength <= 0 && !dead(owner, level, object, error))
        return false;
    owner->globals->score = number(strength <= 0 ? strength : damage);
    return true;
}

static bool break_joint(void *context, AbSpan name, AbError *error) {
    AbClassicExplosions *owner = context;
    return owner->break_joint
               ? owner->break_joint(owner->context, name, error)
               : ab_fail(error, "Missing explosion joint-destruction queue");
}

static bool impulse(AbClassicExplosions *owner, AbClassicLevelView level, size_t object,
                    const AbDataTable *definition, AbVec2 delta, AbVec2 point,
                    float distance, AbError *error) {
    float radius = 0, force = 0;
    if (!read(definition, "explosionRadius", &radius, error))
        return false;
    if (!(distance < radius))
        return true;
    if (fabsf(delta.x) < 0.000001f && fabsf(delta.y) < 0.000001f) {
        delta = (AbVec2){0};
    } else {
        delta.x = delta.x / distance;
        delta.y = delta.y / distance;
    }
    if (!read(definition, "explosionForce", &force, error))
        return false;
    force = owner->physics_scale * force;
    force = force / (distance > 1 ? distance : 1);
    if (!ab_classic_joints_break(level.joints, level.bodies->objects[object].name,
                                 force, break_joint, owner, error))
        return false;
    size_t body = level.bodies->objects[object].physics.body;
    return (ab_world_has_body(level.world, body) &&
            ab_body_impulse_apply(&level.world->storage.bodies[body].motion,
                                  (AbVec2){delta.x * force, delta.y * force},
                                  &point)) ||
           ab_fail(error, "Cannot apply explosion to the actual body");
}

static bool affect(AbClassicExplosions *owner, AbClassicLevelView level, size_t object,
                   const AbDataTable *definition, AbVec2 origin, AbError *error) {
    AbDataTable *physical = &level.properties->objects[object].physical;
    if (truthy(field(physical, "controllable")) || truthy(field(physical, "ghost")) ||
        truthy(field(physical, "ignoreExplosion")))
        return true;
    AbVec2 point = {0};
    if (!read(physical, "x", &point.x, error) || !read(physical, "y", &point.y, error))
        return false;
    AbVec2 delta = {point.x - origin.x, point.y - origin.y};
    float squared = delta.x * delta.x + delta.y * delta.y;
    float distance = (float)sqrt((double)squared);
    if (!isfinite(distance))
        return ab_fail(error, "Explosion distance exceeds finite range");
    if (!impulse(owner, level, object, definition, delta, point, distance, error))
        return false;
    float radius = 0;
    AbDataValue joints = field(definition, "explosionJointDestructionRadius");
    if (truthy(joints)) {
        if (!numeric(joints, &radius, error))
            return false;
        if (distance < radius &&
            (level.joints->joint_count || level.joints->glue_count)) {
            if (!owner->destroy_connected)
                return ab_fail(error,
                               "Explosion joint metadata traversal is not bound");
            if (!owner->destroy_connected(owner->context,
                                          level.bodies->objects[object].name, error))
                return false;
        }
    }
    AbDataValue pop = field(definition, "explosionPigPopRadius");
    if (truthy(field(physical, "levelGoal")) && truthy(pop)) {
        if (!numeric(pop, &radius, error))
            return false;
        if (distance < radius) {
            if (!write(level, object, "strength", number(0), error))
                return false;
            level.character_objects[object].goal.strength = 0;
            if (!dead(owner, level, object, error))
                return false;
        }
    }
    if (!read(definition, "explosionDamageRadius", &radius, error))
        return false;
    if (distance < radius) {
        float damage = 0, defence = 0;
        if (!read(definition, "explosionDamage", &damage, error) ||
            !read(physical, "defence", &defence, error))
            return false;
        damage = damage / (distance > 1 ? distance : 1);
        damage = damage - defence;
        if (0 < damage && !inflict(owner, level, object, damage, error))
            return false;
        /* addExplosionScore152497 is the original empty method. */
    }
    return true;
}

bool ab_classic_explosions_make(AbClassicExplosions *owner, size_t object,
                                AbDataValue definition, AbDataValue sound,
                                AbDataValue scale, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(owner ? owner->level : NULL);
    if (!owner || !level.bodies || object >= level.bodies->count || !level.properties ||
        definition.kind != AB_DATA_TABLE || !definition.table ||
        !isfinite(owner->physics_scale) || level.world->locked || level.world->failed)
        return ab_fail(error, "Invalid ordinary explosion bindings");
    AbSpan name = level.bodies->objects[object].name;
    if (!ab_classic_particles_legacy(owner->particles, name, text("explosion"),
                                     number(1), true, false, error) ||
        !ab_classic_particles_legacy(owner->particles, name, text("explosionBuff"),
                                     number(1), true, false, error))
        return false;
    if (!owner->base_effect)
        return ab_fail(error, "Selected explosion level-effect class is not bound");
    float multiplier = 1;
    if (truthy(scale) && !numeric(scale, &multiplier, error))
        return false;
    AbDataValue force = field(definition.table, "explosionForce");
    if (force.kind == AB_DATA_NIL)
        return true;
    if (sound.kind != AB_DATA_NIL) {
        uint32_t handle = 0;
        if (!ab_audio_bank_play(owner->audio,
                                (AbAudioPlay){.name = sound,
                                              .volume = number(0.7f),
                                              .loop = {.kind = AB_DATA_BOOLEAN},
                                              .channel = number(1)},
                                &handle, error))
            return false;
    }
    float amount = 0;
    if (!numeric(force, &amount, error))
        return false;
    amount = multiplier * amount;
    amount = amount / 1000;
    if (!ab_camera_shake_add(owner->shake, 0.15f, amount))
        return ab_fail(error, "Unsupported explosion camera shake");
    const AbDataTable *physical = &level.properties->objects[object].physical;
    AbVec2 origin = {0};
    if (!read(physical, "x", &origin.x, error) ||
        !read(physical, "y", &origin.y, error))
        return false;
    AbClassicWorldCursor cursor = {0};
    for (;;) {
        size_t target = SIZE_MAX;
        bool present = false;
        if (!ab_classic_level_world_next(owner->level, &cursor, &target, &present,
                                         error))
            return false;
        if (!present)
            return true;
        if (target != object &&
            !affect(owner, level, target, definition.table, origin, error))
            return false;
    }
}
