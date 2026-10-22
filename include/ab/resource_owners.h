#ifndef AB_RESOURCE_OWNERS_H
#define AB_RESOURCE_OWNERS_H

#include "ab/sprite_registry.h"

typedef struct {
    AbSpan name;
    const void *resource;
} AbResourceMember;

typedef struct AbResourceSource AbResourceSource;
struct AbResourceSource {
    AbSpriteRegistryKind kind;
    const AbResourceMember *members;
    size_t count;
    void *context;
    /* Retain/release the whole source, including this descriptor and members.
     * Unload releases only a sheet's image, preserving metadata/payload identity.
     * Callbacks cannot fail or reenter/mutate this owner or its registry. */
    void (*retain)(const AbResourceSource *source);
    void (*release)(const AbResourceSource *source);
    void (*unload)(const AbResourceSource *source);
};

typedef struct {
    uint8_t key[256];
    size_t key_size;
    const AbResourceSource *source;
    bool used;
} AbResourceOwnerEntry;

typedef struct {
    AbResourceOwnerEntry *entries;
    size_t count, capacity;
    AbSpriteRegistry *registry;
    bool busy, failed;
} AbResourceOwners;

/* Initialize fresh caller storage, clearing its slots without allocation. Slots
 * never move while in use, preserving registry owner-label views. Storage and
 * registry must stay disjoint and serialized; only these APIs may mutate owner
 * slots/count. Initialization does not release an existing owner. */
bool ab_resource_owners_init(AbResourceOwners *owners, AbResourceOwnerEntry *entries,
                             size_t capacity, AbSpriteRegistry *registry,
                             AbError *error);

/* Native addSpriteSheet/addCompoSpriteSet map/registry behavior. Keys are literal
 * bytes up to 255, copied into the slot; no path parsing occurs here. Source
 * members represent its final native map: unique names and stable payloads.
 * Caller keeps the source alive through this call. Replace an old key by removing
 * every registry occurrence of its members, registering the new source, retaining
 * it, assigning the map slot, then releasing the old source. Retain once per map
 * entry, even for the same source under multiple keys. Capacity failures happen
 * before mutation/callbacks; later errors retain prefix effects and mark failed. */
bool ab_resource_owners_put(AbResourceOwners *owners, AbSpan key,
                            const AbResourceSource *source, AbError *error);

/* Remove registry members for an already-derived key. For a sheet with
 * unload_image=true, retain the owner entry and unload its image. Otherwise erase
 * and release the source; composites always erase. Missing keys are a no-op.
 * PathName key derivation is a separate caller boundary. */
bool ab_resource_owners_remove(AbResourceOwners *owners, AbSpriteRegistryKind kind,
                               AbSpan key, bool unload_image, AbError *error);

/* Borrow the current source until the next owner mutation. No reference transfer,
 * allocation or lookup fallback. Queries may inspect callback-visible state. */
const AbResourceSource *ab_resource_owners_find(const AbResourceOwners *owners,
                                                AbSpriteRegistryKind kind, AbSpan key);

typedef struct {
    AbSpan owner;
    const AbResourceSource *source;
    const AbResourceMember *member;
} AbResourceMemberView;

/* Composite-file loading searches sheet maps in increasing key order and uses
 * the first matching ordinary sprite, independently of the active registry.
 * Query that map policy for the requested source kind. Image unloading does not
 * remove metadata. Borrow the result through the next owner mutation; missing
 * or invalid queries preserve output. No allocation or reference transfer. */
bool ab_resource_owners_find_member(const AbResourceOwners *owners,
                                    AbSpriteRegistryKind kind, AbSpan name,
                                    AbResourceMemberView *output);

/* Teardown utility: remove member registrations and release every map reference.
 * Slot order is explicit here; native Resources destructor ordering is separate.
 * The registry and any foreign sources remain borrowed. Stop on failure. */
bool ab_resource_owners_clear(AbResourceOwners *owners, AbError *error);

#endif
