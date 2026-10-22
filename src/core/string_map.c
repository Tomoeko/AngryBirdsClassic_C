#include "ab/string_map.h"

#include <stdlib.h>
#include <string.h>

struct AbStringMap {
    AbStringMapSlot *nodes;
    AbStringMapEntry *rehash;
    size_t maximum, count, capacity, last_free;
};

static bool valid_key(AbSpan key) {
    return key.size <= UINT32_MAX && (!key.size || key.data);
}

static bool same_key(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || memcmp(first.data, second.data, first.size) == 0);
}

static size_t main_slot(AbSpan key, size_t capacity) {
    uint32_t hash = (uint32_t)key.size;
    size_t stride = (key.size >> 5) + 1;
    for (size_t offset = key.size; offset >= stride; offset -= stride) {
        hash ^= (hash << 5) + (hash >> 2) + key.data[offset - 1];
    }
    return hash & (capacity - 1);
}

static size_t find_key(const AbStringMap *map, AbSpan key) {
    if (!map->capacity) {
        return SIZE_MAX;
    }
    size_t slot = main_slot(key, map->capacity);
    while (slot != SIZE_MAX) {
        if (map->nodes[slot].key_present && !map->nodes[slot].dead_key &&
            same_key(map->nodes[slot].entry.key, key)) {
            return slot;
        }
        slot = map->nodes[slot].next;
    }
    return SIZE_MAX;
}

static bool capacity_for(size_t count, size_t maximum, size_t *output) {
    size_t capacity = count ? 1 : 0;
    while (capacity < count && capacity < maximum) {
        capacity *= 2;
    }
    if (capacity < count || capacity > maximum) {
        return false;
    }
    *output = capacity;
    return true;
}

bool ab_string_map_reset(AbStringMap *map, size_t hint, AbError *error) {
    size_t capacity;
    if (!map || !capacity_for(hint, map->maximum, &capacity)) {
        return ab_fail(error, "String map hint exceeds bounded storage");
    }
    for (size_t index = 0; index < capacity; ++index) {
        map->nodes[index] = (AbStringMapSlot){.next = SIZE_MAX};
    }
    map->count = 0;
    map->capacity = map->last_free = capacity;
    return true;
}

bool ab_string_map_open(size_t hint, size_t maximum_nodes, size_t allocation_limit,
                        AbStringMap **output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing string map output");
    }
    *output = NULL;
    size_t element_size = sizeof(AbStringMapSlot) + sizeof(AbStringMapEntry);
    if (maximum_nodes > UINT32_C(0x80000000) || (maximum_nodes & (maximum_nodes - 1)) ||
        allocation_limit < sizeof(AbStringMap) ||
        maximum_nodes > (allocation_limit - sizeof(AbStringMap)) / element_size) {
        return ab_fail(error, "Invalid string map allocation bounds");
    }
    AbStringMap *map = calloc(1, sizeof(*map));
    if (!map) {
        return ab_fail(error, "Cannot allocate string map owner");
    }
    map->maximum = maximum_nodes;
    if (maximum_nodes) {
        map->nodes = calloc(maximum_nodes, sizeof(*map->nodes));
        map->rehash = calloc(maximum_nodes, sizeof(*map->rehash));
        if (!map->nodes || !map->rehash) {
            ab_string_map_close(map);
            return ab_fail(error, "Cannot allocate string map workspace");
        }
    }
    if (!ab_string_map_reset(map, hint, error)) {
        ab_string_map_close(map);
        return false;
    }
    *output = map;
    return true;
}

void ab_string_map_close(AbStringMap *map) {
    if (map) {
        free(map->rehash);
        free(map->nodes);
        free(map);
    }
}

size_t ab_string_map_bytes(const AbStringMap *map) {
    return map ? sizeof(*map) +
                     map->maximum * (sizeof(*map->nodes) + sizeof(*map->rehash))
               : 0;
}

static bool place_key(AbStringMap *map, AbSpan key, size_t *output) {
    if (!map->capacity) {
        return false;
    }
    size_t slot = main_slot(key, map->capacity);
    if (map->nodes[slot].entry.present) {
        size_t cursor = map->last_free, free_slot = SIZE_MAX;
        while (cursor) {
            size_t candidate = --cursor;
            if (!map->nodes[candidate].key_present) {
                free_slot = candidate;
                break;
            }
        }
        if (free_slot == SIZE_MAX) {
            return false;
        }
        map->last_free = cursor;
        size_t origin = main_slot(map->nodes[slot].entry.key, map->capacity);
        if (origin != slot) {
            size_t previous = origin;
            while (map->nodes[previous].next != slot) {
                previous = map->nodes[previous].next;
            }
            map->nodes[previous].next = free_slot;
            map->nodes[free_slot] = map->nodes[slot];
            map->nodes[slot] = (AbStringMapSlot){.next = SIZE_MAX};
        } else {
            map->nodes[free_slot].next = map->nodes[slot].next;
            map->nodes[slot].next = free_slot;
            slot = free_slot;
        }
    }
    map->nodes[slot].entry.key = key;
    map->nodes[slot].key_present = true;
    map->nodes[slot].dead_key = false;
    *output = slot;
    return true;
}

