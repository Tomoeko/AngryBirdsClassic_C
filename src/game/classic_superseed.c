#include "ab/classic_superseed.h"

#include "ab/script_math.h"

#include <math.h>
#include <string.h>

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(name));
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool named(AbDataValue value, const char *name) {
    AbSpan wanted = span(name);
    return value.kind == AB_DATA_STRING && value.string.size == wanted.size &&
           !memcmp(value.string.data, wanted.data, wanted.size);
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    memcpy(output, &value.bits, sizeof(*output));
    return (value.kind == AB_DATA_NUMBER && isfinite(*output)) ||
           ab_fail(error, "Super Seeds requires a finite source number");
}

static bool write(AbDataTable *table, const char *key, AbDataValue value,
                  AbError *error) {
    AbObjectPropertyAccess access = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = table}};
    return ab_object_property_set(&access, span(key), value, error);
}

bool ab_classic_superseed_radius(AbDataTable *physical, const AbDataTable *definition,
                                 float *output, AbError *error) {
    if (!physical || !definition || !output)
        return ab_fail(error, "Missing Super Seeds radius binding");
    AbDataValue timer = field(physical, "superseed_timer");
    float elapsed = 0, target;
    if (!numeric(field(physical, "superseed_target_radius"), &target, error) ||
        (timer.kind != AB_DATA_NIL && !numeric(timer, &elapsed, error)))
        return false;
    if (timer.kind == AB_DATA_NIL || elapsed >= 0.5f) {
        if (!write(physical, "superseed_timer", (AbDataValue){0}, error))
            return false;
        *output = target;
        return true;
    }
    float base, oscillation, envelope;
    float phase = elapsed * (5.0f / 0.5f);
    phase = (phase - 0.5f) * 0x1.921fb6p+1f;
    if (!numeric(field(definition, "radius"), &base, error) ||
        !ab_script_sine(phase, &oscillation) ||
        !ab_script_sine((elapsed / 0.5f) * 0x1.921fb6p+1f, &envelope))
        return false;
    float wave = (oscillation + 1) * 0.5f;
    wave = wave + 2 * envelope;
    *output = base + wave * (target - base);
    return true;
}

static bool definition(AbClassicLevelView level, size_t object,
                       const AbDataTable **output, AbError *error) {
    if (!level.properties || object >= level.bodies->count ||
        level.removed_objects[object])
        return ab_fail(error, "Super Seeds has no live bird");
    AbDataValue value;
    if (!ab_object_definition_lookup(&level.properties->lookup,
                                     level.bodies->objects[object].name, &value, error))
        return false;
    if (value.kind != AB_DATA_TABLE)
        return ab_fail(error, "Super Seeds bird has no actual definition");
    *output = value.table;
    return true;
}

