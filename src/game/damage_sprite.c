#include "ab/damage_sprite.h"

#include <math.h>
#include <string.h>

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(
        table, (AbDataValue){.kind = AB_DATA_STRING,
                             .string = {(const uint8_t *)name, strlen(name)}});
    return value ? *value : (AbDataValue){0};
}

static bool numeric(AbDataValue value, float *output) {
    if (value.kind != AB_DATA_NUMBER) {
        return false;
    }
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output);
}

static bool valid_order(const AbDataTable *ranges, const size_t *order) {
    if (ranges->count > ranges->capacity ||
        (ranges->count && (!ranges->entries || !order))) {
        return false;
    }
    for (size_t cursor = 0; cursor < ranges->count; ++cursor) {
        if (order[cursor] >= ranges->count) {
            return false;
        }
        for (size_t previous = 0; previous < cursor; ++previous) {
            if (order[previous] == order[cursor]) {
                return false;
            }
        }
    }
    return true;
}

static void select_range(const AbDataTable *range, AbDamageSpriteNames *output) {
    output->sprite = field(range, "sprite");
    output->blink = output->smile = output->sprite;
    AbDataValue blink = field(range, "blinkSprite");
    AbDataValue smile = field(range, "smileSprite");
    if (blink.kind != AB_DATA_NIL) {
        output->blink = blink;
    }
    if (smile.kind != AB_DATA_NIL) {
        output->smile = smile;
    }
    output->frozen = field(range, "frozenSprite");
}

bool ab_damage_sprite_names(AbDataValue base_sprite, AbDataValue strength,
                            AbDataValue initial_strength,
                            AbDataValue definition_strength, const AbDataTable *ranges,
                            const size_t *range_order, AbDamageSpriteNames *output,
                            AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing damage-sprite selection output");
    }
    AbDamageSpriteNames selected = {
        .sprite = base_sprite, .blink = base_sprite, .smile = base_sprite};
    if (!ranges || strength.kind == AB_DATA_NIL) {
        *output = selected;
        return true;
    }
    if (!valid_order(ranges, range_order)) {
        return ab_fail(error, "Invalid damage-sprite range order");
    }
    bool initial = initial_strength.kind != AB_DATA_NIL &&
                   (initial_strength.kind != AB_DATA_BOOLEAN || initial_strength.bits);
    float remaining, base;
    if (!numeric(strength, &remaining) ||
        !numeric(initial ? initial_strength : definition_strength, &base)) {
        return ab_fail(error, "Unsupported damage-sprite strength boundary");
    }
    float percent = remaining / base;
    percent = percent * 100.0f;
    for (size_t cursor = 0; cursor < ranges->count; ++cursor) {
        AbDataValue value = ranges->entries[range_order[cursor]].value;
        if (value.kind != AB_DATA_TABLE || !value.table) {
            return ab_fail(error, "Invalid damage-sprite interval table");
        }
        float minimum, maximum;
        if (!numeric(field(value.table, "min"), &minimum)) {
            return ab_fail(error, "Invalid damage-sprite interval minimum");
        }
        if (!(minimum < percent)) {
            continue;
        }
        if (!numeric(field(value.table, "max"), &maximum)) {
            return ab_fail(error, "Invalid damage-sprite interval maximum");
        }
        if (percent <= maximum) {
            select_range(value.table, &selected);
            break;
        }
    }
    *output = selected;
    return true;
}

bool ab_object_initial_sprites(const AbObjectDefinition *definition,
                               const AbLuaData *catalog,
                               AbStringOrderWorkspace *workspace,
                               AbDamageSpriteNames *output, AbError *error) {
    if (!definition || !definition->source || !output) {
        return ab_fail(error, "Missing initial damage-sprite definition or output");
    }
    AbDataValue base = field(definition->source, "sprite");
    AbDataValue strength =
        ab_object_property(definition, (AbSpan){(const uint8_t *)"strength", 8});
    AbDataValue ranges = field(definition->source, "damageSprites");
    if (ranges.kind == AB_DATA_NIL || strength.kind == AB_DATA_NIL) {
        return ab_damage_sprite_names(base, strength, (AbDataValue){0},
                                      (AbDataValue){0}, NULL, NULL, output, error);
    }
    if (ranges.kind != AB_DATA_TABLE || !ranges.table || !catalog || !workspace) {
        return ab_fail(error, "Unsupported initial damage-sprite ranges");
    }
    const AbDataTableLayout *layout = ab_lua_table_layout(catalog, ranges.table);
    if (!layout || !layout->no_nil_assignments ||
        !ab_data_string_order(ranges.table, layout->hash_capacity, workspace, error)) {
        return ab_fail(error, "Unsupported damage-sprite constructor order");
    }
    /* ObjectCreation prototype 883, instructions 232..237, writes the material
     * fallback into blockTable.blocks[definition].strength before MuseumMode's
     * initial getDamageSprite call. Keep our catalog immutable and use that
     * normalized property as the initial denominator as well as the numerator. */
    return ab_damage_sprite_names(base, strength, (AbDataValue){0}, strength,
                                  ranges.table, workspace->indices, output, error);
}
