#include "ab/classic_block_collisions.h"

#include "ab/score.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AbClassicBlockCollisions *owner;
    size_t index;
    AbBlockCollisionObject object;
    AbBlockCollisionScript script;
    AbBlockCollisionDefinition definition;
    AbObjectDefinition resolved;
    AbDataValue published[3];
    AbDataValue dead_value;
} CollisionObject;

struct AbClassicBlockCollisions {
    AbClassicBlockCollisionConfig config;
    AbClassicLevelView level;
    AbBlockCollisionHost host;
    CollisionObject *objects, *active[2];
    size_t count, capacity, bytes;
    bool calling, contacting;
};

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue text(AbSpan name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = name};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static bool valid_table(const AbDataTable *table) {
    return table && table->count <= table->capacity &&
           (!table->count || table->entries);
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(span(name)));
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool same_span(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || (first.data && second.data &&
                            !memcmp(first.data, second.data, first.size)));
}

static bool same_value(AbDataValue first, AbDataValue second) {
    if (first.kind != second.kind)
        return false;
    switch (first.kind) {
        case AB_DATA_NIL:
            return true;
        case AB_DATA_STRING:
            return same_span(first.string, second.string);
        case AB_DATA_TABLE:
            return first.table == second.table;
        case AB_DATA_OBJECT:
            return first.object == second.object;
        case AB_DATA_BOOLEAN:
            return !first.bits == !second.bits;
        case AB_DATA_NUMBER:
            return first.bits == second.bits;
    }
    return false;
}

static CollisionObject *find(AbClassicBlockCollisions *owner, AbSpan name) {
    for (size_t index = 0; index < owner->count; ++index) {
        if (same_span(name, owner->level.bodies->objects[index].name))
            return owner->objects + index;
    }
    return NULL;
}

static AbDataTable *physical(CollisionObject *object) {
    return &object->owner->level.properties->objects[object->index].physical;
}

static bool refresh_object(CollisionObject *object, AbError *error) {
    AbDataTable *source = physical(object);
    if (!valid_table(source))
        return ab_fail(error, "Invalid canonical collision physical table");
    AbDataValue name = field(source, "name");
    AbSpan key = object->owner->level.bodies->objects[object->index].name;
    if (name.kind != AB_DATA_STRING || !same_span(name.string, key))
        return ab_fail(error, "Unsupported renamed collision physical object");
    AbDataValue script = field(source, "gameObjectScript");
    object->object.name = name.string;
    object->object.sensor = field(source, "sensor");
    object->object.script = truthy(script) ? &object->script : NULL;
    object->published[0] = object->object.damage_sprite = field(source, "damageSprite");
    object->published[1] = object->object.blink_sprite = field(source, "blinkSprite");
    object->published[2] = object->object.smile_sprite = field(source, "smileSprite");
    return true;
}

static bool refresh(AbClassicBlockCollisions *owner, AbError *error) {
    owner->host.sound_force_threshold = *owner->config.sound_force_threshold;
    for (size_t side = 0; side < 2; ++side) {
        if (!refresh_object(owner->active[side], error))
            return false;
    }
    return true;
}

static bool publish(AbClassicBlockCollisions *owner, AbError *error) {
    const char *const keys[] = {"damageSprite", "blinkSprite", "smileSprite"};
    for (size_t side = 0; side < 2; ++side) {
        CollisionObject *object = owner->active[side];
        const AbDataValue values[] = {object->object.damage_sprite,
                                      object->object.blink_sprite,
                                      object->object.smile_sprite};
        AbObjectPropertyAccess access = {
            .physics_object = {.kind = AB_DATA_TABLE, .table = physical(object)}};
        for (size_t index = 0; index < 3; ++index) {
            if (same_value(object->published[index], values[index]))
                continue;
            if (!ab_object_property_set(&access, span(keys[index]), values[index],
                                        error))
                return false;
            object->published[index] = values[index];
        }
    }
    return true;
}

static bool lookup(void *context, AbSpan name, AbBlockCollisionObject **output,
                   AbError *error) {
    AbClassicBlockCollisions *owner = context;
    CollisionObject *object = find(owner, name);
    if (!object)
        return ab_fail(error, "Unknown initial block collision object");
    AbDataValue current;
    if (!ab_object_world_lookup(&owner->level.properties->lookup, text(name), &current,
                                error))
        return false;
    if (current.kind != AB_DATA_TABLE || current.table != physical(object))
        return ab_fail(error, "Missing or replaced collision world object");
    if (!refresh_object(object, error))
        return false;
    *output = &object->object;
    return true;
}

