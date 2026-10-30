#ifndef AB_INTEGER_MAP_H
#define AB_INTEGER_MAP_H

#include "ab/base.h"

typedef struct AbIntegerMap AbIntegerMap;

typedef struct {
    int32_t key;
    uintptr_t value;
    bool present;
} AbIntegerMapEntry;

typedef struct {
    size_t count, array_capacity, hash_capacity;
} AbIntegerMapLayout;

/* Native binary32 integer-key table layout with continuing insertions, nil
 * writes, deletions and array/hash rehashing. Keys are exact in [-2^24,2^24].
 * Opaque handles and all iterator storage remain caller-owned. Each part is
 * bounded by the same power-of-two maximum; mutation never allocates. No mixed
 * keys, VM, metatables or collection. A nil hash key remains until reuse/rehash.
 * Numeric keys have no collector-dependent identity. */
bool ab_integer_map_open(size_t maximum, size_t bytes, AbIntegerMap **output,
                         AbError *error);
void ab_integer_map_close(AbIntegerMap *map);
size_t ab_integer_map_bytes(const AbIntegerMap *map);
bool ab_integer_map_reset(AbIntegerMap *map, AbError *error);
bool ab_integer_map_set(AbIntegerMap *map, int32_t key, uintptr_t value, bool present,
                        AbError *error);
bool ab_integer_map_get(const AbIntegerMap *map, int32_t key, uintptr_t *value,
                        bool *present, AbError *error);
/* Cursor starts at zero. Delete the returned entry without inserting or
 * rehashing during traversal. End has present=false. */
bool ab_integer_map_next(const AbIntegerMap *map, size_t *cursor,
                         AbIntegerMapEntry *entry, AbError *error);
bool ab_integer_map_layout(const AbIntegerMap *map, AbIntegerMapLayout *output);

#endif
