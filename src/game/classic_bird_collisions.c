#include "ab/classic_bird_collisions.h"

#include "ab/score.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    size_t offset;
} PhysicalField;

/* Only fields written by recovered phases are published back. Read-only fields
 * use the same table so callback refresh cannot leave a stale typed view. */
#define BIRD_FIELD(name, member) {name, offsetof(AbBirdCollisionObject, member)}
static const PhysicalField fields[] = {
    BIRD_FIELD("sensor", object.sensor),
    BIRD_FIELD("damageSprite", object.damage_sprite),
    BIRD_FIELD("blinkSprite", object.blink_sprite),
    BIRD_FIELD("smileSprite", object.smile_sprite),
    BIRD_FIELD("projectileFeather", projectile_feather),
    BIRD_FIELD("controllable", controllable),
    BIRD_FIELD("shot", shot),
    BIRD_FIELD("hasCollided", has_collided),
    BIRD_FIELD("defence", defence),
    BIRD_FIELD("strength", strength),
    BIRD_FIELD("isMightyBait", mighty_bait),
    BIRD_FIELD("isMightyEagle", mighty_eagle),
    BIRD_FIELD("sprite", sprite),
    BIRD_FIELD("dontUpdateDamageSprite", dont_update_damage_sprite),
    BIRD_FIELD("frozenSolid", frozen_solid),
    BIRD_FIELD("collision", collision),
    BIRD_FIELD("inBubble", in_bubble),
    BIRD_FIELD("bombTimer", bomb_timer),
    BIRD_FIELD("pinkTimer", pink_timer),
    BIRD_FIELD("boomerangActive", boomerang_active),
    BIRD_FIELD("explode", explode),
};
#undef BIRD_FIELD

enum { FIELD_COUNT = sizeof(fields) / sizeof(fields[0]) };

typedef struct {
    AbClassicBirdCollisions *owner;
    size_t index;
    AbBirdCollisionObject object;
    AbBirdCollisionScript script;
    AbBirdCollisionEffectBird effect;
    AbBirdCollisionParticleQuery particle_query;
    AbDataValue published[FIELD_COUNT];
    bool loaded, bird;
} BirdObject;

typedef struct {
    AbBlockCollisionDefinition notification;
    AbBirdCollisionSpriteDefinition sprites;
    AbBirdCollisionEffectDefinition effects;
    AbObjectDefinition resolved;
    bool loaded;
} BirdDefinition;

struct AbClassicBirdCollisions {
    AbClassicBirdCollisionConfig config;
    AbClassicLevelView level;
    const AbDataTable *blocks;
    AbDataTable block_table;
    AbDataEntry blocks_entry;
    BirdObject *objects;
    BirdDefinition *definitions;
    size_t count, capacity, definition_count, bytes;
    AbBirdCollisionPreludeState prelude;
    AbBirdCollisionSpriteState sprites;
    AbDataValue published_specialty;
    size_t published_score_count;
    AbDamageSpriteNames damage_names;
    AbBirdCollisionNotifyHost notifications;
    AbBirdCollisionPreludeHost prelude_host;
    AbBirdCollisionSpriteHost sprite_host;
    AbBirdCollisionSpecialtyHost specialty_host;
    AbBirdCollisionProjectileHost projectile_host;
    AbBirdCollisionEffectHost effect_host;
    AbBirdCollisionDamageHost damage_host;
    AbBirdCollisionHost host;
    bool calling;
};

static bool achievement(void *context, AbBirdCollisionObject *first,
                        AbBirdCollisionObject *second, AbError *error);

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue text(AbSpan name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = name};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(span(name)));
    return value ? *value : (AbDataValue){0};
}

static bool valid_table(const AbDataTable *table) {
    return table && table->count <= table->capacity &&
           (!table->count || table->entries);
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
        case AB_DATA_NUMBER:
            return first.bits == second.bits;
        case AB_DATA_BOOLEAN:
            return !first.bits == !second.bits;
        case AB_DATA_STRING:
            return same_span(first.string, second.string);
        case AB_DATA_TABLE:
            return first.table == second.table;
        case AB_DATA_OBJECT:
            return first.object == second.object;
    }
    return false;
}