static bool rehash(AbStringMap *map) {
    size_t capacity;
    if (!capacity_for(map->count + 1, map->maximum, &capacity)) {
        return false;
    }
    size_t count = 0;
    for (size_t cursor = map->capacity; cursor;) {
        const AbStringMapEntry *entry = &map->nodes[--cursor].entry;
        if (entry->present) {
            map->rehash[count++] = *entry;
        }
    }
    for (size_t index = 0; index < capacity; ++index) {
        map->nodes[index] = (AbStringMapSlot){.next = SIZE_MAX};
    }
    map->capacity = map->last_free = capacity;
    for (size_t index = 0; index < count; ++index) {
        size_t slot;
        if (!place_key(map, map->rehash[index].key, &slot)) {
            return false;
        }
        map->nodes[slot].entry = map->rehash[index];
    }
    return true;
}

bool ab_string_map_set(AbStringMap *map, AbSpan key, uintptr_t value, bool present,
                       AbError *error) {
    if (!map || !valid_key(key)) {
        return ab_fail(error, "Invalid string map key");
    }
    size_t slot = find_key(map, key);
    if (slot == SIZE_MAX && !place_key(map, key, &slot)) {
        if (!rehash(map) || !place_key(map, key, &slot)) {
            return ab_fail(error, "String map mutation exceeds bounded storage");
        }
    }
    map->count -= map->nodes[slot].entry.present;
    map->nodes[slot].entry.value = present ? value : 0;
    map->nodes[slot].entry.present = present;
    map->count += present;
    return true;
}

bool ab_string_map_collect_nil_keys(AbStringMap *map) {
    if (!map) {
        return false;
    }
    for (size_t index = 0; index < map->capacity; ++index) {
        if (map->nodes[index].key_present && !map->nodes[index].entry.present) {
            map->nodes[index].dead_key = true;
        }
    }
    return true;
}

bool ab_string_map_get(const AbStringMap *map, AbSpan key, uintptr_t *value,
                       bool *present, AbError *error) {
    if (!map || !value || !present || !valid_key(key)) {
        return ab_fail(error, "Invalid string map lookup input");
    }
    size_t slot = find_key(map, key);
    bool found = slot != SIZE_MAX && map->nodes[slot].entry.present;
    *value = found ? map->nodes[slot].entry.value : 0;
    *present = found;
    return true;
}

bool ab_string_map_next(const AbStringMap *map, size_t *cursor,
                        AbStringMapEntry *output, AbError *error) {
    if (!map || !cursor || !output || *cursor > map->capacity) {
        return ab_fail(error, "Invalid string map iterator");
    }
    while (*cursor < map->capacity) {
        const AbStringMapEntry *entry = &map->nodes[(*cursor)++].entry;
        if (entry->present) {
            *output = *entry;
            return true;
        }
    }
    *output = (AbStringMapEntry){0};
    return true;
}

bool ab_string_map_resume(const AbStringMap *map, AbSpan key, size_t *cursor,
                          AbError *error) {
    if (!map || !cursor || !valid_key(key) || !map->capacity)
        return ab_fail(error, "Invalid string map iterator resume");
    size_t slot = main_slot(key, map->capacity);
    while (slot != SIZE_MAX) {
        const AbStringMapSlot *node = map->nodes + slot;
        if (node->key_present && same_key(node->entry.key, key) &&
            (!node->dead_key || node->entry.key.data == key.data)) {
            *cursor = slot + 1;
            return true;
        }
        slot = node->next;
    }
    return ab_fail(error, "Invalid removed string map iterator key");
}

bool ab_string_map_layout(const AbStringMap *map, AbStringMapLayout *output) {
    if (!map || !output) {
        return false;
    }
    *output = (AbStringMapLayout){map->count, map->capacity, map->last_free};
    return true;
}

bool ab_string_map_slot(const AbStringMap *map, size_t index, AbStringMapSlot *output) {
    if (!map || !output || index >= map->capacity) {
        return false;
    }
    *output = map->nodes[index];
    return true;
}