static bool after_service(AbClassicBlockCollisions *owner, bool okay, AbError *error) {
    return okay && refresh(owner, error);
}

static bool level_hook(void *context, const AbBlockCollisionNotice *notice,
                       AbDataValue *result, AbError *error) {
    AbClassicBlockCollisions *owner = context;
    CollisionObject *first = notice->first->context, *second = notice->second->context;
    if (!publish(owner, error))
        return false;
    return after_service(owner,
                         owner->config.level_hook(owner->config.context, first->index,
                                                  second->index, notice, result, error),
                         error);
}

static bool definition(void *context, AbSpan name, AbBlockCollisionDefinition **output,
                       AbError *error) {
    AbClassicBlockCollisions *owner = context;
    CollisionObject *object = find(owner, name);
    AbDataValue raw;
    if (!object || !ab_object_definition_lookup(&owner->level.properties->lookup, name,
                                                &raw, error))
        return false;
    AbDataValue identifier = field(physical(object), "definition");
    if (identifier.kind != AB_DATA_STRING || raw.kind != AB_DATA_TABLE ||
        !valid_table(raw.table))
        return ab_fail(error, "Invalid canonical block collision definition");
    if (!ab_object_definition_open(owner->level.catalog, identifier.string, NULL,
                                   &object->resolved, error))
        return false;
    if (raw.table != object->resolved.source)
        return ab_fail(error, "Unsupported rebound collision catalog");
    object->definition.on_collision = field(raw.table, "onCollision");
    *output = &object->definition;
    return true;
}

static bool named(void *context, AbDataValue name, const AbBlockCollisionNotice *notice,
                  AbError *error) {
    AbClassicBlockCollisions *owner = context;
    CollisionObject *first = notice->first->context, *second = notice->second->context;
    if (!publish(owner, error))
        return false;
    if (!owner->config.named)
        return ab_fail(error, "Missing owned named block collision service");
    return after_service(owner,
                         owner->config.named(owner->config.context, name, first->index,
                                             second->index, notice, error),
                         error);
}

static bool raw_definition(CollisionObject *object, const AbDataTable **output,
                           AbError *error) {
    AbDataValue blocks = field(object->owner->level.catalog, "blocks");
    if (blocks.kind != AB_DATA_TABLE || !valid_table(blocks.table))
        return ab_fail(error, "Invalid raw collision block catalog");
    AbDataValue name = field(physical(object), "definition");
    const AbDataValue *value = ab_data_get(blocks.table, name);
    if (!value || value->kind == AB_DATA_NIL) {
        *output = NULL;
        return true;
    }
    if (value->kind != AB_DATA_TABLE || !valid_table(value->table))
        return ab_fail(error, "Invalid raw collision block definition");
    *output = value->table;
    return true;
}

static bool event_definition(void *context, AbBlockEventObject *event_object,
                             const AbDataTable **output, AbError *error) {
    AbClassicBlockCollisions *owner = context;
    CollisionObject *object = find(owner, event_object->name);
    if (!object)
        return ab_fail(error, "Unknown block script collision object");
    AbDataValue name = field(physical(object), "definition");
    if (name.kind != AB_DATA_STRING)
        return ab_fail(error, "Unsupported block script definition name");
    event_object->definition = name.string;
    return raw_definition(object, output, error);
}

static bool scripts(void *context, const AbBlockCollisionNotice *notice,
                    AbError *error) {
    AbClassicBlockCollisions *owner = context;
    CollisionObject *object = notice->first->context, *other = notice->second->context;
    if (!publish(owner, error))
        return false;
    if (!owner->config.script_objects)
        return ab_fail(error, "Missing owned block script collision attachments");
    AbBlockEventObject *event_object = owner->config.script_objects + object->index;
    if (!same_span(event_object->name, object->object.name))
        return ab_fail(error, "Mismatched block script collision attachment");
    AbDataValue arguments[] = {{.kind = AB_DATA_TABLE, .table = physical(other)},
                               number(notice->force),
                               number(notice->damage),
                               number(notice->normal.x),
                               number(notice->normal.y)};
    return after_service(
        owner,
        ab_block_event(
            event_object, span("onCollision"), arguments, 5,
            (AbBlockEventHost){.context = owner, .definition = event_definition},
            error),
        error);
}

