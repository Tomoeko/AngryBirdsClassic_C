#ifndef AB_TEST_CLASSIC_BALLOONS_H
#define AB_TEST_CLASSIC_BALLOONS_H

#include "ab/classic_balloons.h"

/* Controlled creator calls use one real surviving physical table; the
 * initializer itself correctly selects zero balloons in Level1. */
static bool lifecycle_balloon_edges(ShotTest *shot, AbClassicBalloons *balloons,
                                    AbStringMap *dead, AbError *error) {
    AbClassicLevelView level = shot->level;
    size_t object = 0;
    while (object < level.bodies->count && level.removed_objects[object])
        ++object;
    CHECK(object < level.bodies->count);
    AbSpan name = level.bodies->objects[object].name;
    AbDataTable *physical = &level.properties->objects[object].physical;
    AbDataValue saved_y = field(physical, "y");
    uintptr_t prior = 0;
    bool present = false;
    CHECK(
        ab_string_map_get(dead, name, &prior, &present, error) && !present &&
        ab_classic_balloons_created(balloons, object, error) &&
        ab_level_property_set(level.properties, object, span("y"),
                              (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(-200)},
                              error) &&
        ab_classic_balloons_update(balloons, error) &&
        ab_string_map_get(dead, name, &prior, &present, error) && !present &&
        ab_level_property_set(level.properties, object, span("y"),
                              (AbDataValue){.kind = AB_DATA_NUMBER,
                                            .bits = bits(nextafterf(-200, -INFINITY))},
                              error) &&
        ab_classic_balloons_update(balloons, error) &&
        ab_string_map_get(dead, name, &prior, &present, error) && present);
    const AbDataValue *marked = (const AbDataValue *)prior;
    CHECK(marked && marked->kind == AB_DATA_TABLE && marked->table == physical &&
          ab_string_map_set(dead, name, 0, false, error) &&
          ab_level_property_set(
              level.properties, object, span("y"),
              (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(NAN)}, error) &&
          ab_classic_balloons_update(balloons, error) &&
          ab_string_map_get(dead, name, &prior, &present, error) && !present);
    AbDataEntry *world = NULL;
    for (size_t index = 0; index < level.properties->world.count; ++index) {
        AbDataEntry *entry = level.properties->world.entries + index;
        if (damage_name_equal(entry->key,
                              (AbDataValue){.kind = AB_DATA_STRING, .string = name}))
            world = entry;
    }
    CHECK(world);
    AbDataValue saved_world = world->value;
    world->value = (AbDataValue){0};
    CHECK(ab_classic_balloons_update(balloons, error) &&
          ab_string_map_get(ab_classic_balloons_members(balloons), name, &prior,
                            &present, error) &&
          !present);
    world->value = saved_world;
    CHECK(ab_classic_balloons_created(balloons, object, error) &&
          ab_level_property_set(level.properties, object, span("y"),
                                (AbDataValue){.kind = AB_DATA_BOOLEAN}, error));
    AbError late = {0};
    CHECK(!ab_classic_balloons_update(balloons, &late) &&
          strstr(late.message, "Nonnumeric canonical balloon height") &&
          !ab_classic_balloons_update(balloons, NULL) &&
          !ab_classic_balloons_created(balloons, object, NULL) &&
          ab_level_property_set(level.properties, object, span("y"), saved_y, error));
    return true;
}

#endif
