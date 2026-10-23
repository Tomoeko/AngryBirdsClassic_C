#ifndef AB_CLASSIC_OBJECT_REMOVAL_H
#define AB_CLASSIC_OBJECT_REMOVAL_H

#include "ab/classic_render_scene.h"
#include "ab/game_object_removal.h"

typedef struct AbClassicObjectRemoval AbClassicObjectRemoval;

typedef struct {
    AbClassicLevel *level;
    AbClassicVisuals *visuals;
    AbClassicRenderScene *scene;
    /* Borrow the actual post-physics accumulator and teleporter registry. */
    AbStringMap *rolling_levels, *teleporters;
    void *context;
    bool (*close_camera_animation)(void *context, AbSpan name, AbError *error);
    void (*destroy_teleporter)(void *context, AbGameTeleporter *teleporter);
    size_t storage_bytes;
    size_t object_capacity;
} AbClassicObjectRemovalConfig;

/* Own native object metadata/map, creation-order bird vector and stable
 * renderer reference counters. Borrow the canonical level, visuals and shared
 * maps until close. Prepare the material-only native rolling-sound argument;
 * initialize camera animation absent, as in the native constructor. Later camera
 * setup must publish its actual flag through the registry view. No body, visual,
 * map, RNG or callback effects occur at open. Explicit object_capacity reserves
 * later direct-circle identities; zero retains the initial count.
 *
 * Remove through the recovered physical/joint/teleporter implementation. The
 * renderer's stable metadata survives detach and every retain/release until this
 * owner closes; GPU/atlas resources stay owned by their resource group. Forget
 * canonical physical membership immediately after native map erasure so later
 * simulation and scene passes skip the removed slot. Lua world/dynamic/goal/dead
 * maps are deliberately cleared later by removeBlocks. Calls allocate nothing.
 * A failure retains preceding body/scene/map effects; discard on failure.
 */
bool ab_classic_object_removal_open(AbClassicObjectRemovalConfig config,
                                    AbClassicObjectRemoval **output, AbError *error);
void ab_classic_object_removal_close(AbClassicObjectRemoval *owner);
size_t ab_classic_object_removal_bytes(const AbClassicObjectRemoval *owner);
AbGameObjectCoreRegistry *
ab_classic_object_removal_registry(AbClassicObjectRemoval *owner);

/* Register the next direct circle, or rebind a fully removed row with zero
 * retained renderer references, after actual visual/scene construction,
 * before later gameplay/removal passes. Native circle rolling name starts empty
 * and camera animation absent; copied Lua material/definition fields do not run
 * ObjectCreation again. Native vector insertion uses the circle's creation-time
 * controllable flag, independently of its current collider/raw copied field.
 * Retained metadata/renderable counters stay stable when a body slot is reused
 * under another new identity. No allocation, resource effect or callback.
 * Capacity must be pre-reserved; earlier effects survive a reached failure. */
bool ab_classic_object_removal_register_circle(AbClassicObjectRemoval *owner,
                                               size_t object, AbError *error);
/* Defined explicit box counterpart. Boxes do not register a native bird. */
bool ab_classic_object_removal_register_box(AbClassicObjectRemoval *owner,
                                            size_t object, AbError *error);
bool ab_classic_object_removal_call(void *context, AbSpan name, AbError *error);

#endif
