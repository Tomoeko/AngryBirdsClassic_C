#include "ab/museum_bird_behavior.h"

#include <string.h>

static AbDataValue key(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static bool named(AbDataValue value, const char *name) {
    return value.kind == AB_DATA_STRING && value.string.size == strlen(name) &&
           (!value.string.size ||
            (value.string.data && !memcmp(value.string.data, name, value.string.size)));
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool field(AbDataValue object, const char *name, AbDataValue *output,
                  AbError *error) {
    if (object.kind != AB_DATA_TABLE || !object.table ||
        object.table->count > object.table->capacity ||
        (object.table->count && !object.table->entries)) {
        return ab_fail(error, "Museum bird behavior indexed a non-table");
    }
    const AbDataValue *value = ab_data_get(object.table, key(name));
    *output = value ? *value : (AbDataValue){0};
    return true;
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

static bool state(AbMuseumBirdBehavior *behavior, AbDataValue *self, AbError *error) {
    size_t capacity = behavior && behavior->kind == AB_MUSEUM_BIRD_BLUES ? 8u : 4u;
    if (!behavior || (unsigned)behavior->kind > AB_MUSEUM_BIRD_BLUES_CAMERA ||
        behavior->fields.entries != behavior->entries ||
        behavior->fields.count > capacity || behavior->fields.capacity != capacity) {
        return ab_fail(error, "Invalid Museum bird behavior instance");
    }
    *self = (AbDataValue){.kind = AB_DATA_TABLE, .table = &behavior->fields};
    return true;
}

static bool set_flying(AbMuseumBirdBehavior *behavior, bool flying, AbError *error) {
    for (size_t index = 0; index < behavior->fields.count; ++index) {
        if (named(behavior->entries[index].key, "flying")) {
            behavior->entries[index].value =
                (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = flying};
            return true;
        }
    }
    if (behavior->fields.count == behavior->fields.capacity)
        return ab_fail(error, "Museum flying-field storage exhausted");
    behavior->entries[behavior->fields.count++] =
        (AbDataEntry){key("flying"), {.kind = AB_DATA_BOOLEAN, .bits = flying}};
    return true;
}

static bool call(AbMuseumBirdHost *host, AbMuseumBirdOperation operation,
                 AbDataValue object, const char *sequence, AbError *error) {
    if (!host || !host->call)
        return ab_fail(error, "Missing Museum bird behavior service");
    AbMuseumBirdCall request = {operation, object, sequence};
    return host->call(host->context, &request, error);
}

bool ab_museum_bird_behavior_initialize(AbSpan name, AbDataValue game_object,
                                        AbDataValue overrides,
                                        AbMuseumBirdBehavior *output, AbError *error) {
    AbDataValue class_name = {.kind = AB_DATA_STRING, .string = name};
    bool trajectory = named(class_name, "MuseumBirdTrajectoryRenderer");
    bool blues = named(class_name, "MuseumBluesTrajectoryRenderer");
    bool blues_camera = named(class_name, "MuseumBluesCameraFollowBehavior");
    if (!output ||
        (!blues && !blues_camera &&
         (!trajectory && !named(class_name, "MuseumBirdCameraFollowBehavior"))))
        return ab_fail(error, "Unsupported Museum bird behavior class");
    if (truthy(overrides) && (overrides.kind != AB_DATA_TABLE || !overrides.table ||
                              overrides.table->count > (blues ? 1u : 0u) ||
                              overrides.table->count > overrides.table->capacity ||
                              (overrides.table->count && !overrides.table->entries))) {
        return ab_fail(error, "Unsupported Museum bird behavior overrides");
    }
    AbDataValue cluster = {.kind = AB_DATA_BOOLEAN};
    if (blues && overrides.kind == AB_DATA_TABLE && overrides.table->count) {
        const AbDataEntry *entry = overrides.table->entries;
        if (!named(entry->key, "isCluster"))
            return ab_fail(error, "Unsupported Blues constructor parameter");
        if (truthy(entry->value))
            cluster = entry->value;
    }
    AbDataValue physical = {0};
    if (!field(game_object, "physicsObject", &physical, error))
        return false;
    AbMuseumBirdBehavior next = {
        .kind = blues_camera ? AB_MUSEUM_BIRD_BLUES_CAMERA
                : blues      ? AB_MUSEUM_BIRD_BLUES
                : trajectory ? AB_MUSEUM_BIRD_TRAJECTORY
                             : AB_MUSEUM_BIRD_CAMERA,
        .fields = {.count = physical.kind == AB_DATA_NIL ? 2u : 3u,
                   .capacity = blues ? 8u : 4u},
        .entries = {{key("name"), class_name},
                    {key("gameObject"), game_object},
                    {key("physicsObject"), physical}}};
    if (blues)
        next.entries[next.fields.count++] = (AbDataEntry){key("isCluster"), cluster};
    *output = next;
    output->fields.entries = output->entries;
    return true;
}

bool ab_museum_bird_behavior_init(AbMuseumBirdBehavior *behavior,
                                  AbMuseumBirdHost *host, AbError *error) {
    AbDataValue self;
    if (!state(behavior, &self, error))
        return false;
    if (behavior->kind == AB_MUSEUM_BIRD_BLUES_CAMERA)
        return call(host, AB_MUSEUM_CAMERA_CANCEL, (AbDataValue){0}, NULL, error) &&
               call(host, AB_MUSEUM_CAMERA_SEQUENCE, (AbDataValue){0},
                    "FollowShotBirdCameraSequence", error);
    return behavior->kind != AB_MUSEUM_BIRD_BLUES ||
           ab_blues_trajectory_event(&behavior->fields, AB_BLUES_TRAJECTORY_INIT,
                                     (AbDataValue){0}, host ? host->blues : NULL,
                                     error);
}

bool ab_museum_bird_behavior_launch(AbMuseumBirdBehavior *behavior,
                                    AbMuseumBirdHost *host, AbError *error) {
    AbDataValue self = {0};
    if (!state(behavior, &self, error))
        return false;
    if (behavior->kind == AB_MUSEUM_BIRD_BLUES_CAMERA)
        return true;
    if (behavior->kind == AB_MUSEUM_BIRD_BLUES)
        return ab_blues_trajectory_event(&behavior->fields, AB_BLUES_TRAJECTORY_LAUNCH,
                                         (AbDataValue){0}, host ? host->blues : NULL,
                                         error);
    if (behavior->kind == AB_MUSEUM_BIRD_CAMERA)
        return call(host, AB_MUSEUM_CAMERA_SEQUENCE, (AbDataValue){0},
                    "FollowShotBirdCameraSequence", error);
    AbDataValue physical = {0};
    return field(self, "physicsObject", &physical, error) &&
           call(host, AB_MUSEUM_TRAJECTORY_ADD, physical, NULL, error) &&
           set_flying(behavior, true, error);
}

bool ab_museum_bird_behavior_collision(AbMuseumBirdBehavior *behavior,
                                       AbDataValue other, AbMuseumBirdHost *host,
                                       AbError *error) {
    AbDataValue self = {0}, controllable = {0}, name = {0}, flying = {0},
                physical = {0};
    if (!state(behavior, &self, error))
        return false;
    if (behavior->kind == AB_MUSEUM_BIRD_CAMERA ||
        behavior->kind == AB_MUSEUM_BIRD_BLUES_CAMERA)
        return true;
    if (behavior->kind == AB_MUSEUM_BIRD_BLUES)
        return ab_blues_trajectory_event(&behavior->fields,
                                         AB_BLUES_TRAJECTORY_COLLISION, other,
                                         host ? host->blues : NULL, error);
    if (!field(other, "controllable", &controllable, error))
        return false;
    if (truthy(controllable))
        return true;
    if (!field(other, "name", &name, error))
        return false;
    if (!truthy(name))
        return true;
    if (!field(self, "flying", &flying, error))
        return false;
    if (!truthy(flying))
        return true;
    if (!field(self, "physicsObject", &physical, error) ||
        !call(host, AB_MUSEUM_TRAJECTORY_REMOVE, physical, NULL, error) ||
        !field(self, "physicsObject", &physical, error))
        return false;
    if (same(physical, host->flying_bird)) {
        if (!field(self, "physicsObject", &physical, error) ||
            !call(host, AB_MUSEUM_TRAJECTORY_KEEP, physical, NULL, error))
            return false;
    }
    return set_flying(behavior, false, error);
}

bool ab_museum_bird_behavior_destroy(AbMuseumBirdBehavior *behavior,
                                     AbMuseumBirdHost *host, AbError *error) {
    AbDataValue self = {0}, flying = {0}, physical = {0};
    if (!state(behavior, &self, error))
        return false;
    if (behavior->kind == AB_MUSEUM_BIRD_CAMERA ||
        behavior->kind == AB_MUSEUM_BIRD_BLUES_CAMERA)
        return true;
    if (behavior->kind == AB_MUSEUM_BIRD_BLUES)
        return ab_blues_trajectory_event(&behavior->fields, AB_BLUES_TRAJECTORY_DESTROY,
                                         (AbDataValue){0}, host ? host->blues : NULL,
                                         error);
    if (!field(self, "flying", &flying, error))
        return false;
    if (!truthy(flying))
        return true;
    if (!field(self, "physicsObject", &physical, error) ||
        !call(host, AB_MUSEUM_TRAJECTORY_REMOVE, physical, NULL, error) ||
        !field(self, "physicsObject", &physical, error))
        return false;
    if (!same(physical, host->flying_bird))
        return true;
    return field(self, "physicsObject", &physical, error) &&
           call(host, AB_MUSEUM_TRAJECTORY_KEEP, physical, NULL, error);
}

bool ab_museum_bird_behavior_specialty(AbMuseumBirdBehavior *behavior,
                                       AbMuseumBirdHost *host, AbError *error) {
    AbDataValue self = {0}, object = {0}, value = {0};
    if (!state(behavior, &self, error))
        return false;
    if (behavior->kind == AB_MUSEUM_BIRD_BLUES_CAMERA)
        return true;
    if (behavior->kind == AB_MUSEUM_BIRD_BLUES)
        return ab_blues_trajectory_event(
            &behavior->fields, AB_BLUES_TRAJECTORY_SPECIALTY, (AbDataValue){0},
            host ? host->blues : NULL, error);
    if (behavior->kind == AB_MUSEUM_BIRD_TRAJECTORY) {
        if (!field(self, "physicsObject", &object, error))
            return false;
        if (!host)
            return ab_fail(error, "Missing Museum flying-bird binding");
        if (!same(object, host->flying_bird))
            return true;
        if (!field(self, "physicsObject", &object, error) ||
            !field(object, "definition", &value, error))
            return false;
        if (named(value, "RedBird"))
            return true;
        return field(self, "physicsObject", &object, error) &&
               call(host, AB_MUSEUM_TRAJECTORY_KEEP, object, NULL, error);
    }
    if (!field(self, "gameObject", &object, error))
        return false;
    if (!host || !host->get_property)
        return ab_fail(error, "Missing Museum bird property getter");
    if (!host->get_property(host->context, object, key("specialty").string, &value,
                            error))
        return false;
    if (!named(value, "BOOMERANG"))
        return true;
    if (!field(self, "gameObject", &object, error))
        return false;
    if (!host->get_property)
        return ab_fail(error, "Missing fresh Museum launch-vector getter");
    if (!host->get_property(host->context, object, key("launchVelocity").string, &value,
                            error) ||
        !field(value, "x", &value, error))
        return false;
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Nonnumeric Museum boomerang launch X");
    float x;
    memcpy(&x, &value.bits, sizeof(x));
    if (!(x < 0))
        return true;
    return call(host, AB_MUSEUM_CAMERA_SEQUENCE, (AbDataValue){0},
                "FollowBoomerangBird", error);
}
