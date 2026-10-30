#include "ab/integer_map.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    AbIntegerMapEntry entry;
    size_t next;
    bool used;
} IntegerNode;

struct AbIntegerMap {
    AbIntegerMapEntry *array, *rehash;
    IntegerNode *nodes;
    size_t maximum, count, array_capacity, hash_capacity, last_free;
};

static bool valid_key(int32_t key) {
    return key >= -16777216 && key <= 16777216;
}

static size_t main_slot(int32_t key, size_t capacity) {
    float number = (float)key;
    uint32_t word;
    memcpy(&word, &number, sizeof(word));
    return key ? word % ((capacity - 1) | 1) : 0;
}

static bool array_key(int32_t key, size_t capacity) {
    return key > 0 && (size_t)key <= capacity;
}

static size_t find_key(const AbIntegerMap *map, int32_t key) {
    size_t slot = map->hash_capacity ? main_slot(key, map->hash_capacity) : SIZE_MAX;
    while (slot != SIZE_MAX) {
        if (map->nodes[slot].used && map->nodes[slot].entry.key == key)
            return slot;
        slot = map->nodes[slot].next;
    }
    return SIZE_MAX;
}

static bool place_key(AbIntegerMap *map, int32_t key, size_t *output) {
    if (!map->hash_capacity)
        return false;
    size_t slot = main_slot(key, map->hash_capacity);
    if (map->nodes[slot].entry.present) {
        size_t cursor = map->last_free, free_slot = SIZE_MAX;
        while (cursor) {
            size_t candidate = --cursor;
            if (!map->nodes[candidate].used) {
                free_slot = candidate;
                break;
            }
        }
        if (free_slot == SIZE_MAX)
            return false;
        map->last_free = cursor;
        size_t origin = main_slot(map->nodes[slot].entry.key, map->hash_capacity);
        if (origin != slot) {
            size_t previous = origin;
            while (map->nodes[previous].next != slot)
                previous = map->nodes[previous].next;
            map->nodes[previous].next = free_slot;
            map->nodes[free_slot] = map->nodes[slot];
            map->nodes[slot] = (IntegerNode){.next = SIZE_MAX};
        } else {
            map->nodes[free_slot].next = map->nodes[slot].next;
            map->nodes[slot].next = free_slot;
            slot = free_slot;
        }
    }
    map->nodes[slot].entry.key = key;
    map->nodes[slot].used = true;
    *output = slot;
    return true;
}

static bool rebuild(AbIntegerMap *map, int32_t new_key, AbError *error) {
    size_t count = 0;
    for (size_t index = 0; index < map->array_capacity; ++index)
        if (map->array[index].present)
            map->rehash[count++] = map->array[index];
    /* Native rehash visits the old hash part backwards, after the array part. */
    for (size_t cursor = map->hash_capacity; cursor;)
        if (map->nodes[--cursor].entry.present)
            map->rehash[count++] = map->nodes[cursor].entry;
    size_t array_capacity = 0, array_members = 0;
    for (size_t capacity = 1; capacity <= UINT32_C(0x1000000); capacity *= 2) {
        size_t members = array_key(new_key, capacity);
        for (size_t index = 0; index < count; ++index)
            members += array_key(map->rehash[index].key, capacity);
        if (members > capacity / 2) {
            array_capacity = capacity;
            array_members = members;
        }
    }
    size_t hash_members = count + 1 - array_members;
    size_t hash_capacity = hash_members ? 1 : 0;
    while (hash_capacity < hash_members)
        hash_capacity *= 2;
    if (array_capacity > map->maximum || hash_capacity > map->maximum)
        return ab_fail(error, "Integer table mutation exceeds bounded storage");
    memset(map->array, 0, array_capacity * sizeof(*map->array));
    for (size_t index = 0; index < hash_capacity; ++index)
        map->nodes[index] = (IntegerNode){.next = SIZE_MAX};
    map->array_capacity = array_capacity;
    map->hash_capacity = map->last_free = hash_capacity;
    for (size_t index = 0; index < count; ++index) {
        AbIntegerMapEntry entry = map->rehash[index];
        if (array_key(entry.key, array_capacity)) {
            map->array[(size_t)entry.key - 1] = entry;
        } else {
            size_t slot;
            if (!place_key(map, entry.key, &slot))
                return ab_fail(error, "Integer table rehash lost storage");
            map->nodes[slot].entry = entry;
        }
    }
    return true;
}

