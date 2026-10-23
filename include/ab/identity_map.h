#ifndef AB_IDENTITY_MAP_H
#define AB_IDENTITY_MAP_H

#include "ab/base.h"

typedef struct AbIdentityMap AbIdentityMap;

typedef struct {
    uintptr_t key, value;
    bool present;
} AbIdentityMapEntry;

typedef struct {
    size_t count, capacity, last_free;
} AbIdentityMapLayout;

typedef struct {
    AbIdentityMapEntry entry;
    size_t next;
    bool key_present;
} AbIdentityMapSlot;

/* Hash-only identity dictionary for retained non-null table identities. The
 * pinned ARMv7 main-position calculation uses raw low 32 pointer bits modulo
 * ((capacity-1)|1); identity equality preserves all host bits. Keys are opaque
 * and never dereferenced. Callers retain identities until reset/close.
 * Collision relocation, descending free search/reinsertion and ascending next
 * mirror the target table-key path. This is a data structure, not a Lua VM.
 * Array/mixed keys, key collection and allocator/address parity are separate.
 *
 * Allocate bounded node/rehash storage once. maximum_nodes is a power of two
 * or zero, hint rounds up. Mutations allocate nothing; nil writes retain keys.
 * present zero values remain live. Unsupported inputs/capacity failure preserve
 * the map and outputs. All calls are serialized. */
bool ab_identity_map_open(size_t hint, size_t maximum_nodes, size_t allocation_limit,
                          AbIdentityMap **output, AbError *error);
bool ab_identity_map_reset(AbIdentityMap *map, size_t hint, AbError *error);
void ab_identity_map_close(AbIdentityMap *map);
size_t ab_identity_map_bytes(const AbIdentityMap *map);
bool ab_identity_map_set(AbIdentityMap *map, uintptr_t key, uintptr_t value,
                         bool present, AbError *error);
bool ab_identity_map_get(const AbIdentityMap *map, uintptr_t key, uintptr_t *value,
                         bool *present, AbError *error);
bool ab_identity_map_next(const AbIdentityMap *map, size_t *cursor,
                          AbIdentityMapEntry *output, AbError *error);
/* Resume after the retained current key's present value may have been erased
 * or callbacks may have rehashed. Discarded identities fail without restarting
 * iteration or suppressing revisits caused by native growth. */
bool ab_identity_map_resume(const AbIdentityMap *map, uintptr_t key, size_t *cursor,
                            AbError *error);
bool ab_identity_map_layout(const AbIdentityMap *map, AbIdentityMapLayout *output);
bool ab_identity_map_slot(const AbIdentityMap *map, size_t index,
                          AbIdentityMapSlot *output);

#endif