static AbDataValue *object_field(BirdObject *object, size_t index) {
    return (AbDataValue *)((uint8_t *)&object->object + fields[index].offset);
}

static AbDataTable *physical(BirdObject *object) {
    return &object->owner->level.properties->objects[object->index].physical;
}

static BirdObject *find(AbClassicBirdCollisions *owner, AbSpan name) {
    for (size_t index = 0; index < owner->count; ++index) {
        if (same_span(name, owner->level.bodies->objects[index].name))
            return owner->objects + index;
    }
    return NULL;
}

static float position_number(AbDataValue value) {
    float result = NAN;
    if (value.kind == AB_DATA_NUMBER)
        memcpy(&result, &value.bits, sizeof(result));
    return result;
}

static bool refresh_object(BirdObject *object, AbError *error) {
    AbDataTable *source = physical(object);
    if (!valid_table(source))
        return ab_fail(error, "Invalid canonical bird collision table");
    AbDataValue name = field(source, "name");
    if (name.kind != AB_DATA_STRING ||
        !same_span(name.string,
                   object->owner->level.bodies->objects[object->index].name))
        return ab_fail(error, "Invalid or renamed canonical bird collision object");
    object->object.object.name = name.string;
    for (size_t index = 0; index < FIELD_COUNT; ++index)
        object->published[index] = *object_field(object, index) =
            field(source, fields[index].name);
    object->object.position = (AbVec2){position_number(field(source, "x")),
                                       position_number(field(source, "y"))};
    bool has_script = truthy(field(source, "gameObjectScript"));
    object->object.script = has_script ? &object->script : NULL;
    object->effect = (AbBirdCollisionEffectBird){
        field(source, "xVel"), field(source, "yVel"), field(source, "mass"),
        has_script ? &object->particle_query : NULL};
    object->loaded = true;
    return true;
}

static bool refresh(AbClassicBirdCollisions *owner, AbError *error) {
    for (size_t index = 0; index < owner->count; ++index) {
        if (owner->objects[index].loaded &&
            !refresh_object(owner->objects + index, error))
            return false;
    }
    AbBirdCollisionGrenades *grenades = owner->config.grenades;
    if (grenades->present && grenades->count <= grenades->capacity &&
        grenades->count <= UINT32_C(0x1000000) &&
        (!grenades->count || grenades->items)) {
        for (size_t item = 0; item < grenades->count; ++item) {
            size_t index = 0;
            while (index < owner->count &&
                   &owner->objects[index].object != grenades->items[item])
                ++index;
            if (index == owner->count) {
                /* Actual flyingGrenades entries are separate {name,timer}
                 * records. Contact marks their explode field, not the physical
                 * object's identically spelled property. */
                AbBirdCollisionObject *record = grenades->items[item];
                if (!record || !find(owner, record->object.name))
                    return ab_fail(error, "Unknown owned flying-grenade name");
                continue;
            }
            if (!owner->objects[index].loaded &&
                !refresh_object(owner->objects + index, error))
                return false;
        }
    }
    owner->published_specialty = owner->sprites.bird_specialty_available =
        *owner->config.specialty_available;
    owner->sprites.flying_bird = NULL;
    AbDataValue flying = *owner->config.flying_object;
    if (truthy(flying)) {
        if (flying.kind != AB_DATA_TABLE)
            return ab_fail(error, "Invalid shared collision flying object");
        size_t index = 0;
        while (index < owner->count && physical(owner->objects + index) != flying.table)
            ++index;
        if (index == owner->count)
            return ab_fail(error, "Unknown shared collision flying object");
        BirdObject *object = owner->objects + index;
        if (!object->loaded && !refresh_object(object, error))
            return false;
        owner->sprites.flying_bird = &object->object;
    }
    owner->prelude.delta_based_slingshot = *owner->config.delta_based_slingshot;
    owner->prelude.floating_score_scaling = *owner->config.floating_score_scaling;
    owner->prelude.collision_scores = owner->config.scores->entries;
    owner->published_score_count = owner->prelude.count = owner->config.scores->count;
    owner->prelude.capacity = owner->config.scores->capacity;
    owner->effect_host.threshold = *owner->config.sound_force_threshold;
    owner->prelude_host.achievement =
        truthy(*owner->config.achievement_processor) ? achievement : NULL;
    return true;
}

