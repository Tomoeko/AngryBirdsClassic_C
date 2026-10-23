#include "ab/identity_map.h"

#include <stdlib.h>
#include <string.h>

struct AbIdentityMap {
    AbIdentityMapSlot *nodes;
    AbIdentityMapEntry *rehash;
    size_t maximum, count, capacity, last_free;
};

static bool valid_key(uintptr_t key) {
    return key != 0;
}

static bool same_key(uintptr_t first, uintptr_t second) {
    return first == second;
}

static size_t main_slot(uintptr_t key, size_t capacity) {
    /* The pinned 32-bit table-key path hashes the raw pointer, without a shift.
     * Host identity comparison retains all bits; only the hash narrows. */
    return (uint32_t)key % ((capacity - 1) | 1);
}

static size_t find_key(const AbIdentityMap *map, uintptr_t key) {
    if (!map->capacity) {
        return SIZE_MAX;
    }
    size_t slot = main_slot(key, map->capacity);
    while (slot != SIZE_MAX) {
        if (map->nodes[slot].key_present && same_key(map->nodes[slot].entry.key, key)) {
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

bool ab_identity_map_reset(AbIdentityMap *map, size_t hint, AbError *error) {
    size_t capacity;
    if (!map || !capacity_for(hint, map->maximum, &capacity)) {
        return ab_fail(error, "Identity map hint exceeds bounded storage");
    }
    for (size_t index = 0; index < capacity; ++index) {
        map->nodes[index] = (AbIdentityMapSlot){.next = SIZE_MAX};
    }
    map->count = 0;
    map->capacity = map->last_free = capacity;
    return true;
}

bool ab_identity_map_open(size_t hint, size_t maximum_nodes, size_t allocation_limit,
                          AbIdentityMap **output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing identity map output");
    }
    *output = NULL;
    size_t element_size = sizeof(AbIdentityMapSlot) + sizeof(AbIdentityMapEntry);
    if (maximum_nodes > UINT32_C(0x80000000) || (maximum_nodes & (maximum_nodes - 1)) ||
        allocation_limit < sizeof(AbIdentityMap) ||
        maximum_nodes > (allocation_limit - sizeof(AbIdentityMap)) / element_size) {
        return ab_fail(error, "Invalid identity map allocation bounds");
    }
    AbIdentityMap *map = calloc(1, sizeof(*map));
    if (!map) {
        return ab_fail(error, "Cannot allocate identity map owner");
    }
    map->maximum = maximum_nodes;
    if (maximum_nodes) {
        map->nodes = calloc(maximum_nodes, sizeof(*map->nodes));
        map->rehash = calloc(maximum_nodes, sizeof(*map->rehash));
        if (!map->nodes || !map->rehash) {
            ab_identity_map_close(map);
            return ab_fail(error, "Cannot allocate identity map workspace");
        }
    }
    if (!ab_identity_map_reset(map, hint, error)) {
        ab_identity_map_close(map);
        return false;
    }
    *output = map;
    return true;
}

void ab_identity_map_close(AbIdentityMap *map) {
    if (map) {
        free(map->rehash);
        free(map->nodes);
        free(map);
    }
}

size_t ab_identity_map_bytes(const AbIdentityMap *map) {
    return map ? sizeof(*map) +
                     map->maximum * (sizeof(*map->nodes) + sizeof(*map->rehash))
               : 0;
}

static bool place_key(AbIdentityMap *map, uintptr_t key, size_t *output) {
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
            map->nodes[slot] = (AbIdentityMapSlot){.next = SIZE_MAX};
        } else {
            map->nodes[free_slot].next = map->nodes[slot].next;
            map->nodes[slot].next = free_slot;
            slot = free_slot;
        }
    }
    map->nodes[slot].entry.key = key;
    map->nodes[slot].key_present = true;
    *output = slot;
    return true;
}

static bool rehash(AbIdentityMap *map) {
    size_t capacity;
    if (!capacity_for(map->count + 1, map->maximum, &capacity)) {
        return false;
    }
    size_t count = 0;
    for (size_t cursor = map->capacity; cursor;) {
        const AbIdentityMapEntry *entry = &map->nodes[--cursor].entry;
        if (entry->present) {
            map->rehash[count++] = *entry;
        }
    }
    for (size_t index = 0; index < capacity; ++index) {
        map->nodes[index] = (AbIdentityMapSlot){.next = SIZE_MAX};
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

bool ab_identity_map_set(AbIdentityMap *map, uintptr_t key, uintptr_t value,
                         bool present, AbError *error) {
    if (!map || !valid_key(key)) {
        return ab_fail(error, "Invalid identity map key");
    }
    size_t slot = find_key(map, key);
    if (slot == SIZE_MAX && !place_key(map, key, &slot)) {
        if (!rehash(map) || !place_key(map, key, &slot)) {
            return ab_fail(error, "Identity map mutation exceeds bounded storage");
        }
    }
    map->count -= map->nodes[slot].entry.present;
    map->nodes[slot].entry.value = present ? value : 0;
    map->nodes[slot].entry.present = present;
    map->count += present;
    return true;
}

bool ab_identity_map_get(const AbIdentityMap *map, uintptr_t key, uintptr_t *value,
                         bool *present, AbError *error) {
    if (!map || !value || !present || !valid_key(key)) {
        return ab_fail(error, "Invalid identity map lookup input");
    }
    size_t slot = find_key(map, key);
    bool found = slot != SIZE_MAX && map->nodes[slot].entry.present;
    *value = found ? map->nodes[slot].entry.value : 0;
    *present = found;
    return true;
}

bool ab_identity_map_next(const AbIdentityMap *map, size_t *cursor,
                          AbIdentityMapEntry *output, AbError *error) {
    if (!map || !cursor || !output || *cursor > map->capacity) {
        return ab_fail(error, "Invalid identity map iterator");
    }
    while (*cursor < map->capacity) {
        const AbIdentityMapEntry *entry = &map->nodes[(*cursor)++].entry;
        if (entry->present) {
            *output = *entry;
            return true;
        }
    }
    *output = (AbIdentityMapEntry){0};
    return true;
}

bool ab_identity_map_resume(const AbIdentityMap *map, uintptr_t key, size_t *cursor,
                            AbError *error) {
    if (!map || !cursor || !valid_key(key) || !map->capacity)
        return ab_fail(error, "Invalid identity map iterator resume");
    size_t slot = main_slot(key, map->capacity);
    while (slot != SIZE_MAX) {
        const AbIdentityMapSlot *node = map->nodes + slot;
        if (node->key_present && same_key(node->entry.key, key)) {
            *cursor = slot + 1;
            return true;
        }
        slot = node->next;
    }
    return ab_fail(error, "Invalid removed identity map iterator key");
}

bool ab_identity_map_layout(const AbIdentityMap *map, AbIdentityMapLayout *output) {
    if (!map || !output) {
        return false;
    }
    *output = (AbIdentityMapLayout){map->count, map->capacity, map->last_free};
    return true;
}

bool ab_identity_map_slot(const AbIdentityMap *map, size_t index,
                          AbIdentityMapSlot *output) {
    if (!map || !output || index >= map->capacity) {
        return false;
    }
    *output = map->nodes[index];
    return true;
}
