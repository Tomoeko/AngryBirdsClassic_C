#include "ab/bird_collision_projectiles.h"

#include <string.h>

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool literal_true(AbDataValue value) {
    return value.kind == AB_DATA_BOOLEAN && value.bits != 0;
}

static bool same_name(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || (first.data && second.data &&
                            !memcmp(first.data, second.data, first.size)));
}

static bool deactivate(AbBirdCollisionObject *object, AbBirdCollisionObject *second,
                       const AbBirdCollisionProjectileHost *host, AbError *error) {
    if (truthy(second->object.sensor) || !truthy(object->controllable)) {
        return true;
    }
    if (!host || !host->definition) {
        return ab_fail(error, "Missing bird projectile definition host");
    }
    AbBirdCollisionSpriteDefinition *definition = NULL;
    if (!host->definition(host->context, object->object.name, &definition, error)) {
        return false;
    }
    if (!definition) {
        return ab_fail(error, "Missing bird projectile definition");
    }
    AbDataValue specialty = definition->specialty;
    if (specialty.kind == AB_DATA_STRING &&
        same_name(specialty.string, (AbSpan){(const uint8_t *)"BOOMERANG", 9})) {
        object->boomerang_active = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    }
    return true;
}

static void collision_flag(AbBirdCollisionObject *object,
                           AbBirdCollisionObject *second) {
    if (!truthy(second->object.sensor) && truthy(object->shot) &&
        truthy(object->controllable)) {
        object->collision = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    }
}

bool ab_bird_collision_projectiles(const AbBirdCollisionProjectileInput *input,
                                   AbBirdCollisionGrenades *grenades,
                                   const AbBirdCollisionProjectileHost *host,
                                   AbBirdCollisionProjectileResult *output,
                                   AbError *error) {
    if (!input || !input->first_object || !input->second_object || !output ||
        (input->first.size && !input->first.data) ||
        (input->second.size && !input->second.data)) {
        return ab_fail(error, "Invalid bird projectile contact input");
    }
    const AbBirdCollisionProjectileInput captured = *input;
    AbBirdCollisionObject *first = captured.first_object,
                          *second = captured.second_object;
    if (!deactivate(first, second, host, error) ||
        !deactivate(second, second, host, error)) {
        return false;
    }
    collision_flag(first, second);
    collision_flag(second, second);
    if (!grenades || !grenades->present || grenades->count > grenades->capacity ||
        grenades->count > UINT32_C(16777216) || (grenades->count && !grenades->items)) {
        return ab_fail(error, "Invalid bird flyingGrenades table");
    }
    AbBirdCollisionProjectileResult result = {.first_present = true};
    for (size_t remaining = grenades->count; remaining; --remaining) {
        AbBirdCollisionObject *grenade = grenades->items[remaining - 1];
        if (!grenade) {
            return ab_fail(error, "Missing bird flyingGrenades sequence entry");
        }
        AbSpan name = grenade->object.name;
        bool matches_first = result.first_present && same_name(captured.first, name);
        bool matches_second = same_name(captured.second, name);
        if ((matches_first && !literal_true(second->controllable)) ||
            (matches_second && !literal_true(first->controllable))) {
            grenade->explode = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
            if (matches_first) {
                result.first_present = false;
            }
        }
        if ((result.first_present && same_name(captured.first, name) &&
             literal_true(second->controllable)) ||
            (matches_second && literal_true(first->controllable))) {
            result.return_early = true;
            *output = result;
            return true;
        }
    }
    *output = result;
    return true;
}
