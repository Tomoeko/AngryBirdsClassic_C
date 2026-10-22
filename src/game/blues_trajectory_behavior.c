#include "ab/blues_trajectory_behavior.h"
#include "ab/object_property_access.h"

#include <string.h>

static AbDataValue table(AbDataTable *value) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = value};
}

static AbDataValue key(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool get(AbDataValue object, AbDataValue name, AbDataValue *output,
                AbError *error) {
    if (object.kind != AB_DATA_TABLE || !object.table ||
        object.table->count > object.table->capacity ||
        (object.table->count && !object.table->entries))
        return ab_fail(error, "Blues trajectory indexed a non-table");
    const AbDataValue *value = ab_data_get(object.table, name);
    *output = value ? *value : (AbDataValue){0};
    return true;
}

static bool field(AbDataTable *instance, const char *name, AbDataValue *output,
                  AbError *error) {
    return get(table(instance), key(name), output, error);
}

static bool set(AbDataTable *instance, const char *name, bool value, AbError *error) {
    AbObjectPropertyAccess access = {.physics_object = table(instance)};
    return ab_object_property_set(&access, key(name).string,
                                  (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = value},
                                  error);
}

static bool same(AbDataValue first, AbDataValue second) {
    if (first.kind != second.kind)
        return false;
    switch (first.kind) {
        case AB_DATA_NIL:
            return true;
        case AB_DATA_BOOLEAN:
            return (first.bits != 0) == (second.bits != 0);
        case AB_DATA_NUMBER: {
            float a, b;
            memcpy(&a, &first.bits, sizeof(a));
            memcpy(&b, &second.bits, sizeof(b));
            return a == b;
        }
        case AB_DATA_STRING:
            return first.string.size == second.string.size &&
                   (!first.string.size ||
                    (first.string.data && second.string.data &&
                     !memcmp(first.string.data, second.string.data,
                             first.string.size)));
        case AB_DATA_TABLE:
            return first.table == second.table;
        case AB_DATA_OBJECT:
            return first.object == second.object;
    }
    return false;
}

static bool call(AbBluesTrajectoryHost *host, AbBluesTrajectoryCall *request,
                 AbError *error) {
    return host && host->call ? host->call(host->context, request, error)
                              : ab_fail(error, "Missing Blues trajectory service");
}

static bool object_call(AbDataTable *instance, AbBluesTrajectoryHost *host,
                        AbBluesTrajectoryOperation operation, AbError *error) {
    AbBluesTrajectoryCall request = {.operation = operation};
    return field(instance, "physicsObject", &request.object, error) &&
           call(host, &request, error);
}

static bool keep_one(AbDataTable *instance, AbBluesTrajectoryHost *host,
                     AbError *error) {
    AbBluesTrajectoryCall request = {.operation = AB_BLUES_TRAJECTORY_KEEP,
                                     .keep_slots = 1};
    return field(instance, "physicsObject", request.keep, error) &&
           call(host, &request, error);
}

static bool keep_children(AbDataTable *instance, AbBluesTrajectoryHost *host,
                          AbError *error) {
    AbBluesTrajectoryCall request = {.operation = AB_BLUES_TRAJECTORY_KEEP,
                                     .keep_slots = 4};
    if (!field(instance, "originalCluster", request.keep, error) ||
        !field(instance, "physicsObject", request.keep + 1, error))
        return false;
    const uint32_t indices[] = {UINT32_C(0x3f800000), UINT32_C(0x40000000)};
    for (size_t index = 0; index < 2; ++index) {
        AbBluesTrajectoryCall lookup = {.operation = AB_BLUES_TRAJECTORY_GET_OBJECT};
        if (!host ||
            !get(host->live_other_birds ? *host->live_other_birds : host->other_birds,
                 (AbDataValue){.kind = AB_DATA_NUMBER, .bits = indices[index]},
                 &lookup.name, error) ||
            !call(host, &lookup, error))
            return false;
        request.keep[index + 2] = lookup.result;
    }
    return call(host, &request, error);
}

static bool collision(AbDataTable *instance, bool cluster, AbDataValue other,
                      AbBluesTrajectoryHost *host, AbError *error) {
    AbDataValue value;
    if (!get(other, key("controllable"), &value, error))
        return false;
    if (truthy(value))
        return true;
    if (!get(other, key("name"), &value, error))
        return false;
    if (!truthy(value))
        return true;
    if (!field(instance, "flying", &value, error))
        return false;
    if (!truthy(value))
        return true;
    if (!object_call(instance, host, AB_BLUES_TRAJECTORY_REMOVE, error))
        return false;
    if (cluster) {
        if (!keep_one(instance, host, error))
            return false;
    } else {
        if (!field(instance, "physicsObject", &value, error))
            return false;
        AbDataValue flying =
            host->live_flying_bird ? *host->live_flying_bird : host->flying_bird;
        if (same(value, flying) && !keep_children(instance, host, error))
            return false;
    }
    return set(instance, "flying", false, error);
}

bool ab_blues_trajectory_event(AbDataTable *instance, AbBluesTrajectoryEvent event,
                               AbDataValue other, AbBluesTrajectoryHost *host,
                               AbError *error) {
    AbDataValue value;
    if (!field(instance, "isCluster", &value, error))
        return false;
    bool cluster = truthy(value);
    switch (event) {
        case AB_BLUES_TRAJECTORY_INIT:
            return cluster ||
                   (object_call(instance, host, AB_BLUES_TRAJECTORY_ADD, error) &&
                    set(instance, "flying", true, error));
        case AB_BLUES_TRAJECTORY_LAUNCH:
            return !cluster ||
                   (set(instance, "flying", true, error) &&
                    object_call(instance, host, AB_BLUES_TRAJECTORY_ADD, error));
        case AB_BLUES_TRAJECTORY_SPECIALTY:
            return !cluster ||
                   (object_call(instance, host, AB_BLUES_TRAJECTORY_REMOVE, error) &&
                    set(instance, "split", true, error));
        case AB_BLUES_TRAJECTORY_COLLISION:
            return collision(instance, cluster, other, host, error);
        case AB_BLUES_TRAJECTORY_DESTROY:
            if (!cluster)
                return true;
            if (!field(instance, "split", &value, error))
                return false;
            if (truthy(value))
                return true;
            if (!field(instance, "flying", &value, error))
                return false;
            return !truthy(value) ||
                   (object_call(instance, host, AB_BLUES_TRAJECTORY_REMOVE, error) &&
                    keep_one(instance, host, error));
    }
    return ab_fail(error, "Invalid Blues trajectory event");
}
