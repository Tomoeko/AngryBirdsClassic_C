#include "ab/classic_blues_child.h"

#include <math.h>
#include <string.h>

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *found = ab_data_get(table, text(name));
    return found ? *found : (AbDataValue){0};
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    float number;
    memcpy(&number, &value.bits, sizeof(number));
    if (value.kind != AB_DATA_NUMBER || !isfinite(number)) {
        ab_fail(error, "Unsupported Blues child numeric value");
        return false;
    }
    *output = number;
    return true;
}

static bool string(AbDataValue value, AbSpan *output, AbError *error) {
    if (value.kind != AB_DATA_STRING || (value.string.size && !value.string.data)) {
        ab_fail(error, "Unsupported Blues child string value");
        return false;
    }
    *output = value.string;
    return true;
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool write(AbDataTable *table, const char *key, AbDataValue value,
                  AbError *error) {
    AbObjectPropertyAccess access = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = table}};
    return ab_object_property_set(&access, span(key), value, error);
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static AbDataValue named_value(AbSpan name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = name};
}

static void trace(AbClassicBluesChild *owner, AbClassicBluesChildStage stage,
                  const AbDataValue *arguments, size_t count) {
    if (owner->config.trace)
        owner->config.trace(owner->config.trace_context, stage, arguments, count);
}

static bool concatenate(AbClassicBluesChild *owner, const AbDataTable *parent,
                        AbSpan suffix, AbSpan *output, AbError *error) {
    AbSpan prefix;
    if (!string(field(parent, "name"), &prefix, error))
        return false;
    size_t remaining = owner->config.names.size - owner->used;
    if (prefix.size > remaining || suffix.size > remaining - prefix.size ||
        prefix.size + suffix.size > UINT32_MAX)
        return ab_fail(error, "Blues child name storage is exhausted");
    size_t length = prefix.size + suffix.size;
    if (!length)
        return ab_fail(error, "Empty Blues child name");
    uint8_t *destination = owner->config.names.data + owner->used;
    /* Earlier generated parent names may belong to the same stable pool. */
    memmove(destination, prefix.data, prefix.size);
    if (suffix.size)
        memmove(destination + prefix.size, suffix.data, suffix.size);
    owner->used += length;
    *output = (AbSpan){destination, length};
    return true;
}

static bool create_circle(AbClassicBluesChild *owner, const AbDataTable *parent,
                          AbSpan name, AbVec2 offset, size_t *object, AbError *error) {
    AbGameCircleInput input = {.name = name};
    AbSpan unused_sprite;
    float x, y;
    if (!string(field(parent, "sprite"), &unused_sprite, error) ||
        !numeric(field(parent, "x"), &x, error) ||
        !numeric(field(parent, "y"), &y, error) ||
        !numeric(field(parent, "radius"), &input.radius, error) ||
        !numeric(field(parent, "density"), &input.density, error) ||
        !numeric(field(parent, "friction"), &input.friction, error) ||
        !numeric(field(parent, "restitution"), &input.restitution, error) ||
        !numeric(field(parent, "z_order"), &input.z_order, error))
        return false;
    input.position = (AbVec2){x + offset.x, y + offset.y};
    input.controllable = truthy(field(parent, "controllable"));
    const AbDataValue arguments[] = {named_value(name),
                                     named_value(unused_sprite),
                                     number(input.position.x),
                                     number(input.position.y),
                                     number(input.radius),
                                     number(input.density),
                                     number(input.friction),
                                     number(input.restitution),
                                     field(parent, "controllable"),
                                     number(input.z_order)};
    trace(owner, AB_BLUES_CHILD_CIRCLE, arguments, 10);
    return ab_classic_level_create_circle(owner->config.playfield.level, input, object,
                                          error);
}

static bool renderer(AbClassicBluesChild *owner, const AbDataTable *parent,
                     size_t object, AbError *error) {
    AbSpan sprite;
    float layer;
    if (!string(field(parent, "damageSprite"), &sprite, error) ||
        !numeric(field(parent, "z_order"), &layer, error))
        return false;
    AbClassicLevelView level = ab_classic_level_view(owner->config.playfield.level);
    const AbDataValue arguments[] = {named_value(level.bodies->objects[object].name),
                                     named_value(sprite), number(layer)};
    trace(owner, AB_BLUES_CHILD_RENDERER, arguments, 3);
    return ab_classic_render_scene_create_object(owner->config.playfield.scene,
                                                 owner->config.playfield.visuals,
                                                 object, sprite, layer, error);
}

