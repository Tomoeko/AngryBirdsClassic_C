#include "ab/bird_collision_mighty.h"

#include <string.h>

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool same_name(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || (first.data && second.data &&
                            memcmp(first.data, second.data, first.size) == 0));
}

bool ab_bird_collision_mighty(const AbBirdCollisionMightyInput *input,
                              const AbBirdCollisionMightyHost *host, AbError *error) {
    if (!input) {
        return ab_fail(error, "Missing mighty bird collision input");
    }
    if (!input->flying_present) {
        return true;
    }
    if (!host || !host->object ||
        (input->flying_name.size && !input->flying_name.data)) {
        return ab_fail(error, "Invalid mighty bird collision world lookup");
    }
    AbBirdCollisionObject *object = NULL;
    if (!host->object(host->context, input->flying_name, &object, error)) {
        return false;
    }
    if (!object) {
        return ab_fail(error, "Missing flying bird collision world object");
    }
    if (!truthy(object->mighty_bait) && !truthy(object->mighty_eagle)) {
        return true;
    }
    if ((input->first.size && !input->first.data) ||
        (input->second.size && !input->second.data)) {
        return ab_fail(error, "Invalid mighty bird collision incoming names");
    }
    AbSpan other = {0};
    bool invoke = false;
    if (same_name(input->flying_name, input->first)) {
        if (!input->second_object) {
            return ab_fail(error, "Missing mighty bird second collision object");
        }
        if (!truthy(input->second_object->object.sensor)) {
            other = input->second;
            invoke = true;
        }
    }
    if (!invoke && same_name(input->flying_name, input->second)) {
        if (!input->first_object) {
            return ab_fail(error, "Missing mighty bird first collision object");
        }
        if (!truthy(input->first_object->object.sensor)) {
            other = input->first;
            invoke = true;
        }
    }
    if (!invoke) {
        return true;
    }
    AbBirdCollisionFunction function = object->collision_function;
    if (!function.call) {
        return ab_fail(error, "Mighty bird collisionFunction is not callable");
    }
    AbSpan name = input->flying_name;
    return function.call(function.context, name, other, error);
}
