#include "ab/classic_scores.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct AbClassicScores {
    AbClassicScoreConfig config;
    AbClassicLevelView level;
    AbStringMap *map;
    size_t bytes;
    bool updating;
};

static bool equal(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || (first.data && second.data &&
                            !memcmp(first.data, second.data, first.size)));
}

bool ab_classic_scores_open(AbClassicScoreConfig config, AbClassicScores **output,
                            AbError *error) {
    if (!output)
        return ab_fail(error, "Missing owned score table output");
    *output = NULL;
    AbClassicLevelView level = ab_classic_level_view(config.level);
    if (!level.bodies || !config.blocks || !config.birds || !config.objects ||
        config.count != level.bodies->count || config.count > SIZE_MAX - 2 ||
        config.storage_bytes < sizeof(AbClassicScores))
        return ab_fail(error, "Invalid owned score table bindings");
    for (size_t index = 0; index < config.count; ++index)
        if (config.objects[index].present)
            return ab_fail(error,
                           "Owned score table needs fresh per-object membership");
    size_t maximum = 1;
    while (maximum < config.count + 2 && maximum <= SIZE_MAX / 2)
        maximum *= 2;
    if (maximum < config.count + 2)
        return ab_fail(error, "Owned score table capacity overflows");
    AbClassicScores *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate owned score table");
    owner->config = config;
    owner->level = level;
    if (!ab_string_map_open(0, maximum, config.storage_bytes - sizeof(*owner),
                            &owner->map, error) ||
        !ab_string_map_set(owner->map, (AbSpan){(const uint8_t *)"blocks", 6},
                           (uintptr_t)&config.blocks->total, true, error) ||
        !ab_string_map_set(owner->map, (AbSpan){(const uint8_t *)"birds", 5},
                           (uintptr_t)config.birds, true, error)) {
        ab_classic_scores_close(owner);
        return false;
    }
    owner->bytes = sizeof(*owner) + ab_string_map_bytes(owner->map);
    *output = owner;
    return true;
}

void ab_classic_scores_close(AbClassicScores *owner) {
    if (owner) {
        ab_string_map_close(owner->map);
        free(owner);
    }
}

size_t ab_classic_scores_bytes(const AbClassicScores *owner) {
    return owner ? owner->bytes : 0;
}

AbClassicScoreView ab_classic_scores_view(const AbClassicScores *owner) {
    return owner ? (AbClassicScoreView){owner->config.blocks, owner->config.birds,
                                        owner->config.objects, owner->config.count}
                 : (AbClassicScoreView){0};
}

bool ab_classic_scores_entry(AbClassicScores *owner, AbSpan name,
                             AbBirdCollisionScoreEntry **output, AbError *error) {
    if (!owner || !output || owner->updating || (name.size && !name.data))
        return ab_fail(error, "Invalid owned score entry request");
    size_t index = 0;
    while (index < owner->config.count &&
           !equal(name, owner->level.bodies->objects[index].name))
        ++index;
    if (index == owner->config.count)
        return ab_fail(error, "Unknown owned score entry name");
    AbBirdCollisionScoreEntry *entry = owner->config.objects + index;
    uintptr_t value = 0;
    bool present = false;
    if (!ab_string_map_get(owner->map, name, &value, &present, error))
        return false;
    if (present) {
        if (!entry->present || value != (uintptr_t)&entry->score)
            return ab_fail(error, "Rebound or reserved owned score entry");
    } else {
        if (entry->present)
            return ab_fail(error, "Unregistered owned score entry presence");
        if (!ab_string_map_set(owner->map, name, (uintptr_t)&entry->score, true, error))
            return false;
        entry->present = true;
        entry->score = 0;
    }
    *output = entry;
    return true;
}

bool ab_classic_scores_update(AbClassicScores *owner, float *total, AbError *error) {
    if (!owner || !total || owner->updating)
        return ab_fail(error, "Invalid or reentrant owned score update");
    owner->updating = true;
    *total = 0;
    size_t cursor = 0;
    bool okay = true;
    for (;;) {
        AbStringMapEntry entry;
        if (!ab_string_map_next(owner->map, &cursor, &entry, error)) {
            okay = false;
            break;
        }
        if (!entry.present)
            break;
        if (!entry.value || !isfinite(*(const float *)entry.value)) {
            okay = ab_fail(error, "Unsupported owned numeric score entry");
            break;
        }
        *total = *total + *(const float *)entry.value;
        if (!isfinite(*total)) {
            okay = ab_fail(error, "Owned score sum exceeds finite range");
            break;
        }
    }
    owner->updating = false;
    return okay;
}