static bool clone_fields(const AbDataTable *parent, AbDataTable *child, AbVec2 offset,
                         AbError *error) {
    static const char *const copied[] = {
        "definition",    "controllable",          "strength",
        "defence",       "materialName",          "levelGoal",
        "damageFactors", "spritePivotX",          "spritePivotY",
        "damageSprite",  "useLegacyCollisionPath"};
    for (size_t index = 0; index < sizeof(copied) / sizeof(*copied); ++index)
        if (!write(child, copied[index], field(parent, copied[index]), error))
            return false;
    if (!write(child, "shot", (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1},
               error) ||
        !write(child, "sleeping", (AbDataValue){.kind = AB_DATA_BOOLEAN}, error) ||
        !write(child, "hasCollided", (AbDataValue){.kind = AB_DATA_BOOLEAN}, error) ||
        !write(child, "parentName", field(parent, "name"), error))
        return false;
    float value;
    if (!numeric(field(parent, "xVel"), &value, error) ||
        !write(child, "xVel", number(value + offset.x * 7.0f), error) ||
        !numeric(field(parent, "yVel"), &value, error) ||
        !write(child, "yVel", number(value + offset.y * 7.0f), error) ||
        !write(child, "collider", field(parent, "collider"), error))
        return false;
    return write(child, "velocityMultiplierModifier",
                 field(parent, "velocityMultiplierModifier"), error);
}

static bool transform(AbClassicBluesChild *owner, const AbDataTable *parent,
                      size_t object, AbVec2 offset, AbError *error) {
    AbClassicPlayfieldView *playfield = &owner->config.playfield;
    AbClassicLevelView level = ab_classic_level_view(playfield->level);
    AbLevelObject *child = level.bodies->objects + object;
    AbDataValue name = named_value(child->name);
    AbSpan sprite;
    float angle;
    if (!string(field(parent, "damageSprite"), &sprite, error))
        return false;
    const AbDataValue sprite_arguments[] = {name, named_value(sprite)};
    trace(owner, AB_BLUES_CHILD_SPRITE, sprite_arguments, 2);
    if (!ab_classic_visuals_set_sprite(playfield->visuals, object, sprite, error) ||
        !numeric(field(parent, "angle"), &angle, error))
        return false;
    const AbDataValue rotation_arguments[] = {name, number(angle)};
    trace(owner, AB_BLUES_CHILD_ROTATION, rotation_arguments, 2);
    if (!ab_classic_level_set_rotation(playfield->level, object, angle, error))
        return false;
    float x, y;
    if (!numeric(field(parent, "xVel"), &x, error) ||
        !numeric(field(parent, "yVel"), &y, error))
        return false;
    AbVec2 velocity = {x + offset.x * 7.0f, y + offset.y * 7.0f};
    const AbDataValue velocity_arguments[] = {name, number(velocity.x),
                                              number(velocity.y)};
    trace(owner, AB_BLUES_CHILD_VELOCITY, velocity_arguments, 3);
    AbBodyMotion *body = &level.world->storage.bodies[child->physics.body].motion;
    if (!ab_body_velocity_set(body, velocity))
        return ab_fail(error, "Unsupported Blues child velocity");
    float collider;
    AbDataValue raw = field(parent, "collider");
    const AbDataValue collider_arguments[] = {name, raw};
    trace(owner, AB_BLUES_CHILD_COLLIDER, collider_arguments, 2);
    if (!numeric(raw, &collider, error) || collider < 0 ||
        collider >= (float)AB_GAME_COLLIDER_TYPES ||
        collider != (float)(uint32_t)collider)
        return ab_fail(error, "Unsupported Blues child collider");
    /* Native setColliderType only changes GameObject metadata. Lua collider was
     * copied earlier; fixture filtering is not rewritten by this boundary. */
    child->physics.collider = raw;
    level.frame->objects[object].collider = (uint32_t)collider;
    return true;
}

