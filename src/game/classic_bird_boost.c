#include "ab/classic_bird_boost.h"
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

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Missing numeric boost field");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite boost field");
}

static bool impulse(const AbClassicBirdBoost *boost, AbClassicLevelView level,
                    size_t object, AbError *error) {
    const AbDataTable *physical = &level.properties->objects[object].physical;
    float mass, x, y;
    if (!numeric(field(physical, "mass"), &mass, error) ||
        !numeric(field(physical, "xVel"), &x, error) ||
        !numeric(field(physical, "yVel"), &y, error))
        return false;
    float force = boost->force * boost->physics_scale;
    force = force * mass;
    if (fabsf(x) < 0.000001f && fabsf(y) < 0.000001f) {
        x = y = 0;
    } else {
        float xx = x * x, yy = y * y;
        float length = (float)sqrt((double)(xx + yy));
        x = x / length;
        y = y / length;
    }
    AbVec2 value = {-x * force, -y * force};
    size_t body = level.bodies->objects[object].physics.body;
    return (ab_world_has_body(level.world, body) &&
            ab_body_impulse_apply(&level.world->storage.bodies[body].motion, value,
                                  NULL)) ||
           ab_fail(error, "Cannot apply boost to the actual bird body");
}

static bool emit(const AbClassicBirdBoost *boost, AbClassicLevelView level,
                 size_t object, AbError *error) {
    AbDataValue particles;
    if (!ab_level_property_get(level.properties, object, span("particles"), &particles,
                               error))
        return false;
    if (particles.kind != AB_DATA_TABLE)
        return ab_fail(error, "Missing actual boost particle descriptor");
    AbDataValue name = field(&level.properties->objects[object].physical, "name");
    return (name.kind == AB_DATA_STRING &&
            ab_classic_particles_removal(boost->particles, name.string,
                                         field(particles.table, "boost"), false, false,
                                         error)) ||
           (error && error->message[0]
                ? false
                : ab_fail(error, "Missing actual boost bird name"));
}

static bool sound(const AbClassicBirdBoost *boost, AbClassicLevelView level,
                  size_t object, AbError *error) {
    AbDataValue definition, selected;
    if (!ab_object_definition_lookup(&level.properties->lookup,
                                     level.bodies->objects[object].name, &definition,
                                     error))
        return false;
    if (definition.kind != AB_DATA_TABLE)
        return ab_fail(error, "Missing current boost sound definition");
    if (!ab_audio_bank_name_value(boost->audio, field(definition.table, "specialSound"),
                                  &selected, error))
        return false;
    uint32_t handle;
    return ab_audio_bank_play(
        boost->audio,
        (AbAudioPlay){.name = selected,
                      .volume = {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3f800000)},
                      .loop = {.kind = AB_DATA_BOOLEAN}},
        &handle, error);
}

bool ab_classic_bird_boost(const AbClassicBirdBoost *boost, size_t object,
                           const AbDataTable *definition, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(boost ? boost->level : NULL);
    if (!boost || !definition || !level.properties || !level.bodies ||
        object >= level.bodies->count || level.removed_objects[object] ||
        level.world->locked || level.world->failed || !isfinite(boost->force) ||
        !isfinite(boost->physics_scale))
        return ab_fail(error, "Invalid ordinary bird boost bindings");
    if (!ab_settings_statistics_extra_tutorial(boost->settings,
                                               field(definition, "sprite"), error) ||
        !impulse(boost, level, object, error) || !emit(boost, level, object, error) ||
        !sound(boost, level, object, error))
        return false;
    AbDataValue sprite = field(definition, "spriteSpecial");
    if (!ab_level_property_set(level.properties, object, span("sprite"), sprite, error))
        return false;
    if (sprite.kind != AB_DATA_STRING)
        return ab_fail(error, "Missing actual boosted bird sprite");
    if (!ab_classic_visuals_set_sprite(boost->visuals, object, sprite.string, error))
        return false;
    AbDataTable *physical = &level.properties->objects[object].physical;
    AbVec2 physics, world;
    if (!numeric(field(physical, "x"), &physics.x, error) ||
        !numeric(field(physical, "y"), &physics.y, error) ||
        !ab_coordinate_point(NULL, AB_SPACE_PHYSICS, AB_SPACE_WORLD, physics, &world))
        return ab_fail(error, "Unsupported boost puff position");
    return ab_classic_trajectories_puff(
        boost->trajectories, world,
        (AbDataValue){.kind = AB_DATA_TABLE, .table = physical}, (AbDataValue){0},
        (AbDataValue){0}, error);
}
