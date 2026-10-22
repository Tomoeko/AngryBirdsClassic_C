#include "ab/bird_collision_specialty.h"

#include <string.h>

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool literal_true(AbDataValue value) {
    return value.kind == AB_DATA_BOOLEAN && value.bits != 0;
}

static bool named(AbSpan name, const char *text) {
    size_t length = strlen(text);
    return name.size == length &&
           (!length || (name.data && !memcmp(name.data, text, length)));
}

static bool same_name(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || (first.data && second.data &&
                            !memcmp(first.data, second.data, first.size)));
}

static bool specialty_is(AbDataValue value, const char *text) {
    return value.kind == AB_DATA_STRING && named(value.string, text);
}

static bool flying(AbBirdCollisionSpriteState *state, AbBirdCollisionObject **output,
                   AbError *error) {
    if (!state->flying_bird) {
        return ab_fail(error, "Missing current bird collision flying object");
    }
    *output = state->flying_bird;
    return true;
}

static bool world_flying(AbBirdCollisionSpriteState *state,
                         const AbBirdCollisionSpecialtyHost *host,
                         AbBirdCollisionObject **output, AbError *error) {
    AbBirdCollisionObject *current = NULL;
    if (!flying(state, &current, error)) {
        return false;
    }
    if (!host->object) {
        return ab_fail(error, "Missing bird specialty world-object host");
    }
    *output = NULL;
    if (!host->object(host->context, current->object.name, output, error)) {
        return false;
    }
    if (!*output) {
        return ab_fail(error, "Missing bird specialty world object");
    }
    return true;
}

static AbUiPointerValue printable(AbDataValue value) {
    switch (value.kind) {
        case AB_DATA_BOOLEAN:
            return (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN,
                                      .boolean = value.bits != 0};
        case AB_DATA_NUMBER:
            return (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER,
                                      .number_bits = value.bits};
        case AB_DATA_STRING:
            return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = value.string};
        case AB_DATA_TABLE:
            return (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT,
                                      .object = (uintptr_t)value.table};
        default:
            return (AbUiPointerValue){0};
    }
}

static bool named_collision(AbBirdCollisionSpriteState *state,
                            AbBirdCollisionSpriteDefinition *definition,
                            const AbBirdCollisionNamedInput *contact,
                            const AbBirdCollisionSpecialtyHost *host, AbError *error) {
    AbBirdCollisionObject *current = NULL;
    if (!flying(state, &current, error)) {
        return false;
    }
    if (truthy(current->collision) || !truthy(definition->on_flying_bird_collided)) {
        return true;
    }
    AbBirdCollisionPrint print = host->print;
    if (!print) {
        return ab_fail(error, "Missing bird collision diagnostic print host");
    }
    if (!print(host->context, printable(definition->on_flying_bird_collided), error)) {
        return false;
    }
    print = host->print;
    if (!host->binding) {
        return ab_fail(error, "Missing bird collision function-binding host");
    }
    AbUiPointerValue function = {0};
    if (!host->binding(host->context, definition->on_flying_bird_collided, &function,
                       error)) {
        return false;
    }
    if (!print) {
        return ab_fail(error, "Missing bird collision diagnostic print host");
    }
    if (!print(host->context, function, error)) {
        return false;
    }
    if (!host->binding) {
        return ab_fail(error, "Missing bird collision function-binding host");
    }
    function = (AbUiPointerValue){0};
    if (!host->binding(host->context, definition->on_flying_bird_collided, &function,
                       error)) {
        return false;
    }
    if (function.kind != AB_UI_POINTER_FUNCTION || !function.function) {
        return ab_fail(error, "Bird collision named binding is not callable");
    }
    if (!host->call) {
        return ab_fail(error, "Missing bird collision named-function host");
    }
    return host->call(host->context, function, state->flying_bird, contact, error);
}

static bool set_sprite(AbBirdCollisionSpriteState *state,
                       const AbBirdCollisionSpecialtyHost *host,
                       AbBirdCollisionObject *object, AbError *error) {
    if (!host->sprite) {
        return ab_fail(error, "Missing bird specialty setSprite host");
    }
    AbBirdCollisionObject *current = NULL;
    if (!flying(state, &current, error)) {
        return false;
    }
    AbSpan name = current->object.name;
    AbDataValue value = object->sprite;
    return host->sprite(host->context, name, value, error);
}