static bool publish(AbClassicBirdCollisions *owner, AbError *error) {
    for (size_t row = 0; row < owner->count; ++row) {
        BirdObject *object = owner->objects + row;
        if (!object->loaded)
            continue;
        AbObjectPropertyAccess access = {
            .physics_object = {.kind = AB_DATA_TABLE, .table = physical(object)}};
        for (size_t index = 0; index < FIELD_COUNT; ++index) {
            AbDataValue value = *object_field(object, index);
            if (same_value(value, object->published[index]))
                continue;
            if (!ab_object_property_set(&access, span(fields[index].name), value,
                                        error))
                return false;
            object->published[index] = value;
            if (!strcmp(fields[index].name, "strength") && value.kind == AB_DATA_NUMBER)
                memcpy(&owner->level.character_objects[row].goal.strength, &value.bits,
                       sizeof(float));
        }
    }
    if (!same_value(owner->published_specialty,
                    owner->sprites.bird_specialty_available)) {
        *owner->config.specialty_available = owner->sprites.bird_specialty_available;
        owner->published_specialty = owner->sprites.bird_specialty_available;
    }
    if (owner->published_score_count != owner->prelude.count) {
        owner->config.scores->count = owner->prelude.count;
        owner->published_score_count = owner->prelude.count;
    }
    return true;
}

static bool after_service(AbClassicBirdCollisions *owner, bool okay, AbError *error) {
    return okay && refresh(owner, error);
}

static bool lookup(void *context, AbSpan name, AbBirdCollisionObject **output,
                   AbError *error) {
    AbClassicBirdCollisions *owner = context;
    if (!publish(owner, error))
        return false;
    BirdObject *object = find(owner, name);
    if (!object)
        return ab_fail(error, "Unknown initial bird collision object");
    AbDataValue current;
    if (!ab_object_world_lookup(&owner->level.properties->lookup, text(name), &current,
                                error))
        return false;
    if (current.kind == AB_DATA_NIL) {
        *output = NULL;
        return true;
    }
    if (current.kind != AB_DATA_TABLE || current.table != physical(object))
        return ab_fail(error, "Unsupported replaced bird collision world object");
    if (!refresh_object(object, error))
        return false;
    *output = &object->object;
    return true;
}

static bool resolve_definition(AbClassicBirdCollisions *owner, AbSpan name,
                               BirdDefinition **output, AbError *error) {
    if (!publish(owner, error))
        return false;
    BirdObject *object = find(owner, name);
    AbDataValue raw;
    if (!object || !ab_object_definition_lookup(&owner->level.properties->lookup, name,
                                                &raw, error))
        return false;
    AbDataValue identifier = field(physical(object), "definition");
    if (identifier.kind != AB_DATA_STRING || raw.kind != AB_DATA_TABLE)
        return ab_fail(error, "Invalid live bird collision definition");
    size_t index = 0;
    while (index < owner->definition_count &&
           !same_value(owner->blocks->entries[index].key, identifier))
        ++index;
    if (index == owner->definition_count ||
        owner->blocks->entries[index].value.kind != AB_DATA_TABLE ||
        owner->blocks->entries[index].value.table != raw.table)
        return ab_fail(error, "Unsupported rebound bird collision catalog");
    BirdDefinition *definition = owner->definitions + index;
    if (!definition->loaded) {
        if (!ab_object_definition_open(owner->level.catalog, identifier.string, NULL,
                                       &definition->resolved, error))
            return false;
        const AbObjectDefinition *resolved = &definition->resolved;
        definition->notification =
            (AbBlockCollisionDefinition){field(raw.table, "onCollision"), definition};
        definition->sprites = (AbBirdCollisionSpriteDefinition){
            ab_object_property(resolved, span("specialty")),
            ab_object_property(resolved, span("spriteCollision")),
            ab_object_property(resolved, span("onFlyingBirdCollided")),
            ab_object_property(resolved, span("specialtyAvailableAfterCollision")),
            ab_object_property(resolved, span("explosionTimer")),
            ab_object_property(resolved, span("spriteSpecial1"))};
        definition->effects = (AbBirdCollisionEffectDefinition){
            ab_object_property(resolved, span("materialName")),
            ab_object_property(resolved, span("collisionSound")),
            ab_object_property(resolved, span("damageSound"))};
        definition->loaded = true;
    }
    *output = definition;
    return true;
}

