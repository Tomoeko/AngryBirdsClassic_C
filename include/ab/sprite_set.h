#ifndef AB_SPRITE_SET_H
#define AB_SPRITE_SET_H

#include "ab/sprite.h"
#include "ab/sprite_registry.h"
#include "ab/zip.h"

typedef struct AbSpriteSet AbSpriteSet;

typedef struct {
    AbSpan metadata_path;
    const AbSpriteSheet *sheet;
} AbSpriteSheetView;

typedef struct {
    AbSpan metadata_path;
    const AbSpriteSheet *sheet;
    const AbSpriteRegion *region;
} AbSpriteBinding;

/* Retain parsed SPRT metadata from an explicit ordered list of APK entries.
 * No image decoding, GPU objects or resource-group selection occurs. The byte
 * limit bounds the owner, sheet array, retained DAT bytes and region arrays.
 * Archive bytes remain borrowed for metadata_path until close; other returned
 * spans/regions borrow this owner. Paths are used only during open. All partial
 * allocations are released and output is NULL on failure. */
bool ab_sprite_set_open(const AbZip *archive, const char *const *paths, size_t count,
                        size_t allocation_limit, AbSpriteSet **output, AbError *error);
void ab_sprite_set_close(AbSpriteSet *set);

/* Enumerate the explicit input order. Views borrow the same owners as find;
 * invalid indices preserve output. Neither operation allocates. */
size_t ab_sprite_set_count(const AbSpriteSet *set);
size_t ab_sprite_set_bytes(const AbSpriteSet *set);
bool ab_sprite_set_sheet(const AbSpriteSet *set, size_t index,
                         AbSpriteSheetView *output);

/* Search sheets from last to first, using each sheet's last named definition.
 * This is an explicit ordered-set policy, not recovery of native resource-group
 * load order. Output remains unchanged when missing/invalid. No allocation. */
bool ab_sprite_set_find(const AbSpriteSet *set, AbSpan name, AbSpriteBinding *output);

/* Register the source sheet's final named regions in the native-matched registry.
 * Payload identities are the owned AbSpriteRegion pointers. Registry storage and
 * owner-label bytes remain caller-owned; both this set and those bytes must
 * outlive registrations. Preflight capacity before mutation; repeated calls add
 * repeated bindings. Source-set key replacement/ownership is separate. */
bool ab_sprite_set_register_sheet(const AbSpriteSet *set, size_t index, AbSpan owner,
                                  AbSpriteRegistry *registry, AbError *error);
bool ab_sprite_set_unregister_sheet(const AbSpriteSet *set, size_t index,
                                    AbSpriteRegistry *registry, AbError *error);

/* Resolve only the registry's active ordinary-sprite binding and require that its
 * region belongs to this set. A composite or a foreign set masks older entries.
 * No allocation; returned views have the same lifetime as sheet/find views. */
bool ab_sprite_set_find_registered(const AbSpriteSet *set,
                                   const AbSpriteRegistry *registry, AbSpan name,
                                   AbSpriteBinding *output);

#endif
