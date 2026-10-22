#include "ab/bird_collision_sprites.h"

#include <string.h>

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool grenade(AbDataValue value) {
    return value.kind == AB_DATA_STRING && value.string.size == 7 &&
           value.string.data && !memcmp(value.string.data, "GRENADE", 7);
}

static bool apply(AbBirdCollisionObject *object, AbBirdCollisionObject *second,
                  AbBirdCollisionSpriteState *state,
                  const AbBirdCollisionSpriteHost *host, AbError *error) {
    if (truthy(second->object.sensor) || !truthy(object->shot)) {
        return true;
    }
    object->has_collided = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    if (!host || !host->definition) {
        return ab_fail(error, "Missing bird collision sprite-definition host");
    }
    AbBirdCollisionSpriteDefinition *definition = NULL;
    if (!host->definition(host->context, object->object.name, &definition, error)) {
        return false;
    }
    if (!definition) {
        return ab_fail(error, "Missing bird collision sprite definition");
    }
    if (definition->sprite_collision.kind == AB_DATA_NIL ||
        truthy(object->dont_update_damage_sprite)) {
        return true;
    }
    if (grenade(definition->specialty)) {
        if (!state) {
            return ab_fail(error, "Missing bird collision grenade state");
        }
        if (!truthy(state->bird_specialty_available) || state->flying_bird != object) {
            return true;
        }
    } else if (truthy(second->frozen_solid)) {
        return true;
    }
    if (!host->object) {
        return ab_fail(error, "Missing bird collision sprite world host");
    }
    AbBirdCollisionObject *world_object = NULL;
    if (!host->object(host->context, object->object.name, &world_object, error)) {
        return false;
    }
    if (!world_object) {
        return ab_fail(error, "Missing bird collision sprite world object");
    }
    world_object->sprite = definition->sprite_collision;
    if (!host->sprite) {
        return ab_fail(error, "Missing bird collision setSprite host");
    }
    AbSpan name = object->object.name;
    AbDataValue sprite = world_object->sprite;
    return host->sprite(host->context, name, sprite, error);
}

bool ab_bird_collision_sprites(AbBirdCollisionObject *first,
                               AbBirdCollisionObject *second,
                               AbBirdCollisionSpriteState *state,
                               const AbBirdCollisionSpriteHost *host, AbError *error) {
    if (!first || !second) {
        return ab_fail(error, "Missing bird collision sprite contact objects");
    }
    return apply(first, second, state, host, error) &&
           apply(second, second, state, host, error);
}
