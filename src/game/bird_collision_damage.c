#include "ab/bird_collision_damage.h"

#include <string.h>

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool same_value(AbDataValue first, AbDataValue second) {
    if (first.kind != second.kind) {
        return false;
    }
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

bool ab_bird_collision_damage_refresh(AbBirdCollisionObject *second,
                                      AbBirdCollisionDamageHost *host, AbError *error) {
    if (!second) {
        return ab_fail(error, "Missing bird collision damage object");
    }
    if (truthy(second->object.sensor) ||
        (second->controllable.kind == AB_DATA_BOOLEAN && second->controllable.bits)) {
        return true;
    }
    if (!host || host->block_table.kind != AB_DATA_TABLE || !host->block_table.table ||
        host->block_table.table->count > host->block_table.table->capacity ||
        (host->block_table.table->count && !host->block_table.table->entries)) {
        return ab_fail(error, "Invalid bird collision blockTable");
    }
    const AbDataValue *found =
        ab_data_get(host->block_table.table,
                    (AbDataValue){.kind = AB_DATA_STRING,
                                  .string = {(const uint8_t *)"blocks", 6}});
    AbDataValue blocks = found ? *found : (AbDataValue){0};
    if (!host->sprites) {
        return ab_fail(error, "Missing bird collision damage-sprite host");
    }
    AbDamageSpriteNames *names = NULL;
    if (!host->sprites(host->context, second, blocks, &names, error)) {
        return false;
    }
    if (!names) {
        return ab_fail(error, "Missing bird collision damage-sprite descriptor");
    }
    if (same_value(second->object.damage_sprite, names->sprite)) {
        return true;
    }
    second->object.damage_sprite = names->sprite;
    second->object.blink_sprite = names->blink;
    second->object.smile_sprite = names->smile;
    if (!host->particles) {
        return ab_fail(error, "Missing bird collision damage-particle host");
    }
    return host->particles(host->context, second, error);
}