static bool object_script(void *context, const AbBlockCollisionNotice *notice,
                          AbError *error) {
    CollisionObject *object = context;
    AbClassicBlockCollisions *owner = object->owner;
    if (!publish(owner, error))
        return false;
    AbDataValue script = field(physical(object), "gameObjectScript");
    if (script.kind != AB_DATA_TABLE ||
        script.table != &owner->level.properties->objects[object->index].script)
        return ab_fail(error, "Unsupported rebound block collision script");
    return after_service(owner,
                         ab_classic_behaviors_block_collision(
                             owner->config.behaviors, object->index, notice, error),
                         error);
}

static bool sprites(void *context, AbBlockCollisionObject *view,
                    AbDamageSpriteNames *output, AbError *error) {
    AbClassicBlockCollisions *owner = context;
    CollisionObject *object = view->context;
    return publish(owner, error) &&
           after_service(owner,
                         ab_classic_visuals_update_damage(
                             owner->config.visuals, object->index,
                             owner->config.sprite_workspace, output, error),
                         error);
}

static bool sound(void *context, AbBlockCollisionSound kind,
                  AbBlockCollisionObject *view, AbBlockCollisionDefinition *selected,
                  AbDataValue *output, AbError *error) {
    AbClassicBlockCollisions *owner = context;
    CollisionObject *object = view->context;
    CollisionObject *cached = selected->context;
    if (!publish(owner, error))
        return false;
    if (!owner->config.sound)
        return ab_fail(error, "Missing owned block collision level-effect service");
    return after_service(owner,
                         owner->config.sound(owner->config.context, kind, object->index,
                                             &cached->resolved, output, error),
                         error);
}

static bool audio_name(void *context, AbDataValue value, AbDataValue *output,
                       AbError *error) {
    AbClassicBlockCollisions *owner = context;
    return ab_audio_bank_name_value(owner->config.audio, value, output, error);
}

static bool audio(void *context, AbAudioPlay request, AbError *error) {
    AbClassicBlockCollisions *owner = context;
    uint32_t handle;
    return ab_audio_bank_play(owner->config.audio, request, &handle, error);
}

static bool particles(void *context, AbBlockCollisionObject *view, AbError *error) {
    AbClassicBlockCollisions *owner = context;
    CollisionObject *object = view->context;
    const AbDataTable *raw;
    if (!publish(owner, error) || !raw_definition(object, &raw, error))
        return false;
    if (!raw)
        return ab_fail(error, "Missing raw damage-particle definition");
    AbVec2 position = {0};
    const AbVec2 *position_pointer = NULL;
    if (field(raw, "damageStateChangedParticles").kind != AB_DATA_NIL) {
        AbDataValue x = field(physical(object), "x"), y = field(physical(object), "y");
        if (x.kind != AB_DATA_NUMBER || y.kind != AB_DATA_NUMBER)
            return ab_fail(error, "Missing canonical damage-particle position");
        memcpy(&position.x, &x.bits, sizeof(position.x));
        memcpy(&position.y, &y.bits, sizeof(position.y));
        position_pointer = &position;
    }
    return after_service(owner,
                         ab_damage_state_particles(raw, position_pointer,
                                                   owner->config.particles, error),
                         error);
}

static bool score(void *context, float damage, float *output, AbError *error) {
    (void)context;
    return ab_damage_score(damage, output) ||
           ab_fail(error, "Invalid owned block collision damage score");
}

bool ab_classic_block_collisions_open(AbClassicBlockCollisionConfig config,
                                      AbClassicBlockCollisions **output,
                                      AbError *error) {
    if (!output)
        return ab_fail(error, "Missing owned block collision output");
    *output = NULL;
    AbClassicLevelView level = ab_classic_level_view(config.level);
    if (!level.bodies || !level.properties || !level.catalog ||
        level.bodies->count != level.properties->count ||
        ab_classic_visuals_view(config.visuals).level != config.level ||
        !config.sound_force_threshold ||
        (config.script_objects && config.script_object_count != level.bodies->count) ||
        (!config.script_objects && config.script_object_count) ||
        config.storage_bytes < sizeof(AbClassicBlockCollisions))
        return ab_fail(error, "Invalid owned block collision binding or byte limit");
    size_t remaining = config.storage_bytes - sizeof(AbClassicBlockCollisions);
    size_t capacity =
        config.object_capacity ? config.object_capacity : level.bodies->count;
    if (capacity < level.bodies->count || capacity > level.bodies->capacity ||
        capacity > remaining / sizeof(CollisionObject))
        return ab_fail(error, "Owned block collision rows exceed byte limit");
    AbClassicBlockCollisions *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate owned block collisions");
    owner->config = config;
    owner->level = level;
    owner->count = level.bodies->count;
    owner->capacity = capacity;
    owner->bytes = sizeof(*owner) + capacity * sizeof(CollisionObject);
    owner->objects = calloc(capacity, sizeof(*owner->objects));
    if (owner->count && !owner->objects) {
        ab_classic_block_collisions_close(owner);
        return ab_fail(error, "Cannot allocate owned block collision rows");
    }
    for (size_t index = 0; index < owner->count; ++index) {
        CollisionObject *object = owner->objects + index;
        object->owner = owner;
        object->index = index;
        object->object.context = object;
        object->script = (AbBlockCollisionScript){object, object_script};
        object->definition.context = object;
        object->dead_value =
            (AbDataValue){.kind = AB_DATA_TABLE, .table = physical(object)};
    }
    owner->host = (AbBlockCollisionHost){.context = owner,
                                         .object = lookup,
                                         .level = config.level_hook ? level_hook : NULL,
                                         .definition = definition,
                                         .named = named,
                                         .scripts = scripts,
                                         .sprites = sprites,
                                         .sound = sound,
                                         .audio_name = audio_name,
                                         .audio = audio,
                                         .particles = particles,
                                         .score = score,
                                         .scores = config.scores};
    *output = owner;
    return true;
}