static bool ordinary_sprite(AbBirdCollisionSpriteState *state,
                            AbBirdCollisionSpriteDefinition *definition,
                            AbDataValue specialty,
                            const AbBirdCollisionSpecialtyHost *host, AbError *error) {
    if (truthy(definition->specialty_available_after_collision) ||
        specialty_is(specialty, "BOMB") || specialty_is(specialty, "BUBBLES")) {
        return true;
    }
    state->bird_specialty_available = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    if (definition->sprite_collision.kind == AB_DATA_NIL) {
        return true;
    }
    AbBirdCollisionObject *current = NULL;
    if (!flying(state, &current, error)) {
        return false;
    }
    if (truthy(current->dont_update_damage_sprite)) {
        return true;
    }
    if (specialty_is(specialty, "GRENADE") &&
        !truthy(state->bird_specialty_available)) {
        return true;
    }
    AbBirdCollisionObject *object = NULL;
    if (!world_flying(state, host, &object, error)) {
        return false;
    }
    object->sprite = definition->sprite_collision;
    return set_sprite(state, host, object, error);
}

static bool bomb(AbBirdCollisionSpriteState *state,
                 AbBirdCollisionSpriteDefinition *definition,
                 const AbBirdCollisionSpecialtyHost *host, AbError *error) {
    AbBirdCollisionObject *current = NULL;
    if (!flying(state, &current, error)) {
        return false;
    }
    if (literal_true(current->collision)) {
        return true;
    }
    AbBirdCollisionObject *object = NULL;
    if (!world_flying(state, host, &object, error)) {
        return false;
    }
    object->bomb_timer =
        truthy(definition->explosion_timer)
            ? definition->explosion_timer
            : (AbDataValue){.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3fc00000)};
    object->object.damage_sprite = definition->sprite_special1;
    object->sprite = object->object.damage_sprite;
    if (!set_sprite(state, host, object, error)) {
        return false;
    }
    state->bird_specialty_available = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    return true;
}

static bool bubbles(AbBirdCollisionSpriteState *state,
                    const AbBirdCollisionSpecialtyHost *host, AbError *error) {
    AbBirdCollisionObject *current = NULL;
    if (!flying(state, &current, error)) {
        return false;
    }
    if (truthy(current->in_bubble) || current->pink_timer.kind != AB_DATA_NIL ||
        literal_true(current->collision)) {
        return true;
    }
    AbBirdCollisionObject *object = NULL;
    if (!world_flying(state, host, &object, error)) {
        return false;
    }
    object->pink_timer =
        (AbDataValue){.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3fc00000)};
    return true;
}

static bool release_hat(AbBirdCollisionSpriteState *state,
                        const AbBirdCollisionSpecialtyHost *host, AbError *error) {
    AbBirdCollisionObject *current = NULL;
    if (!flying(state, &current, error)) {
        return false;
    }
    if (!host->hat) {
        return ab_fail(error, "Missing bird specialty hat-lookup host");
    }
    AbBirdCollisionHat *hat = NULL;
    if (!host->hat(host->context, current->object.name, &hat, error)) {
        return false;
    }
    if (!hat) {
        return true;
    }
    if (!host->object) {
        return ab_fail(error, "Missing bird specialty hat-object host");
    }
    AbBirdCollisionObject *object = NULL;
    if (!host->object(host->context, hat->object_name, &object, error)) {
        return false;
    }
    if (!object) {
        return true;
    }
    if (!host->release_hat) {
        return ab_fail(error, "Missing bird specialty releaseHat host");
    }
    return host->release_hat(host->context, current->object.name, error);
}

bool ab_bird_collision_specialty(const AbBlockCollisionInput *contact,
                                 AbBirdCollisionObject *second,
                                 AbBirdCollisionSpriteState *state,
                                 const AbBirdCollisionSpecialtyHost *host,
                                 AbError *error) {
    if (!contact || !second) {
        return ab_fail(error, "Invalid bird collision specialty input");
    }
    if (truthy(second->object.sensor)) {
        return true;
    }
    if (!state) {
        return ab_fail(error, "Missing bird collision specialty state");
    }
    if (!state->flying_bird ||
        (!same_name(state->flying_bird->object.name, contact->first) &&
         !same_name(state->flying_bird->object.name, contact->second))) {
        return true;
    }
    if (!host || !host->definition) {
        return ab_fail(error, "Missing bird collision specialty definition host");
    }
    const AbBirdCollisionNamedInput captured = {contact->first, contact->second,
                                                contact->force, contact->damage,
                                                contact->normal};
    AbBirdCollisionSpriteDefinition *definition = NULL;
    if (!host->definition(host->context, state->flying_bird->object.name, &definition,
                          error)) {
        return false;
    }
    if (!definition) {
        return ab_fail(error, "Missing bird collision specialty definition");
    }
    AbDataValue specialty = definition->specialty;
    if (!named_collision(state, definition, &captured, host, error) ||
        !ordinary_sprite(state, definition, specialty, host, error)) {
        return false;
    }
    if (specialty_is(specialty, "BOMB") && !bomb(state, definition, host, error)) {
        return false;
    }
    if (specialty_is(specialty, "BUBBLES") && !bubbles(state, host, error)) {
        return false;
    }
    AbBirdCollisionObject *current = NULL;
    if (!flying(state, &current, error)) {
        return false;
    }
    current->collision = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    return release_hat(state, host, error);
}