static bool bird_and_script(AbClassicBluesChild *owner, size_t object, AbSpan name,
                            AbDataTable *physical, AbError *error) {
    AbDataValue birds = *owner->config.birds;
    if (birds.kind != AB_DATA_TABLE || !birds.table)
        return ab_fail(error, "Blues child indexed a non-table birds registry");
    AbObjectPropertyAccess access = {.physics_object = birds};
    if (!ab_object_property_set(&access, name,
                                (AbDataValue){.kind = AB_DATA_TABLE, .table = physical},
                                error))
        return false;
    AbDataTable empty = {0};
    AbDataEntry entry = {text("MuseumBluesTrajectoryRenderer"),
                         {.kind = AB_DATA_TABLE, .table = &empty}};
    AbDataTable descriptor = {&entry, 1, 1, NULL};
    AbDataTableLayout layout = {
        .table = &descriptor, .hash_capacity = 1, .no_nil_assignments = true};
    AbLuaData data = {.layouts = &layout};
    const AbDataValue arguments[] = {{.kind = AB_DATA_TABLE, .table = physical},
                                     {.kind = AB_DATA_TABLE, .table = &descriptor}};
    trace(owner, AB_BLUES_CHILD_GAME_OBJECT, arguments, 2);
    return ab_classic_behaviors_create_object(
        owner->config.playfield.behaviors, object,
        (AbDataValue){.kind = AB_DATA_TABLE, .table = &descriptor}, &data, error);
}

static bool create(AbClassicBluesChild *owner, size_t parent_index, AbSpan suffix,
                   AbVec2 offset, size_t *object, AbSpan *output, AbError *error) {
    AbClassicPlayfieldView *playfield = &owner->config.playfield;
    AbClassicLevelView level = ab_classic_level_view(playfield->level);
    AbDataTable *parent = &level.properties->objects[parent_index].physical;
    AbSpan name;
    size_t index;
    if (!concatenate(owner, parent, suffix, &name, error) ||
        !create_circle(owner, parent, name, offset, &index, error) ||
        !renderer(owner, parent, index, error))
        return false;
    AbDataValue resolved = {0};
    if (!ab_object_world_lookup(&level.properties->lookup,
                                (AbDataValue){.kind = AB_DATA_STRING, .string = name},
                                &resolved, error) ||
        resolved.kind != AB_DATA_TABLE ||
        resolved.table != &level.properties->objects[index].physical)
        return ab_fail(error, "Rebound Blues child world identity");
    AbDataTable *physical = resolved.table;
    if (!clone_fields(parent, physical, offset, error) ||
        !transform(owner, parent, index, offset, error) ||
        !bird_and_script(owner, index, name, physical, error))
        return false;
    AbDataValue scale = field(parent, "powerup_scale");
    if (truthy(scale)) {
        float x, y;
        if (!numeric(scale, &x, error) ||
            !numeric(field(parent, "powerup_scale"), &y, error))
            return false;
        const AbDataValue arguments[] = {named_value(name), number(x), number(y)};
        trace(owner, AB_BLUES_CHILD_SCALE, arguments, 3);
        if (!ab_classic_scene_set_scale(
                ab_classic_render_scene_objects(playfield->scene), index,
                (AbVec2){x, y}, error))
            return false;
    }
    *object = index;
    *output = name;
    return true;
}

bool ab_classic_blues_child_initialize(AbClassicBluesChildConfig config,
                                       AbClassicBluesChild *output, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(config.playfield.level);
    if (!output || !level.bodies || !level.properties || !config.playfield.visuals ||
        !config.playfield.scene || !config.playfield.behaviors || !config.birds ||
        !config.names.data || !config.names.size ||
        ab_classic_visuals_view(config.playfield.visuals).level !=
            config.playfield.level ||
        ab_classic_render_scene_level(config.playfield.scene) != config.playfield.level)
        return ab_fail(error, "Invalid source Blues child bindings");
    *output = (AbClassicBluesChild){.config = config, .initialized = true};
    return true;
}

bool ab_classic_blues_child_create(AbClassicBluesChild *owner, size_t parent,
                                   AbSpan suffix, AbVec2 offset, size_t *object,
                                   AbSpan *name, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(
        owner && owner->initialized ? owner->config.playfield.level : NULL);
    if (!owner || !owner->initialized || owner->calling || owner->failed || !object ||
        !name || !level.bodies || parent >= level.bodies->count ||
        level.removed_objects[parent] || level.world->failed || level.world->locked ||
        (suffix.size && !suffix.data) || !isfinite(offset.x) || !isfinite(offset.y) ||
        owner->used > owner->config.names.size)
        return ab_fail(error,
                       "Invalid, failed or reentrant source Blues child creation");
    owner->calling = true;
    bool okay = create(owner, parent, suffix, offset, object, name, error);
    owner->calling = false;
    owner->failed = !okay;
    return okay;
}