bool ab_integer_map_open(size_t maximum, size_t bytes, AbIntegerMap **output,
                         AbError *error) {
    if (!output)
        return ab_fail(error, "Missing integer table output");
    *output = NULL;
    size_t per_slot = 3 * sizeof(AbIntegerMapEntry) + sizeof(IntegerNode);
    if (!maximum || maximum > UINT32_C(0x1000000) || (maximum & (maximum - 1)) ||
        bytes < sizeof(AbIntegerMap) ||
        maximum > (bytes - sizeof(AbIntegerMap)) / per_slot)
        return ab_fail(error, "Invalid integer table allocation bounds");
    AbIntegerMap *map = calloc(1, sizeof(*map));
    if (!map)
        return ab_fail(error, "Cannot allocate integer table");
    map->maximum = maximum;
    map->array = calloc(maximum, sizeof(*map->array));
    map->nodes = calloc(maximum, sizeof(*map->nodes));
    map->rehash = calloc(maximum * 2, sizeof(*map->rehash));
    if (!map->array || !map->nodes || !map->rehash) {
        ab_integer_map_close(map);
        return ab_fail(error, "Cannot allocate integer table workspace");
    }
    *output = map;
    return true;
}

void ab_integer_map_close(AbIntegerMap *map) {
    if (map) {
        free(map->array);
        free(map->nodes);
        free(map->rehash);
        free(map);
    }
}

size_t ab_integer_map_bytes(const AbIntegerMap *map) {
    return map ? sizeof(*map) + map->maximum * (3 * sizeof(AbIntegerMapEntry) +
                                                sizeof(IntegerNode))
               : 0;
}

bool ab_integer_map_reset(AbIntegerMap *map, AbError *error) {
    if (!map)
        return ab_fail(error, "Missing integer table");
    map->count = map->array_capacity = map->hash_capacity = map->last_free = 0;
    return true;
}

bool ab_integer_map_set(AbIntegerMap *map, int32_t key, uintptr_t value, bool present,
                        AbError *error) {
    if (!map || !valid_key(key))
        return ab_fail(error, "Invalid integer table key");
    AbIntegerMapEntry *entry;
    if (array_key(key, map->array_capacity)) {
        entry = map->array + (size_t)key - 1;
    } else {
        size_t slot = find_key(map, key);
        if (slot == SIZE_MAX && !place_key(map, key, &slot)) {
            if (!rebuild(map, key, error))
                return false;
            if (array_key(key, map->array_capacity))
                return ab_integer_map_set(map, key, value, present, error);
            if (!place_key(map, key, &slot))
                return ab_fail(error, "Integer table insertion lost storage");
        }
        entry = &map->nodes[slot].entry;
    }
    map->count -= entry->present;
    *entry = (AbIntegerMapEntry){key, present ? value : 0, present};
    map->count += present;
    return true;
}

bool ab_integer_map_get(const AbIntegerMap *map, int32_t key, uintptr_t *value,
                        bool *present, AbError *error) {
    if (!map || !valid_key(key) || !value || !present)
        return ab_fail(error, "Invalid integer table lookup");
    const AbIntegerMapEntry *entry = NULL;
    if (array_key(key, map->array_capacity))
        entry = map->array + (size_t)key - 1;
    else {
        size_t slot = find_key(map, key);
        if (slot != SIZE_MAX)
            entry = &map->nodes[slot].entry;
    }
    *present = entry && entry->present;
    *value = *present ? entry->value : 0;
    return true;
}

bool ab_integer_map_next(const AbIntegerMap *map, size_t *cursor,
                         AbIntegerMapEntry *entry, AbError *error) {
    if (!map || !cursor || !entry || *cursor > map->array_capacity + map->hash_capacity)
        return ab_fail(error, "Invalid integer table iterator");
    while (*cursor < map->array_capacity + map->hash_capacity) {
        size_t index = (*cursor)++;
        *entry = index < map->array_capacity
                     ? map->array[index]
                     : map->nodes[index - map->array_capacity].entry;
        if (entry->present)
            return true;
    }
    *entry = (AbIntegerMapEntry){0};
    return true;
}

bool ab_integer_map_layout(const AbIntegerMap *map, AbIntegerMapLayout *output) {
    if (!map || !output)
        return false;
    *output = (AbIntegerMapLayout){map->count, map->array_capacity, map->hash_capacity};
    return true;
}
