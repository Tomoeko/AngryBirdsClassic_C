#include "ab/level_egg.h"

#include <string.h>

static const AbLevelEggScript scripts[] = {
    {"Level34", "ExtraBeachBall_1", "LevelGE_2", true},
    {"LevelP2_64", "ExtraGoldenEgg_1", "LevelGE_5", false},
    {"LevelP2_67", "ExtraGoldenEgg_1", "LevelGE_3", false},
    {"LevelP3_220", "StaticBalloon03_2", "LevelGE_6", true},
    {"LevelP3_313", "ExtraGoldenEgg_1", "LevelGE_8", false},
    {"LevelP4_440", "ExtraGoldenEgg_1", "LevelGE_9", false},
    {"LevelP4_444", "ExtraRubberDuck_1", "LevelGE_10", true},
    {"LevelP4_478", "ExtraGoldenEgg_1", "LevelGE_11", false},
    {"LevelP5_357", "ExtraHolyGrail_4", "LevelGE_16", true},
    {"LevelP5_650", "ExtraGoldenEgg_1", "LevelGE_17", false},
    {"LevelP5_354", "ExtraSuperBowl_2", "LevelGE_19", false},
    {"LevelP5_364", "ExtraGoldenEgg_1", "LevelGE_18", false},
    {"Level695", "ExtraGoldenEgg_1", "LevelGE_20", false},
    {"Level697", "ExtraGoldenEgg_1", "LevelGE_21", false},
    {"LevelP6_7", "ExtraTreasureChest_1", "LevelGE_23", false},
    {"LevelH564", "ExtraGoldenEgg_1", "LevelGE_24", false},
    {"UGC1Level15_custom", "ExtraGoldenEgg_1", "LevelGE_UGC1", false},
    {"Level974", "ExtraGoldenEgg_1", "LevelGE_25", false},
};

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static bool equal(AbSpan left, AbSpan right) {
    return left.size == right.size &&
           (!left.size ||
            (left.data && right.data && memcmp(left.data, right.data, left.size) == 0));
}

static bool valid_table(const AbDataTable *table) {
    return table && table->count <= table->capacity &&
           (!table->count || table->entries);
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value =
        ab_data_get(table, (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)});
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

const AbLevelEggScript *ab_level_egg_script(AbSpan level) {
    for (size_t index = 0; index < sizeof(scripts) / sizeof(*scripts); ++index) {
        if (equal(level, span(scripts[index].level)))
            return scripts + index;
    }
    return NULL;
}

bool ab_level_remove_loaded_object(const AbLuaData *level, AbSpan name,
                                   AbError *error) {
    if (!level || !valid_table(level->globals) || (name.size && !name.data))
        return ab_fail(error, "Invalid loaded-object removal input");
    AbDataValue world = field(level->globals, "world");
    if (world.kind != AB_DATA_TABLE || !valid_table(world.table))
        return ab_fail(error, "Missing loaded world for object removal");
    AbDataEntry *found = NULL;
    for (size_t index = 0; index < world.table->count; ++index) {
        AbDataEntry *entry = world.table->entries + index;
        if (entry->key.kind == AB_DATA_STRING && equal(entry->key.string, name)) {
            found = entry;
            break;
        }
    }
    if (!found)
        return ab_fail(error, "Loaded-object pruning requires its original key");
    found->value = (AbDataValue){0};
    AbDataValue joints = field(level->globals, "joints");
    if (joints.kind != AB_DATA_TABLE || !valid_table(joints.table))
        return ab_fail(error, "Missing loaded joints for object removal");
    for (size_t index = 0; index < joints.table->count; ++index) {
        AbDataEntry *entry = joints.table->entries + index;
        if (entry->value.kind == AB_DATA_NIL)
            continue;
        if (entry->value.kind != AB_DATA_TABLE || !valid_table(entry->value.table))
            return ab_fail(error, "Loaded joint removal indexes a non-table");
        AbDataValue first = field(entry->value.table, "end1");
        AbDataValue second = field(entry->value.table, "end2");
        if ((first.kind == AB_DATA_STRING && equal(first.string, name)) ||
            (second.kind == AB_DATA_STRING && equal(second.string, name)))
            entry->value = (AbDataValue){0};
    }
    return true;
}

bool ab_level_egg_prepare(const AbLevelEggScript *script, const AbLuaData *level,
                          AbLevelEggHost host, AbError *error) {
    if (!script || !script->object || !script->egg)
        return ab_fail(error, "Missing golden-egg level script");
    if (script->keep_object)
        return true;
    AbDataValue unlocked = {0};
    if (host.full_version &&
        (!host.unlocked ||
         !host.unlocked(host.context, span(script->egg), &unlocked, error)))
        return error && error->message[0]
                   ? false
                   : ab_fail(error, "Missing golden-egg unlock query");
    return (host.full_version && !truthy(unlocked)) ||
           ab_level_remove_loaded_object(level, span(script->object), error);
}

bool ab_level_egg_removed(const AbLevelEggScript *script, const AbDataTable *object,
                          AbLevelEggHost host, AbError *error) {
    if (!script || !script->object || !script->egg || !valid_table(object))
        return ab_fail(error, "Invalid golden-egg removal input");
    AbDataValue name = field(object, "name");
    if (name.kind != AB_DATA_STRING || !equal(name.string, span(script->object)))
        return true;
    if (!host.eagle_bait_launched)
        return ab_fail(error, "Missing current golden-egg eagle flag");
    if (truthy(*host.eagle_bait_launched) || !host.full_version)
        return true;
    return host.achieve ? host.achieve(host.context, span(script->egg), error)
                        : ab_fail(error, "Missing golden-egg achievement host");
}
