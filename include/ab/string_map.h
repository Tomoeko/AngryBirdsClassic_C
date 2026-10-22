#ifndef AB_STRING_MAP_H
#define AB_STRING_MAP_H

#include "ab/base.h"

typedef struct AbStringMap AbStringMap;

typedef struct {
    AbSpan key;
    uintptr_t value;
    bool present;
} AbStringMapEntry;

typedef struct {
    size_t count, capacity, last_free;
} AbStringMapLayout;

typedef struct {
    AbStringMapEntry entry;
    size_t next;
    bool key_present, dead_key;
} AbStringMapSlot;

/* String-key dictionary with explicit absent values and opaque value handles.
 * Key bytes stay borrowed and pinned for the map's lifetime. Allocate bounded
 * node/rehash storage once; hint rounds to a power of two and maximum_nodes must
 * already be a power of two (zero is allowed). Mutation never allocates. This
 * is a data structure, without a VM, metatables, array/numeric keys or GC.
 * Deleted keys remain until overwritten or rehashed. Nil writes may reserve a
 * key even when no live value exists. Native comparison covers continuing
 * string-only mutations with pinned keys. Collection scheduling and mixed key
 * types remain separate. See the evidence ledger for measured coverage. */
bool ab_string_map_open(size_t hint, size_t maximum_nodes, size_t allocation_limit,
                        AbStringMap **output, AbError *error);
bool ab_string_map_reset(AbStringMap *map, size_t hint, AbError *error);
void ab_string_map_close(AbStringMap *map);
/* Retained owner/node/rehash bytes, excluding borrowed key storage. */
size_t ab_string_map_bytes(const AbStringMap *map);

/* present=false deletes a value, retaining its key/node. A present zero handle
 * remains a live value. Invalid input/capacity failure preserves the map; other
 * outputs remain unchanged on failure. Key length must fit uint32_t. */
bool ab_string_map_set(AbStringMap *map, AbSpan key, uintptr_t value, bool present,
                       AbError *error);
/* Explicit collection traversal phase: retained nil-valued string keys become
 * dead keys, including pinned strings. Later lookup skips them, while free-slot
 * search still treats them as used. This does not implement a VM, roots, memory
 * reclamation, allocation timing or collector scheduling. No allocation. */
bool ab_string_map_collect_nil_keys(AbStringMap *map);
bool ab_string_map_get(const AbStringMap *map, AbSpan key, uintptr_t *value,
                       bool *present, AbError *error);

/* Iterate live values in physical slot order with a caller-owned cursor,
 * starting at zero. Missing output has present=false. Cursor must not exceed
 * capacity. Removing the returned key between calls preserves slot traversal;
 * insertion/rehash may change it. No allocation. Layout/slot inspection exposes
 * key retention and collision links for native comparison; SIZE_MAX ends links. */
bool ab_string_map_next(const AbStringMap *map, size_t *cursor,
                        AbStringMapEntry *output, AbError *error);
/* Resume native string-key next after a callback may delete/rehash its current
 * key. Find the retained key in its current hash chain, then scan after its new
 * slot. Nil keys remain valid while retained. An explicitly collected dead key
 * requires the same pinned key bytes/identity returned by this map. A discarded
 * key fails without changing cursor; never restart iteration or deduplicate
 * revisits after growth. No allocation or table mutation. */
bool ab_string_map_resume(const AbStringMap *map, AbSpan key, size_t *cursor,
                          AbError *error);
bool ab_string_map_layout(const AbStringMap *map, AbStringMapLayout *output);
bool ab_string_map_slot(const AbStringMap *map, size_t index, AbStringMapSlot *output);

#endif