bool ab_classic_block_collisions_register_object(AbClassicBlockCollisions *owner,
                                                 size_t object, AbError *error) {
    if (!owner || owner->calling || owner->contacting || object != owner->count ||
        object >= owner->capacity || object >= owner->level.bodies->count)
        return ab_fail(error, "Invalid new block-collision row");
    CollisionObject *entry = owner->objects + object;
    entry->owner = owner;
    entry->index = object;
    entry->object.context = entry;
    entry->script = (AbBlockCollisionScript){entry, object_script};
    entry->definition.context = entry;
    entry->dead_value = (AbDataValue){.kind = AB_DATA_TABLE, .table = physical(entry)};
    ++owner->count;
    if (owner->config.script_objects)
        owner->config.script_object_count = owner->count;
    return true;
}

void ab_classic_block_collisions_close(AbClassicBlockCollisions *owner) {
    if (owner) {
        free(owner->objects);
        free(owner);
    }
}

size_t ab_classic_block_collisions_bytes(const AbClassicBlockCollisions *owner) {
    return owner ? owner->bytes : 0;
}

bool ab_classic_block_collisions_call(AbClassicBlockCollisions *owner,
                                      const AbBlockCollisionInput *input,
                                      AbError *error) {
    if (!owner || !input || owner->calling || owner->contacting ||
        (input->first.size && !input->first.data) ||
        (input->second.size && !input->second.data))
        return ab_fail(error, "Invalid or reentrant owned block collision call");
    owner->active[0] = find(owner, input->first);
    owner->active[1] = find(owner, input->second);
    if (!owner->active[0] || !owner->active[1])
        return ab_fail(error, "Unknown initial owned block collision pair");
    owner->calling = true;
    owner->host.sound_force_threshold = *owner->config.sound_force_threshold;
    bool okay = ab_block_collision_callback(input, &owner->host, error);
    owner->calling = false;
    return okay;
}

static bool finite_number(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Non-numeric canonical block damage property");
    float result;
    memcpy(&result, &value.bits, sizeof(result));
    if (!isfinite(result))
        return ab_fail(error, "Nonfinite canonical block damage property");
    *output = result;
    return true;
}

static bool script_flag(CollisionObject *object, const char *key, bool *output,
                        AbError *error) {
    AbClassicBlockCollisions *owner = object->owner;
    AbDataValue script = field(physical(object), "gameObjectScript");
    if (script.kind != AB_DATA_TABLE ||
        script.table != &owner->level.properties->objects[object->index].script)
        return ab_fail(error, "Missing canonical block damage script getter");
    AbDataValue value;
    if (!ab_level_property_get(owner->level.properties, object->index, span(key),
                               &value, error))
        return false;
    *output = truthy(value);
    return true;
}