bool ab_classic_superseed_resize(AbClassicSuperSeed *seed, size_t object,
                                 AbError *error) {
    if (!seed || !seed->physical || !seed->birds || !seed->simulation ||
        !seed->clear_updates)
        return ab_fail(error, "Super Seeds requires actual body lifecycle services");
    AbClassicPlayfieldView *playfield = &seed->playfield;
    AbClassicLevelView level = ab_classic_level_view(playfield->level);
    const AbDataTable *description = NULL;
    if (!definition(level, object, &description, error))
        return false;
    AbDataTable *physical = &level.properties->objects[object].physical;
    if (physical->count > AB_LEVEL_PHYSICAL_PROPERTY_CAPACITY)
        return ab_fail(error, "Super Seeds physical snapshot exceeds storage");
    AbDataEntry entries[AB_LEVEL_PHYSICAL_PROPERTY_CAPACITY];
    memcpy(entries, physical->entries, physical->count * sizeof(*entries));
    AbDataTable old = {entries, physical->count, AB_LEVEL_PHYSICAL_PROPERTY_CAPACITY,
                       NULL};
    AbSpan name = level.bodies->objects[object].name;
    if (!ab_classic_object_removal_call(seed->physical, name, error) ||
        !seed->clear_updates(seed->context, object, error) ||
        !write(&old, "updateFunction", (AbDataValue){0}, error))
        return false;
    float old_radius, radius, y;
    if (!numeric(field(&old, "radius"), &old_radius, error) ||
        !ab_classic_superseed_radius(&old, description, &radius, error) ||
        !write(&old, "radius", number(radius), error))
        return false;
    const char *height_key = truthy(field(&old, "jumpOn")) ? "oldY" : "y";
    if (!numeric(field(&old, height_key), &y, error) ||
        !write(&old, height_key, number(y - (radius - old_radius) / 2), error))
        return false;
    AbGameCircleInput input = {.name = name,
                               .radius = radius,
                               .controllable = truthy(field(&old, "controllable"))};
    AbDataValue sprite = field(&old, "sprite");
    if (sprite.kind != AB_DATA_STRING ||
        !numeric(field(&old, "x"), &input.position.x, error) ||
        !numeric(field(&old, "y"), &input.position.y, error) ||
        !numeric(field(&old, "density"), &input.density, error) ||
        !numeric(field(&old, "friction"), &input.friction, error) ||
        !numeric(field(&old, "restitution"), &input.restitution, error) ||
        !numeric(field(&old, "z_order"), &input.z_order, error) ||
        !ab_classic_level_recreate_circle(playfield->level, object, input, error) ||
        !ab_classic_render_scene_create_object(playfield->scene, playfield->visuals,
                                               object, sprite.string, input.z_order,
                                               error))
        return false;
    physical = &level.properties->objects[object].physical;
    for (size_t index = 0; index < old.count; ++index) {
        const AbDataEntry *entry = old.entries + index;
        const AbDataValue *present = ab_data_get(physical, entry->key);
        if (!present || present->kind == AB_DATA_NIL) {
            /* Keys are borrowed spans, including names without terminators. */
            AbObjectPropertyAccess access = {
                .physics_object = {.kind = AB_DATA_TABLE, .table = physical}};
            if (entry->key.kind != AB_DATA_STRING ||
                !ab_object_property_set(&access, entry->key.string, entry->value,
                                        error))
                return false;
        }
    }
    float base, collider;
    if (!numeric(field(description, "radius"), &base, error) ||
        !ab_classic_scene_set_scale(ab_classic_render_scene_objects(playfield->scene),
                                    object, (AbVec2){radius / base, radius / base},
                                    error) ||
        !numeric(field(&old, "collider"), &collider, error) || collider < 0 ||
        collider >= (float)AB_GAME_COLLIDER_TYPES || collider != truncf(collider))
        return ab_fail(error, "Invalid Super Seeds native scale or collider");
    level.bodies->objects[object].physics.collider = field(&old, "collider");
    level.frame->objects[object].collider = (uint32_t)collider;
    if (!write(physical, "useLegacyCollisionPath", (AbDataValue){0}, error))
        return false;
    AbShotState *shot = ab_classic_shots_state(playfield->shots);
    bool current = shot->current_bird_name &&
                   strlen(shot->current_bird_name) == name.size &&
                   !memcmp(shot->current_bird_name, name.data, name.size);
    if (!ab_world_set_body_type(
            level.world, level.frame->objects[object].body,
            shot->bird_ready && current ? AB_BODY_DYNAMIC : AB_BODY_STATIC, error))
        return false;
    AbShotBird *launch = ab_classic_shots_bird(playfield->shots, object);
    if (!launch)
        return ab_fail(error, "Replacement Super Seeds bird lost its launch row");
    launch->mass = level.bodies->objects[object].initial_mass;
    launch->position = level.character_objects[object].jump.position;
    if (named(field(physical, "damageFactors"), "HomingDamageFactors") &&
        !write(physical, "damageFactors", text("HomingSeedDamageFactors"), error))
        return false;
    AbDataTable *birds = ab_classic_birds_view(seed->birds).birds;
    AbObjectPropertyAccess bird_access = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = birds}};
    if (!write(physical, "superSeedBird",
               (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1}, error) ||
        !ab_object_property_set(&bird_access, name,
                                (AbDataValue){.kind = AB_DATA_TABLE, .table = physical},
                                error) ||
        !ab_classic_object_removal_register_circle(seed->physical, object, error))
        return false;
    AbDataValue hook = field(description, "onBirdAdded");
    if (truthy(hook) &&
        (!seed->bird_added || !seed->bird_added(seed->context, object, hook, error)))
        return error && error->message[0]
                   ? false
                   : ab_fail(error, "Super Seeds requires its onBirdAdded hook");
    return ab_classic_behaviors_recreate_object(
               playfield->behaviors, object, field(description, "behaviors"), error) &&
           ab_classic_simulation_recreate_circle(seed->simulation, object, error);
}

