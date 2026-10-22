#ifndef AB_SPRITE_REGISTRY_H
#define AB_SPRITE_REGISTRY_H

#include "ab/base.h"

typedef enum {
    AB_SPRITE_REGISTRY_ANY = 0,
    AB_SPRITE_REGISTRY_SPRITE = 1,
    AB_SPRITE_REGISTRY_COMPOSITE = 2
} AbSpriteRegistryKind;

typedef struct {
    AbSpan name, owner;
    AbSpriteRegistryKind kind;
    const void *resource;
} AbSpriteRegistryEntry;

typedef struct {
    AbSpriteRegistryEntry *entries;
    size_t count, capacity;
} AbSpriteRegistry;

/* Initialize with caller storage and count zero. Strings and payloads stay
 * borrowed until removal/reset; storage cannot alias inputs/outputs. Serialize
 * calls and do not mutate retained entries externally. No allocation occurs.
 * These are native Resources registry operations, without source-set ownership,
 * GPU work or sheet/composite decoding. A batch caller preserves its native
 * source map's last definition for each name before registering resources. */
bool ab_sprite_registry_add(AbSpriteRegistry *registry, AbSpriteRegistryEntry entry,
                            AbError *error);

/* Remove every occurrence of the named payload and type, across owner labels.
 * Other bindings retain order, exposing any earlier binding. Resource identity
 * comes from its owning source set; removal does not destroy that resource.
 * Invalid input preserves the registry. Repeated registration is not deduplicated. */
bool ab_sprite_registry_remove(AbSpriteRegistry *registry, AbSpan name,
                               AbSpriteRegistryKind kind, const void *resource,
                               AbError *error);

/* Select the most recently registered name. A specific type must match that top
 * binding; it never searches past a different type. Empty names are valid. A
 * missing/invalid query leaves output unchanged. Output borrows retained inputs. */
bool ab_sprite_registry_find(const AbSpriteRegistry *registry, AbSpan name,
                             AbSpriteRegistryKind kind, AbSpriteRegistryEntry *output);

#endif