static bool definition(void *context, AbSpan name, AbBlockCollisionDefinition **output,
                       AbError *error) {
    BirdDefinition *resolved;
    if (!resolve_definition(context, name, &resolved, error))
        return false;
    *output = &resolved->notification;
    return true;
}

static bool sprite_definition(void *context, AbSpan name,
                              AbBirdCollisionSpriteDefinition **output,
                              AbError *error) {
    BirdDefinition *resolved;
    if (!resolve_definition(context, name, &resolved, error))
        return false;
    *output = &resolved->sprites;
    return true;
}

static AbBirdCollisionEffectDefinition *
effect_definition(void *context, AbBlockCollisionDefinition *cached) {
    (void)context;
    BirdDefinition *definition = cached->context;
    return &definition->effects;
}

static bool level_hook(void *context, const AbBlockCollisionNotice *notice,
                       AbError *error) {
    AbClassicBirdCollisions *owner = context;
    BirdObject *first = notice->first->context, *second = notice->second->context;
    return publish(owner, error) &&
           after_service(owner,
                         owner->config.level_hook(owner->config.context, first->index,
                                                  second->index, notice, error),
                         error);
}

static bool named(void *context, AbDataValue name, const AbBlockCollisionNotice *notice,
                  AbError *error) {
    AbClassicBirdCollisions *owner = context;
    BirdObject *first = notice->first->context, *second = notice->second->context;
    if (!publish(owner, error))
        return false;
    if (!owner->config.named)
        return ab_fail(error, "Missing required owned named bird collision service");
    return after_service(owner,
                         owner->config.named(owner->config.context, name, first->index,
                                             second->index, notice, error),
                         error);
}

static bool raw_definition(BirdObject *object, const AbDataTable **output,
                           AbError *error) {
    AbDataValue identifier = field(physical(object), "definition");
    const AbDataValue *value = ab_data_get(object->owner->blocks, identifier);
    if (!value || value->kind == AB_DATA_NIL) {
        *output = NULL;
        return true;
    }
    if (value->kind != AB_DATA_TABLE || !valid_table(value->table))
        return ab_fail(error, "Invalid raw bird collision definition");
    *output = value->table;
    return true;
}