bool ab_classic_superseed_apply(AbClassicSuperSeed *seed, size_t object,
                                AbError *error) {
    AbClassicLevelView level =
        ab_classic_level_view(seed ? seed->playfield.level : NULL);
    const AbDataTable *description = NULL;
    if (!seed || !definition(level, object, &description, error))
        return false;
    AbDataTable *physical = &level.properties->objects[object].physical;
    if (truthy(field(physical, "shot")))
        return ab_fail(error, "Super Seeds requires an unshot bird");
    static const struct {
        const char *name;
        float radius, density, velocity;
    } modifiers[] = {{"RedBird", 1.5f, 2, 4.9f},
                     {"SmallBlueBird", 1.5f, 3, 2.2f},
                     {"YellowBird", 1.5f, 1.5f, 2.65f},
                     {"BasicBird2", 1.5f, 2.5f, 8},
                     {"BlackBird", 1.5f, 2, 4.9f},
                     {"RedBigBird", 1.4f, 1.01f, 1.02f},
                     {"BoomerangBird", 1.5f, 1.5f, 1.12f},
                     {"GlobeBird", 1.5f, 2, 3.5f},
                     {"PinkBird", 1.6f, 1, 16},
                     {"BlackBird_Shockwave", 1.5f, 1.45f, 4.9f}};
    float radius_factor = 1.6f, density_factor = 1, velocity_factor = 1.2f;
    for (size_t index = 0; index < sizeof(modifiers) / sizeof(*modifiers); ++index)
        if (named(field(physical, "definition"), modifiers[index].name)) {
            radius_factor = modifiers[index].radius;
            density_factor = modifiers[index].density;
            velocity_factor = modifiers[index].velocity;
            break;
        }
    float radius, density, base;
    if (!numeric(field(physical, "radius"), &radius, error) ||
        !numeric(field(physical, "density"), &density, error) ||
        !numeric(field(description, "radius"), &base, error) ||
        !write(physical, "superseed_timer", number(0), error) ||
        !write(physical, "superseed_target_radius", number(radius * radius_factor),
               error) ||
        !write(physical, "density", number(density * density_factor), error) ||
        !ab_level_property_set(level.properties, object,
                               span("velocityMultiplierModifier"),
                               number(velocity_factor), error) ||
        !ab_classic_superseed_resize(seed, object, error))
        return false;
    return write(physical, "powerup_scale", number((radius * radius_factor) / base),
                 error) &&
           write(physical, "usedPowerPotion",
                 (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1}, error);
}

bool ab_classic_superseed_update(AbClassicSuperSeed *seed, size_t object, float seconds,
                                 bool *alive, AbError *error) {
    AbClassicLevelView level =
        ab_classic_level_view(seed ? seed->playfield.level : NULL);
    if (!seed || !alive || !level.properties || object >= level.bodies->count ||
        !isfinite(seconds))
        return ab_fail(error, "Invalid Super Seeds bird continuation");
    AbDataTable *physical = &level.properties->objects[object].physical;
    float timer;
    if (!numeric(field(physical, "superseed_timer"), &timer, error) ||
        !write(physical, "superseed_timer", number(timer + seconds), error) ||
        !ab_classic_superseed_resize(seed, object, error))
        return false;
    *alive = true;
    return true;
}
