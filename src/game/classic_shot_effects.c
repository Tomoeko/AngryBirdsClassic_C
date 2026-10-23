#include "ab/classic_shot_effects.h"

#include <string.h>

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value =
        ab_data_get(table, (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)});
    return value ? *value : (AbDataValue){0};
}

static AbDataTable *physical(AbClassicLevel *level, size_t object, AbError *error) {
    AbClassicLevelView view = ab_classic_level_view(level);
    if (!view.properties || object >= view.properties->count) {
        ab_fail(error, "Invalid shot effect object");
        return NULL;
    }
    return &view.properties->objects[object].physical;
}

bool ab_classic_shot_attach_particles_host(AbClassicLevel *level, size_t object,
                                           const AbPowerupUses *uses,
                                           bool powerups_enabled, void *context,
                                           AbClassicShotTrails create, AbError *error) {
    AbDataTable *bird = physical(level, object, error);
    if (!bird)
        return false;
    AbDataValue king = {.kind = AB_DATA_BOOLEAN};
    AbDataValue potion = king;
    if (powerups_enabled) {
        bool used;
        if (!ab_powerup_uses_contains(uses, "kingsling", &used, error))
            return false;
        king.bits = used;
        potion = field(bird, "usedPowerPotion");
        if (!truthy(potion))
            potion = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    }
    AbObjectPropertyAccess raw = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = bird}};
    if (!ab_object_property_set(&raw, span("shotWithKingSling"), king, error) ||
        !ab_object_property_set(&raw, span("shotWithPowerPotion"), potion, error))
        return false;
    if (truthy(field(bird, "shotWithPowerPotion")) ||
        truthy(field(bird, "shotWithKingSling"))) {
        if (!create)
            return ab_fail(error, "Powered shot trail spawners remain unsupported");
        return create(context, object, truthy(field(bird, "shotWithPowerPotion")),
                      truthy(field(bird, "shotWithKingSling")), error);
    }
    return true;
}

bool ab_classic_shot_attach_particles(AbClassicLevel *level, size_t object,
                                      const AbPowerupUses *uses, bool powerups_enabled,
                                      AbError *error) {
    return ab_classic_shot_attach_particles_host(level, object, uses, powerups_enabled,
                                                 NULL, NULL, error);
}

bool ab_classic_red_bird_shot(AbClassicLevel *level, size_t object,
                              bool red_special_available, AbError *error) {
    if (!red_special_available)
        return true;
    AbDataTable *bird = physical(level, object, error);
    if (!bird)
        return false;
    if (truthy(field(bird, "hasCollided")))
        return true;
    return ab_fail(error, "Active red-bird special setup remains unsupported");
}

bool ab_classic_red_bird_collided(bool red_special_available, AbError *error) {
    return !red_special_available ||
           ab_fail(error, "Active red-bird special collision remains unsupported");
}

bool ab_classic_red_bird_removed(bool red_special_available, AbError *error) {
    return !red_special_available ||
           ab_fail(error, "Active red-bird removal cleanup remains unsupported");
}
