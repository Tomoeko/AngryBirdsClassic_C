#include "ab/classic_boomerang.h"
#include "ab/coordinates.h"
#include "ab/physics_step.h"

#include <math.h>
#include <string.h>

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value =
        ab_data_get(table, (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)});
    return value ? *value : (AbDataValue){0};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Missing numeric boomerang field");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite boomerang field");
}

static bool read(const AbDataTable *table, const char *name, float *output,
                 AbError *error) {
    return numeric(field(table, name), output, error);
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool strict_true(AbDataValue value) {
    return value.kind == AB_DATA_BOOLEAN && value.bits;
}

static bool write(AbClassicLevelView level, size_t object, const char *name,
                  AbDataValue value, AbError *error) {
    return ab_level_property_set(level.properties, object, span(name), value, error);
}

static bool current_definition(AbClassicLevelView level, size_t object,
                               AbDataValue *output, AbError *error) {
    return ab_object_definition_lookup(&level.properties->lookup,
                                       level.bodies->objects[object].name, output,
                                       error) &&
           (output->kind == AB_DATA_TABLE ||
            ab_fail(error, "Missing current boomerang definition"));
}

static bool sound(AbClassicBoomerang *owner, AbDataValue name, float volume,
                  AbError *error) {
    uint32_t handle = 0;
    return ab_audio_bank_play(owner->audio,
                              (AbAudioPlay){.name = name,
                                            .volume = number(volume),
                                            .loop = {.kind = AB_DATA_BOOLEAN}},
                              &handle, error);
}

static bool prepare_turn(AbClassicBoomerang *owner, AbClassicLevelView level,
                         size_t object, const AbDataTable *definition, AbError *error) {
    AbDataTable *physical = &level.properties->objects[object].physical;
    float x = 0, y = 0;
    if (!write(level, object, "boomerangActive",
               (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1}, error) ||
        !read(physical, "xVel", &x, error))
        return false;
    if (truthy(field(definition, "facebookBoomerangMode")))
        return write(level, object, "boomerangTargetXVel", number(-1.5f * x), error);
    float coefficient = 0;
    if (x != 0) {
        if (!read(physical, "yVel", &y, error))
            return false;
        float slope = fabsf(y / x);
        coefficient = 2 - (slope < 2 ? slope : 2);
    }
    owner->y_force_coefficient = number(coefficient);
    float power = 0, mass = 0, horizontal = 0, vertical = 0, speed = 0;
    AbDataValue current = {0};
    if (!ab_classic_sling_power(owner->sling, &power, error) ||
        !read(physical, "mass", &mass, error) ||
        !current_definition(level, object, &current, error) ||
        !read(current.table, "boomerangHorizontalForce", &horizontal, error) ||
        !read(current.table, "boomerangVerticalForce", &vertical, error) ||
        !read(current.table, "boomerangMaxHorizontalSpeed", &speed, error))
        return false;
    float x_force = x * owner->physics_scale;
    x_force = x_force * mass;
    x_force = x_force * horizontal;
    x_force = x_force * power;
    float y_force = coefficient * owner->physics_scale;
    y_force = y_force * mass;
    y_force = y_force * vertical;
    y_force = y_force * power;
    return write(level, object, "boomerangXForce", number(x_force), error) &&
           write(level, object, "boomerangYForce", number(y_force), error) &&
           write(level, object, "boomerangMinXVel", number(-speed * power), error) &&
           write(level, object, "boomerangMaxXVel", number(speed * power), error);
}

bool ab_classic_boomerang_trigger(AbClassicBoomerang *owner, size_t object,
                                  const AbDataTable *definition, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(owner ? owner->level : NULL);
    if (!owner || !definition || !level.properties || !level.bodies ||
        object >= level.bodies->count || level.removed_objects[object] ||
        level.world->locked || level.world->failed || !isfinite(owner->physics_scale))
        return ab_fail(error, "Invalid boomerang specialty bindings");
    if (!ab_settings_statistics_extra_tutorial(owner->settings,
                                               field(definition, "sprite"), error) ||
        !prepare_turn(owner, level, object, definition, error))
        return false;
    AbDataValue sprite = field(definition, "spriteSpecial");
    if (!write(level, object, "sprite", sprite, error))
        return false;
    if (sprite.kind != AB_DATA_STRING)
        return ab_fail(error, "Missing actual boomerang sprite");
    AbDataValue current = {0}, selected = {0};
    if (!ab_classic_visuals_set_sprite(owner->visuals, object, sprite.string, error) ||
        !current_definition(level, object, &current, error) ||
        !ab_audio_bank_name_value(owner->audio, field(current.table, "specialSound"),
                                  &selected, error) ||
        !sound(owner, selected, 1, error))
        return false;
    AbDataTable *physical = &level.properties->objects[object].physical;
    AbVec2 position = {0}, world = {0};
    if (!read(physical, "x", &position.x, error) ||
        !read(physical, "y", &position.y, error) ||
        !ab_coordinate_point(NULL, AB_SPACE_PHYSICS, AB_SPACE_WORLD, position, &world))
        return ab_fail(error, "Unsupported boomerang puff position");
    return ab_classic_trajectories_puff(
        owner->trajectories, world,
        (AbDataValue){.kind = AB_DATA_TABLE, .table = physical}, (AbDataValue){0},
        (AbDataValue){0}, error);
}

static bool queue_force(AbClassicBoomerang *owner, const AbDataTable *physical,
                        AbVec2 force, AbError *error) {
    AbDataValue name = field(physical, "name");
    AbVec2 point = {0};
    if (name.kind != AB_DATA_STRING || !read(physical, "x", &point.x, error) ||
        !read(physical, "y", &point.y, error))
        return ab_fail(error, "Missing actual boomerang force identity/position");
    return ab_force_queue_append(owner->forces,
                                 (AbQueuedForce){name.string, force, point}, error);
}

static bool spin(AbClassicLevelView level, size_t object, float seconds, bool active,
                 AbError *error) {
    const AbDataTable *physical = &level.properties->objects[object].physical;
    AbDataValue value = field(physical, "angularVelocity");
    float angular = 0;
    if (active || value.kind != AB_DATA_NIL) {
        if (!numeric(value, &angular, error))
            return false;
    } else if (!write(level, object, "angularVelocity", number(0), error)) {
        return false;
    }
    angular = angular + seconds * (active ? 20 : 15);
    if (!write(level, object, "angularVelocity", number(angular), error))
        return false;
    float limit = active ? 20 : 6.28000020980835f;
    if (limit < angular) {
        angular = limit;
        if (!write(level, object, "angularVelocity", number(angular), error))
            return false;
    }
    size_t body = level.bodies->objects[object].physics.body;
    return (ab_world_has_body(level.world, body) &&
            ab_body_angular_velocity_set(&level.world->storage.bodies[body].motion,
                                         angular)) ||
           ab_fail(error, "Cannot spin the actual boomerang body");
}

static bool swish(AbClassicBoomerang *owner, AbClassicLevelView level, size_t object,
                  AbError *error) {
    AbDataTable *physical = &level.properties->objects[object].physical;
    float angle = 0, previous = 0;
    AbDataValue old = field(physical, "prevAngle");
    if (!read(physical, "angle", &angle, error))
        return false;
    if (old.kind != AB_DATA_NIL) {
        if (!numeric(old, &previous, error))
            return false;
        if (previous < -1 && -1 <= angle) {
            float x = 0;
            if (!level.bodies->load_edges_present || !read(physical, "x", &x, error))
                return ab_fail(error, "Missing creation-time boomerang level edges");
            float volume = 1;
            if (x < level.bodies->load_edges.x)
                volume = 1 - (level.bodies->load_edges.x - x) / 100;
            else if (level.bodies->load_edges.y < x)
                volume = 1 - (x - level.bodies->load_edges.y) / 100;
            if (volume < 0)
                volume = 0;
            owner->volume = number(volume);
            if (!sound(owner,
                       (AbDataValue){.kind = AB_DATA_STRING,
                                     .string = span("boomerang_swish")},
                       volume, error))
                return false;
        }
    }
    return write(level, object, "prevAngle", number(angle), error);
}

static bool turn(AbClassicBoomerang *owner, AbClassicLevelView level, size_t object,
                 const AbDataTable *definition, float seconds, AbError *error) {
    AbDataTable *physical = &level.properties->objects[object].physical;
    if (!swish(owner, level, object, error))
        return false;
    float x = 0, y = 0;
    if (!read(physical, "xVel", &x, error) || !read(physical, "yVel", &y, error))
        return false;
    if (truthy(field(definition, "facebookBoomerangMode"))) {
        float target = 0;
        if (!read(physical, "boomerangTargetXVel", &target, error))
            return false;
        /* setVelocity changes the native body, leaving this frame's cached
         * physical xVel untouched for the source's following comparison. */
        if (target != 0 && (target < x || x < target)) {
            float delta = (seconds * 1000) / 10;
            float velocity = target < x ? x - delta : x + delta;
            size_t body = level.bodies->objects[object].physics.body;
            if (!ab_world_has_body(level.world, body) ||
                !ab_body_velocity_set(&level.world->storage.bodies[body].motion,
                                      (AbVec2){velocity, y}))
                return ab_fail(error, "Cannot turn the actual Facebook boomerang");
        }
    } else {
        float minimum = 0, maximum = 0, force = 0;
        if (!read(physical, "boomerangMinXVel", &minimum, error))
            return false;
        if (minimum <= x) {
            if (!read(physical, "boomerangMaxXVel", &maximum, error))
                return false;
            if (x <= maximum &&
                (!read(physical, "boomerangXForce", &force, error) ||
                 !queue_force(owner, physical, (AbVec2){force, 0}, error)))
                return false;
        }
        if (5 < y && (!read(physical, "boomerangYForce", &force, error) ||
                      !queue_force(owner, physical, (AbVec2){0, force}, error)))
            return false;
    }
    return spin(level, object, seconds, true, error);
}

static bool update_bird(AbClassicBoomerang *owner, AbClassicLevelView level,
                        size_t object, const AbDataTable *definition, float seconds,
                        AbError *error) {
    AbDataTable *physical = &level.properties->objects[object].physical;
    if (strict_true(field(physical, "boomerangActive")))
        return turn(owner, level, object, definition, seconds, error);
    if (owner->flying->kind != AB_DATA_TABLE || owner->flying->table != physical ||
        !truthy(*owner->specialty_available))
        return true;
    float force = 0;
    AbDataValue acceleration = field(definition, "flyVerticalAcceleration");
    if (acceleration.kind != AB_DATA_NIL) {
        float mass = 0;
        if (!numeric(acceleration, &force, error) ||
            !read(physical, "mass", &mass, error))
            return false;
        force = force * mass;
    } else if (!read(definition, "flyVerticalForce", &force, error)) {
        return false;
    }
    return queue_force(owner, physical, (AbVec2){0, force}, error) &&
           spin(level, object, seconds, false, error);
}

bool ab_classic_boomerang_update(AbClassicBoomerang *owner, float seconds,
                                 AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(owner ? owner->level : NULL);
    AbDataTable *birds = ab_classic_birds_view(owner ? owner->birds : NULL).birds;
    if (!owner || !level.bodies || !birds || !owner->flying ||
        !owner->specialty_available || !isfinite(seconds) || level.world->locked ||
        level.world->failed)
        return ab_fail(error, "Invalid boomerang update bindings");
    for (size_t cursor = 0; cursor < birds->count; ++cursor) {
        const AbDataEntry *entry = birds->entries + cursor;
        if (entry->value.kind == AB_DATA_NIL)
            continue;
        size_t object = 0;
        while (object < level.bodies->count &&
               (entry->value.kind != AB_DATA_TABLE ||
                entry->value.table != &level.properties->objects[object].physical))
            ++object;
        if (object == level.bodies->count)
            return ab_fail(error, "Foreign canonical boomerang bird table");
        if (level.removed_objects[object])
            continue;
        AbDataValue definition = {0};
        if (!current_definition(level, object, &definition, error))
            return false;
        AbDataValue specialty = field(definition.table, "specialty");
        if (strict_true(field(entry->value.table, "shot")) &&
            specialty.kind == AB_DATA_STRING && specialty.string.size == 9 &&
            !memcmp(specialty.string.data, "BOOMERANG", 9) &&
            !update_bird(owner, level, object, definition.table, seconds, error))
            return false;
    }
    return true;
}
