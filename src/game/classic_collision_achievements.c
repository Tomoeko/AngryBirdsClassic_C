#include "ab/classic_collision_achievements.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AbDataTable table;
    AbDataEntry entry;
} CollisionEvent;

struct AbClassicCollisionAchievements {
    AbClassicCollisionAchievementConfig config;
    AbClassicLevelView level;
    CollisionEvent *events;
    size_t used, bytes;
    bool calling;
};

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

static bool equal(AbDataValue first, AbDataValue second) {
    if (first.kind != second.kind)
        return false;
    switch (first.kind) {
        case AB_DATA_NIL:
            return true;
        case AB_DATA_BOOLEAN:
            return !first.bits == !second.bits;
        case AB_DATA_NUMBER: {
            float a, b;
            memcpy(&a, &first.bits, sizeof(a));
            memcpy(&b, &second.bits, sizeof(b));
            return a == b;
        }
        case AB_DATA_TABLE:
            return first.table == second.table;
        case AB_DATA_OBJECT:
            return first.object == second.object;
        case AB_DATA_STRING:
            return first.string.size == second.string.size &&
                   (!first.string.size ||
                    (first.string.data && second.string.data &&
                     !memcmp(first.string.data, second.string.data,
                             first.string.size)));
    }
    return false;
}

static bool set_flag(AbDataTable *object, const char *key, AbError *error) {
    AbObjectPropertyAccess access = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = object}};
    return ab_object_property_set(
        &access, span(key), (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1}, error);
}

static bool suppressed(AbClassicCollisionAchievements *owner) {
    AbDataValue eagle = *owner->config.eagle_bait_launched;
    return (eagle.kind == AB_DATA_BOOLEAN && eagle.bits) ||
           truthy(*owner->config.special_game_mode);
}

static bool set_setting(AbClassicCollisionAchievements *owner, const char *name,
                        AbError *error) {
    AbSpan key = span(name);
    return ab_settings_statistics_write(
        owner->config.settings, &key, 1,
        (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1}, error);
}

static bool notify(AbClassicCollisionAchievements *owner, const char *identifier,
                   AbError *error) {
    if (owner->used == owner->config.notifications)
        return ab_fail(error, "Collision achievement event storage exhausted");
    CollisionEvent *event = owner->events + owner->used++;
    event->entry = (AbDataEntry){text("id"), text(identifier)};
    event->table = (AbDataTable){&event->entry, 1, 1, NULL};
    return ab_event_manager_notify(owner->config.events, &event->table, error);
}

static bool both_shot(AbClassicCollisionAchievements *owner, AbDataTable *first,
                      AbDataTable *second, AbError *error) {
    if (!truthy(field(first, "hasCollided")) && !truthy(field(second, "hasCollided")) &&
        !truthy(field(first, "isEagleBait")) && !truthy(field(second, "isEagleBait"))) {
        AbDataValue a = field(first, "parentName"), b = field(second, "parentName");
        if (a.kind != AB_DATA_NIL && b.kind != AB_DATA_NIL && equal(a, b))
            return true;
        if (!set_flag(second, "hasCollided", error) ||
            !set_flag(first, "hasCollided", error))
            return false;
        if (suppressed(owner))
            return true;
        return set_setting(owner, "wilhelmTell", error) &&
               notify(owner, "EID_BIRDS_COLLIDED_ON_FLY", error);
    }
    if (truthy(field(first, "isMightyEagle")) && truthy(field(second, "isEagleBait")))
        return set_flag(second, "outsideBoundaries", error);
    if (truthy(field(second, "isMightyEagle")) && truthy(field(first, "isEagleBait")))
        return set_flag(first, "outsideBoundaries", error);
    return true;
}

static bool bullseye(AbClassicCollisionAchievements *owner, AbDataTable *bird,
                     AbDataTable *goal, bool global_class, AbError *error) {
    if (truthy(field(ab_settings_statistics_root(owner->config.settings), "bullsEye")))
        return true;
    if (!owner->config.start_position)
        return ab_fail(error, "Missing collision achievement sling start position");
    AbVec2 start;
    if (!owner->config.start_position(owner->config.context, global_class, &start,
                                      error))
        return false;
    if (truthy(field(bird, "hasCollided")))
        return true;
    AbDataValue position = field(goal, "x");
    float x;
    if (position.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Nonnumeric collision achievement goal position");
    memcpy(&x, &position.bits, sizeof(x));
    if (!isfinite(x) || !isfinite(start.x))
        return ab_fail(error, "Nonfinite collision achievement position");
    if (!(x - start.x > 75.0f))
        return true;
    if (truthy(field(bird, "shot")) && !set_flag(bird, "hasCollided", error))
        return false;
    if (suppressed(owner))
        return true;
    return notify(owner, "EID_ACHIEVEMENT_BULLSEYE", error) &&
           set_setting(owner, "bullsEye", error);
}

bool ab_classic_collision_achievements_open(AbClassicCollisionAchievementConfig config,
                                            AbClassicCollisionAchievements **output,
                                            AbError *error) {
    if (!output)
        return ab_fail(error, "Missing collision achievement output");
    *output = NULL;
    AbClassicLevelView level = ab_classic_level_view(config.level);
    if (!level.properties || !config.settings || !config.events ||
        !config.eagle_bait_launched || !config.special_game_mode ||
        config.storage_bytes < sizeof(AbClassicCollisionAchievements) ||
        config.notifications >
            (config.storage_bytes - sizeof(AbClassicCollisionAchievements)) /
                sizeof(CollisionEvent))
        return ab_fail(error, "Invalid collision achievement binding or byte limit");
    AbClassicCollisionAchievements *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate collision achievements");
    owner->config = config;
    owner->level = level;
    owner->bytes = sizeof(*owner) + config.notifications * sizeof(CollisionEvent);
    if (config.notifications) {
        owner->events = calloc(config.notifications, sizeof(*owner->events));
        if (!owner->events) {
            ab_classic_collision_achievements_close(owner);
            return ab_fail(error, "Cannot allocate collision achievement events");
        }
    }
    *output = owner;
    return true;
}

void ab_classic_collision_achievements_close(AbClassicCollisionAchievements *owner) {
    if (owner) {
        free(owner->events);
        free(owner);
    }
}

size_t
ab_classic_collision_achievements_bytes(const AbClassicCollisionAchievements *owner) {
    return owner ? owner->bytes : 0;
}

bool ab_classic_collision_achievements_call(void *context, size_t first_index,
                                            size_t second_index, AbError *error) {
    AbClassicCollisionAchievements *owner = context;
    if (!owner || owner->calling || first_index >= owner->level.properties->count ||
        second_index >= owner->level.properties->count)
        return ab_fail(error, "Invalid or reentrant collision achievement call");
    owner->calling = true;
    AbDataTable *first = &owner->level.properties->objects[first_index].physical;
    AbDataTable *second = &owner->level.properties->objects[second_index].physical;
    bool okay = true;
    if (truthy(field(first, "shot")) && truthy(field(second, "shot")))
        okay = both_shot(owner, first, second, error);
    else if (truthy(field(first, "shot")) && truthy(field(second, "levelGoal")))
        okay = bullseye(owner, first, second, false, error);
    else if (truthy(field(second, "shot")) && truthy(field(first, "levelGoal")))
        okay = bullseye(owner, second, first, true, error);
    owner->calling = false;
    return okay;
}