static bool damage_body(CollisionObject *object, AbLegacyCollisionBody *output,
                        AbError *error) {
    AbDataTable *source = physical(object);
    AbLegacyCollisionBody body = {0};
    AbDataValue strength = field(source, "strength"),
                defence = field(source, "defence");
    body.has_strength = truthy(strength);
    body.has_defence = truthy(defence);
    body.ground_or_static = same_span(object->object.name, span("ground")) ||
                            (object->resolved.collider.kind == AB_DATA_NUMBER &&
                             object->resolved.collider.bits == UINT32_C(0x40000000));
    if (!finite_number(field(source, "mass"), &body.mass, error) ||
        !finite_number(field(source, "xVel"), &body.velocity.x, error) ||
        !finite_number(field(source, "yVel"), &body.velocity.y, error) ||
        (body.has_strength && !finite_number(strength, &body.strength, error)) ||
        (body.has_defence && !finite_number(defence, &body.defence, error)) ||
        !script_flag(object, "ignoreGroundDamage", &body.ignore_ground_damage, error) ||
        !script_flag(object, "noCollisionDamage", &body.no_collision_damage, error) ||
        !script_flag(object, "ignoreAllDamage", &body.ignore_all_damage, error))
        return false;
    *output = body;
    return true;
}

static bool apply_strength(CollisionObject *object, const AbLegacyCollisionBody *body,
                           float remaining, bool dead, AbError *error) {
    AbClassicBlockCollisions *owner = object->owner;
    AbObjectPropertyAccess access = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = physical(object)}};
    if (body->has_strength &&
        !same_value(field(physical(object), "strength"), number(remaining))) {
        if (!ab_object_property_set(&access, span("strength"), number(remaining),
                                    error))
            return false;
        owner->level.character_objects[object->index].goal.strength = remaining;
    }
    if (dead) {
        if (!owner->config.dead_blocks)
            return ab_fail(error, "Missing owned block damage removal map");
        if (!ab_string_map_set(owner->config.dead_blocks, object->object.name,
                               (uintptr_t)&object->dead_value, true, error))
            return false;
    }
    return true;
}

static bool break_joint(void *context, AbSpan name, AbError *error) {
    AbClassicBlockCollisions *owner = context;
    if (!owner->config.break_joint)
        return ab_fail(error, "Missing owned deferred joint-break service");
    return owner->config.break_joint(owner->config.context, name, error);
}

static bool contact_damage(AbClassicBlockCollisions *owner, AbGameContactCall *call,
                           AbBlockCollisionInput *callback, AbError *error) {
    AbSpan names[] = {call->first, call->second};
    CollisionObject *objects[2];
    AbLegacyCollisionBody bodies[2];
    for (size_t side = 0; side < 2; ++side) {
        AbBlockCollisionObject *view;
        AbBlockCollisionDefinition *selected;
        if (!lookup(owner, names[side], &view, error) ||
            !definition(owner, names[side], &selected, error))
            return false;
        objects[side] = view->context;
        float collider = 0;
        if (!finite_number(objects[side]->resolved.collider, &collider, error) ||
            collider < 0 || collider >= (float)AB_GAME_COLLIDER_TYPES ||
            floorf(collider) != collider)
            return ab_fail(error, "Invalid block damage definition collider");
        if (collider == (float)AB_COLLIDER_BIRD ||
            collider == (float)AB_COLLIDER_EAGLE || collider == (float)AB_COLLIDER_GLUE)
            return ab_fail(error,
                           "Block damage requires the enclosing bird/glue route");
        if (!damage_body(objects[side], bodies + side, error))
            return false;
    }
    AbLegacyBlockDamage damage;
    if (!ab_legacy_block_damage(bodies, &damage))
        return ab_fail(error, "Unsupported owned block contact damage");
    for (size_t side = 0; side < 2; ++side) {
        if (!apply_strength(objects[side], bodies + side,
                            damage.remaining_strength[side], damage.mark_dead[side],
                            error))
            return false;
    }
    for (size_t side = 0; side < 2; ++side) {
        if (!ab_classic_joints_break(owner->level.joints, names[side], damage.force,
                                     break_joint, owner, error))
            return false;
    }
    *callback = (AbBlockCollisionInput){.first = call->first,
                                        .second = call->second,
                                        .force = damage.force,
                                        .damage = damage.callback_damage,
                                        .normal = call->normal,
                                        .point = call->point};
    return true;
}

bool ab_classic_block_collisions_contact(void *context, AbGameContactCall *call,
                                         AbError *error) {
    AbClassicBlockCollisions *owner = context;
    if (!owner || !call || owner->calling || owner->contacting ||
        (call->first.size && !call->first.data) ||
        (call->second.size && !call->second.data))
        return ab_fail(error, "Invalid or reentrant owned block contact");
    owner->contacting = true;
    AbBlockCollisionInput callback;
    bool okay = contact_damage(owner, call, &callback, error);
    owner->contacting = false;
    return okay && ab_classic_block_collisions_call(owner, &callback, error);
}