static bool event_definition(void *context, AbBlockEventObject *event_object,
                             const AbDataTable **output, AbError *error) {
    AbClassicBirdCollisions *owner = context;
    BirdObject *object = find(owner, event_object->name);
    if (!object)
        return ab_fail(error, "Unknown owned bird block-event object");
    AbDataValue identifier = field(physical(object), "definition");
    if (identifier.kind != AB_DATA_STRING)
        return ab_fail(error, "Invalid bird block-event definition name");
    event_object->definition = identifier.string;
    return raw_definition(object, output, error);
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static bool scripts(void *context, const AbBlockCollisionNotice *notice,
                    AbError *error) {
    AbClassicBirdCollisions *owner = context;
    BirdObject *object = notice->first->context, *other = notice->second->context;
    if (!publish(owner, error))
        return false;
    if (!owner->config.script_objects)
        return ab_fail(error, "Missing owned bird collision block-script attachments");
    AbBlockEventObject *attachment = owner->config.script_objects + object->index;
    if (!same_span(attachment->name, object->object.object.name))
        return ab_fail(error, "Mismatched owned bird collision attachment");
    AbDataValue arguments[] = {{.kind = AB_DATA_TABLE, .table = physical(other)},
                               number(notice->force),
                               number(notice->damage),
                               number(notice->normal.x),
                               number(notice->normal.y)};
    return after_service(
        owner,
        ab_block_event(
            attachment, span("onCollision"), arguments, 5,
            (AbBlockEventHost){.context = owner, .definition = event_definition},
            error),
        error);
}

static bool valid_script(BirdObject *object, AbError *error) {
    AbDataValue script = field(physical(object), "gameObjectScript");
    return (script.kind == AB_DATA_TABLE &&
            script.table ==
                &object->owner->level.properties->objects[object->index].script) ||
           ab_fail(error, "Unsupported owned bird collision script receiver");
}

static bool object_script(void *context, const AbBlockCollisionNotice *notice,
                          AbError *error) {
    BirdObject *receiver = context;
    AbClassicBirdCollisions *owner = receiver->owner;
    BirdObject *first = notice->first->context, *second = notice->second->context;
    return publish(owner, error) && valid_script(receiver, error) &&
           after_service(owner,
                         ab_classic_behaviors_bird_collision(
                             owner->config.behaviors, receiver->index, first->index,
                             second->index, notice, error),
                         error);
}

static bool hat(void *context, AbSpan name, AbBirdCollisionHat **output,
                AbError *error) {
    AbClassicBirdCollisions *owner = context;
    if (!publish(owner, error))
        return false;
    uintptr_t value = 0;
    bool present = false;
    if (!ab_string_map_get(owner->config.hats, name, &value, &present, error))
        return false;
    if (present && !value)
        return ab_fail(error, "Invalid live bird hat registry handle");
    *output = present ? (AbBirdCollisionHat *)value : NULL;
    return true;
}

static bool score_entry(void *context, AbSpan name, AbBirdCollisionScoreEntry **output,
                        AbError *error) {
    AbClassicBirdCollisions *owner = context;
    if (!publish(owner, error))
        return false;
    BirdObject *object = find(owner, name);
    if (!object)
        return ab_fail(error, "Unknown incoming bird collision score key");
    if (owner->config.root_scores)
        return ab_classic_scores_entry(owner->config.root_scores, name, output, error);
    *output = owner->config.bird_scores + object->index;
    return true;
}

static bool score(void *context, float damage, float *output, AbError *error) {
    AbClassicBirdCollisions *owner = context;
    return publish(owner, error) &&
           (ab_damage_score(damage, output) ||
            ab_fail(error, "Invalid owned bird collision damage score"));
}

static bool floating(void *context, const AbBirdCollisionFloatingScore *request,
                     AbError *error) {
    AbClassicBirdCollisions *owner = context;
    return publish(owner, error) &&
           after_service(
               owner,
               ab_floating_scores_add(owner->config.floating_scores, request, error),
               error);
}

static bool cooldown(void *context, AbError *error) {
    AbClassicBirdCollisions *owner = context;
    if (!publish(owner, error))
        return false;
    if (!owner->config.cooldown)
        return ab_fail(error, "Missing owned bird collision sling cooldown service");
    return after_service(owner, owner->config.cooldown(owner->config.context, error),
                         error);
}

static bool achievement(void *context, AbBirdCollisionObject *first,
                        AbBirdCollisionObject *second, AbError *error) {
    AbClassicBirdCollisions *owner = context;
    BirdObject *first_row = first->object.context, *second_row = second->object.context;
    if (!publish(owner, error))
        return false;
    if (!owner->config.achievement)
        return ab_fail(error, "Missing owned bird collision achievement service");
    return after_service(owner,
                         owner->config.achievement(owner->config.context,
                                                   first_row->index, second_row->index,
                                                   error),
                         error);
}

static bool mighty(void *context, AbSpan first, AbSpan second, AbError *error) {
    BirdObject *object = context;
    AbClassicBirdCollisions *owner = object->owner;
    if (!publish(owner, error))
        return false;
    if (!owner->config.mighty)
        return ab_fail(error, "Missing owned mighty collision function");
    return after_service(owner,
                         owner->config.mighty(owner->config.context, object->index,
                                              first, second, error),
                         error);
}

static bool sprite(void *context, AbSpan name, AbDataValue value, AbError *error) {
    AbClassicBirdCollisions *owner = context;
    if (!publish(owner, error))
        return false;
    BirdObject *object = find(owner, name);
    if (!object || value.kind != AB_DATA_STRING)
        return ab_fail(error, "Unsupported owned bird collision sprite value");
    return after_service(owner,
                         ab_classic_visuals_set_sprite(
                             owner->config.visuals, object->index, value.string, error),
                         error);
}

static bool print(void *context, AbUiPointerValue value, AbError *error) {
    AbClassicBirdCollisions *owner = context;
    if (!publish(owner, error))
        return false;
    if (!owner->config.print)
        return ab_fail(error, "Missing owned bird collision print service");
    return after_service(
        owner, owner->config.print(owner->config.context, value, error), error);
}

static bool binding(void *context, AbDataValue key, AbUiPointerValue *output,
                    AbError *error) {
    AbClassicBirdCollisions *owner = context;
    if (!publish(owner, error))
        return false;
    if (!owner->config.binding)
        return ab_fail(error, "Missing owned bird collision specialty binding");
    return after_service(
        owner, owner->config.binding(owner->config.context, key, output, error), error);
}

static bool specialty(void *context, AbUiPointerValue function,
                      AbBirdCollisionObject *flying,
                      const AbBirdCollisionNamedInput *contact, AbError *error) {
    AbClassicBirdCollisions *owner = context;
    if (!publish(owner, error))
        return false;
    if (!owner->config.specialty)
        return ab_fail(error, "Missing owned bird collision specialty function");
    BirdObject *object = flying ? flying->object.context : NULL;
    return after_service(owner,
                         owner->config.specialty(owner->config.context, function,
                                                 object ? &object->index : NULL,
                                                 contact, error),
                         error);
}

static bool release_hat(void *context, AbSpan name, AbError *error) {
    AbClassicBirdCollisions *owner = context;
    if (!publish(owner, error))
        return false;
    if (!owner->config.release_hat)
        return ab_fail(error, "Missing owned bird collision hat-release service");
    return after_service(
        owner, owner->config.release_hat(owner->config.context, name, error), error);
}

static bool particle_query(void *context, AbDataValue *output, AbError *error) {
    BirdObject *object = context;
    AbClassicBirdCollisions *owner = object->owner;
    return publish(owner, error) && valid_script(object, error) &&
           after_service(owner,
                         ab_level_property_get(owner->level.properties, object->index,
                                               span("particles"), output, error),
                         error);
}

static bool effect_bird(void *context, AbSpan name, AbBirdCollisionEffectBird **output,
                        AbError *error) {
    AbClassicBirdCollisions *owner = context;
    if (!publish(owner, error))
        return false;
    BirdObject *object = find(owner, name);
    if (!object || !object->bird) {
        *output = NULL;
        return true;
    }
    if (!refresh_object(object, error))
        return false;
    *output = &object->effect;
    return true;
}

static bool audio_name(void *context, AbDataValue sound, AbDataValue *output,
                       AbError *error) {
    AbClassicBirdCollisions *owner = context;
    return publish(owner, error) &&
           after_service(
               owner,
               ab_audio_bank_name_value(owner->config.audio, sound, output, error),
               error);
}

static bool audio(void *context, AbAudioPlay request, AbError *error) {
    AbClassicBirdCollisions *owner = context;
    uint32_t handle;
    return publish(owner, error) &&
           after_service(
               owner, ab_audio_bank_play(owner->config.audio, request, &handle, error),
               error);
}

static bool contact_particles(void *context, AbSpan name, AbDataValue descriptor,
                              AbError *error) {
    AbClassicBirdCollisions *owner = context;
    if (!publish(owner, error))
        return false;
    BirdObject *object = find(owner, name);
    if (!object || !owner->config.contact_particles)
        return ab_fail(error, "Missing owned bird contact-particle service");
    return after_service(owner,
                         owner->config.contact_particles(
                             owner->config.context, object->index, descriptor, error),
                         error);
}

static bool damage_sprites(void *context, AbBirdCollisionObject *view,
                           AbDataValue blocks, AbDamageSpriteNames **output,
                           AbError *error) {
    AbClassicBirdCollisions *owner = context;
    BirdObject *object = view->object.context;
    if (!publish(owner, error))
        return false;
    if (blocks.kind != AB_DATA_TABLE || blocks.table != owner->blocks)
        return ab_fail(error, "Unsupported bird damage catalog binding");
    if (!after_service(owner,
                       ab_classic_visuals_update_damage(
                           owner->config.visuals, object->index,
                           owner->config.sprite_workspace, &owner->damage_names, error),
                       error))
        return false;
    *output = &owner->damage_names;
    return true;
}

static bool damage_particles(void *context, AbBirdCollisionObject *view,
                             AbError *error) {
    AbClassicBirdCollisions *owner = context;
    BirdObject *object = view->object.context;
    const AbDataTable *raw;
    if (!publish(owner, error) || !raw_definition(object, &raw, error))
        return false;
    if (!raw)
        return ab_fail(error, "Missing raw bird damage-particle definition");
    const AbVec2 *position =
        field(raw, "damageStateChangedParticles").kind == AB_DATA_NIL
            ? NULL
            : &object->object.position;
    return after_service(
        owner,
        ab_damage_state_particles(raw, position, owner->config.damage_particles, error),
        error);
}

bool ab_classic_bird_collisions_open(AbClassicBirdCollisionConfig config,
                                     AbClassicBirdCollisions **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing owned bird collision output");
    *output = NULL;
    AbClassicLevelView level = ab_classic_level_view(config.level);
    AbDataValue blocks = field(level.catalog, "blocks");
    if (!level.bodies || !level.properties || !level.catalog ||
        level.bodies->count != level.properties->count ||
        ab_classic_visuals_view(config.visuals).level != config.level ||
        blocks.kind != AB_DATA_TABLE || !valid_table(blocks.table) || !config.hats ||
        !config.grenades || !config.bird_scores || !config.scores ||
        config.bird_score_count != level.bodies->count || !config.flying_object ||
        !config.specialty_available || !config.delta_based_slingshot ||
        !config.achievement_processor || !config.floating_score_scaling ||
        !config.sound_force_threshold ||
        (config.script_objects && config.script_object_count != level.bodies->count) ||
        (!config.script_objects && config.script_object_count) ||
        config.storage_bytes < sizeof(AbClassicBirdCollisions))
        return ab_fail(error, "Invalid owned bird collision binding or byte limit");
    size_t remaining = config.storage_bytes - sizeof(AbClassicBirdCollisions);
    size_t capacity =
        config.object_capacity ? config.object_capacity : level.bodies->count;
    if (capacity < level.bodies->count || capacity > level.bodies->capacity ||
        capacity > remaining / sizeof(BirdObject))
        return ab_fail(error, "Owned bird collision rows exceed byte limit");
    remaining -= capacity * sizeof(BirdObject);
    if (blocks.table->count > remaining / sizeof(BirdDefinition))
        return ab_fail(error, "Owned bird collision definitions exceed byte limit");
    if (config.root_scores) {
        AbClassicScoreView view = ab_classic_scores_view(config.root_scores);
        if (view.blocks != config.scores || view.objects != config.bird_scores ||
            view.count != config.bird_score_count)
            return ab_fail(error, "Mismatched owned root/bird score bindings");
    }
    AbClassicBirdCollisions *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate owned bird collisions");
    owner->config = config;
    owner->level = level;
    owner->blocks = blocks.table;
    owner->count = level.bodies->count;
    owner->capacity = capacity;
    owner->definition_count = blocks.table->count;
    owner->bytes = sizeof(*owner) + capacity * sizeof(BirdObject) +
                   owner->definition_count * sizeof(BirdDefinition);
    owner->objects = calloc(capacity, sizeof(*owner->objects));
    owner->definitions = calloc(owner->definition_count, sizeof(*owner->definitions));
    if ((owner->count && !owner->objects) ||
        (owner->definition_count && !owner->definitions)) {
        ab_classic_bird_collisions_close(owner);
        return ab_fail(error, "Cannot allocate owned bird collision rows");
    }
    for (size_t index = 0; index < owner->count; ++index) {
        BirdObject *object = owner->objects + index;
        object->owner = owner;
        object->index = index;
        object->object.object.context = object;
        object->object.collision_function = (AbBirdCollisionFunction){object, mighty};
        object->script = (AbBirdCollisionScript){object, object_script};
        object->particle_query = (AbBirdCollisionParticleQuery){object, particle_query};
    }
    for (size_t index = 0; index < level.bird_count; ++index)
        owner->objects[level.bird_order[index]].bird = true;
    owner->notifications = (AbBirdCollisionNotifyHost){
        owner,      lookup, config.level_hook ? level_hook : NULL,
        definition, named,  scripts};
    owner->prelude_host = (AbBirdCollisionPreludeHost){
        owner, hat, lookup, score_entry, score, floating, cooldown, achievement};
    owner->sprite_host =
        (AbBirdCollisionSpriteHost){owner, sprite_definition, lookup, sprite};
    owner->specialty_host = (AbBirdCollisionSpecialtyHost){
        owner, sprite_definition, lookup, binding, print, specialty, sprite,
        hat,   release_hat};
    owner->projectile_host = (AbBirdCollisionProjectileHost){owner, sprite_definition};
    owner->effect_host = (AbBirdCollisionEffectHost){.context = owner,
                                                     .bird = effect_bird,
                                                     .audio_name = audio_name,
                                                     .audio = audio,
                                                     .particles = contact_particles};
    owner->blocks_entry = (AbDataEntry){text(span("blocks")), blocks};
    owner->block_table = (AbDataTable){&owner->blocks_entry, 1, 1, NULL};
    owner->damage_host = (AbBirdCollisionDamageHost){
        owner,
        {.kind = AB_DATA_TABLE, .table = &owner->block_table},
        damage_sprites,
        damage_particles};
    owner->host = (AbBirdCollisionHost){&owner->notifications,   &owner->prelude_host,
                                        &owner->sprite_host,     &owner->specialty_host,
                                        &owner->projectile_host, &owner->effect_host,
                                        &owner->damage_host,     owner,
                                        effect_definition};
    *output = owner;
    return true;
}

bool ab_classic_bird_collisions_register_object(AbClassicBirdCollisions *owner,
                                                size_t object, AbError *error) {
    if (!owner || owner->calling || object != owner->count ||
        object >= owner->capacity || object >= owner->level.bodies->count)
        return ab_fail(error, "Invalid new bird-collision row");
    BirdObject *entry = owner->objects + object;
    entry->owner = owner;
    entry->index = object;
    entry->object.object.context = entry;
    entry->object.collision_function = (AbBirdCollisionFunction){entry, mighty};
    entry->script = (AbBirdCollisionScript){entry, object_script};
    entry->particle_query = (AbBirdCollisionParticleQuery){entry, particle_query};
    AbClassicLevelView level = ab_classic_level_view(owner->config.level);
    for (size_t bird = 0; bird < level.bird_count; ++bird)
        if (level.bird_order[bird] == object)
            entry->bird = true;
    ++owner->count;
    owner->config.bird_score_count = owner->count;
    if (owner->config.script_objects)
        owner->config.script_object_count = owner->count;
    return true;
}

void ab_classic_bird_collisions_close(AbClassicBirdCollisions *owner) {
    if (owner) {
        free(owner->definitions);
        free(owner->objects);
        free(owner);
    }
}

size_t ab_classic_bird_collisions_bytes(const AbClassicBirdCollisions *owner) {
    return owner ? owner->bytes : 0;
}

AbBirdCollisionObject *ab_classic_bird_collisions_object(AbClassicBirdCollisions *owner,
                                                         size_t object) {
    return owner && object < owner->count ? &owner->objects[object].object : NULL;
}

bool ab_classic_bird_collisions_call(void *context,
                                     const AbBirdCollisionNotifyInput *input,
                                     AbError *error) {
    AbClassicBirdCollisions *owner = context;
    if (!owner || !input || owner->calling)
        return ab_fail(error, "Invalid or reentrant owned bird collision call");
    owner->calling = true;
    for (size_t index = 0; index < owner->count; ++index)
        owner->objects[index].loaded = false;
    /* Optional method presence can change between calls or during services. */
    owner->prelude_host.achievement = achievement;
    bool okay = refresh(owner, error) &&
                ab_bird_collision_callback(
                    input,
                    (AbBirdCollisionState){&owner->prelude, &owner->sprites,
                                           owner->config.grenades},
                    &owner->host, error);
    AbError publication_error = {0};
    bool published = publish(owner, okay ? error : &publication_error);
    owner->calling = false;
    return okay && published;
}
