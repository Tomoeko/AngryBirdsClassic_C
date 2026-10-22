#ifndef AB_GAME_OBJECT_REMOVE_H
#define AB_GAME_OBJECT_REMOVE_H

#include "ab/game_collision.h"
#include "ab/physics_world.h"
#include "ab/string_map.h"

typedef struct {
    AbSpan name, rolling_sound;
    size_t body;
    AbGameColliderType collider;
    bool camera_animation;
    uintptr_t renderable;
    bool delayed_destroy;
    float destroy_timer;
} AbGameObjectCore;

typedef struct {
    AbPhysicsWorld *world;
    /* Non-null borrowed AbGameObjectCore handles. Rolling values are raw
     * binary32 words in uintptr_t; present positive zero is a live cache entry. */
    AbStringMap *objects, *rolling_levels;
    AbGameObjectCore **birds;
    size_t bird_count, bird_capacity;
} AbGameObjectCoreRegistry;

typedef struct {
    void *context;
    bool (*close_camera_animation)(void *context, AbSpan current_object_name,
                                   AbError *error);
    void (*retain_renderable)(void *context, uintptr_t renderable);
    bool (*remove_renderable)(void *context, uintptr_t retained_renderable,
                              AbError *error);
    void (*release_renderable)(void *context, uintptr_t renderable);
    /* Optional for borrowed fixed storage. Called after registry erasure and
     * detachment, before releasing the retained renderable. Never fails. */
    void (*destroy_object)(void *context, AbGameObjectCore *object);
} AbGameObjectCoreHost;

/* GameLua.removeObject's physical-object prefix, entry ELF 0x31d48c, main path
 * through 0x31d688 and its out-of-line bird/cache/map-erasure branches.
 * Missing keys succeed with removed=false, without touching world/cache/list.
 * For bird/eagle colliders remove just the first list entry whose current name
 * equals the requested key. Then set any nonempty rolling-sound cache key to
 * present positive zero, destroy the body unless the world is locked, and close
 * current camera animation if requested. Do not clear the stored body handle.
 *
 * Retain an attached renderable, remove it from the scene, detach/release the
 * object's current renderable, erase the physical map key, destroy the object,
 * then release the retained handle. Services and their context remain stable
 * during this call; metadata may change through body/scene callbacks, but cached
 * objects/renderables stay alive until their release. No reentry or map rehash.
 * Caller owns healthy world, pinned names, bounded maps/list and stable object
 * metadata. Failure preserves earlier effects and leaves output unchanged.
 *
 * The subsequent joint-record and teleporter cleanup must still run, including
 * when the physical key is absent. This function does not implement that suffix,
 * Lua map cleanup, spawning or registry ownership. Camera/renderable host effects
 * and full owned integration have separate validation gates.
 */
bool ab_game_object_remove_core(AbGameObjectCoreRegistry *registry, AbSpan key,
                                const AbGameObjectCoreHost *host, bool *removed,
                                AbError *error);

#endif
