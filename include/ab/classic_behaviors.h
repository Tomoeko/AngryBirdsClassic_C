#ifndef AB_CLASSIC_BEHAVIORS_H
#define AB_CLASSIC_BEHAVIORS_H

#include "ab/classic_level.h"
#include "ab/game_behavior.h"
#include "ab/museum_bird_behavior.h"

typedef struct AbClassicBehaviors AbClassicBehaviors;

typedef struct {
    /* NULL uses the APK-owned level catalog's constructor layouts. Explicit
     * metadata must describe that same borrowed catalog. */
    const AbLuaData *catalog_data;
    AbDataValue *flying_bird;
    void *context;
    bool (*call)(void *context, const AbMuseumBirdCall *call, AbError *error);
    size_t storage_bytes;
    AbBluesTrajectoryHost *blues;
    /* Zero reserves only the initially loaded object count. Explicitly reserve
     * more stable rows/maps for later source GameObject construction. */
    size_t object_capacity;
} AbClassicBehaviorConfig;

/* Own the initial ordinary Museum level's behavior groups and concrete legacy
 * bird instances. Resolve each non-ground object's definition through the live
 * level property registry, then visit its authored behaviors using the actual
 * constructor layout. Ground receives no behavior argument. Support the two
 * legacy Museum bird classes, the Blues camera class, and the Blues trajectory
 * class with its authored isCluster override; other classes,
 * malformed or mixed configurations and missing layouts reject explicitly.
 *
 * Compose the recovered addBehavior factory/append boundary and event dispatcher.
 * Publish each missing empty group before its factory, then append the actual
 * instance after construction. Script/group table addresses stay stable. All
 * initial script behavior tables must be empty and
 * unbound. Construction uses an already loaded level; interleaving body
 * creation with level hooks remains outer loader work. The byte limit covers
 * all owned arrays and maps. No frame allocation,
 * Lua execution, removal, renderer or camera is supplied. Explicit later
 * create_object calls require pre-reserved object_capacity.
 *
 * Level, catalog, config context and flying_bird remain borrowed. Close before
 * the level and after releasing all borrowed group/instance views. Close clears
 * this owner's table projections without sending native onDestroy events; game
 * destruction must dispatch those separately. Failure returns NULL and releases
 * partial construction. Services are required only when an event reaches them.
 * Serialize calls; callbacks may change live fields and *flying_bird but must not
 * replace/release registry storage or reenter this owner. */
bool ab_classic_behaviors_open(AbClassicLevel *level, AbClassicBehaviorConfig config,
                               AbClassicBehaviors **output, AbError *error);
void ab_classic_behaviors_close(AbClassicBehaviors *behaviors);
size_t ab_classic_behaviors_bytes(const AbClassicBehaviors *behaviors);
const AbStringMap *ab_classic_behaviors_groups(const AbClassicBehaviors *behaviors,
                                               size_t object);

/* GameObject.createGameObject1204 for the next newly registered physical slot.
 * Bind actual GameObject.init state, then construct the supplied source behavior
 * descriptor through addBehavior and the real class factories. The descriptor's
 * exact static constructor layout is required; it need not be the parent's
 * catalog descriptor. Borrow its values/names as in the class constructor.
 * Pre-reserved maps/instances keep addresses stable and calls allocate nothing.
 *
 * Slots append without gaps or reuse. Source init callbacks run during this call
 * and may reach actual camera/trajectory services. Missing/unsupported bindings
 * retain preceding script/instance/callback effects; discard on failure. This
 * does not supply bird membership, raw clone fields, physics adapters or notify
 * onBirdAdded (which the Blues child source does not call). */
bool ab_classic_behaviors_create_object(AbClassicBehaviors *behaviors, size_t object,
                                        AbDataValue descriptor,
                                        const AbLuaData *descriptor_data,
                                        AbError *error);

/* Recreate the source GameObject after native circle/renderer replacement.
 * Discard the old behavior groups and initialize fresh instances; raw physical
 * fields copied by Super Seeds remain available to the new script. */
bool ab_classic_behaviors_recreate_object(AbClassicBehaviors *behaviors, size_t object,
                                          AbDataValue descriptor, AbError *error);

/* Actual later GameObject.addBehavior on a registered canonical script. Retain
 * existing groups/instances and publish a missing empty group before a reached
 * factory failure. Two distinct classes with one instance each fit the reserved
 * per-object bound, including the source Blues child trajectory + camera pair.
 * Values and class-name bytes remain borrowed; this allocates no storage. Init
 * can call camera/trajectory services. Do not reenter. Earlier effects survive
 * failure; discard after a reached unsupported class or exhausted group/item
 * bound. This does not select flyingBird or register a camera target row. */
bool ab_classic_behaviors_add(AbClassicBehaviors *behaviors, size_t object, AbSpan name,
                              AbDataValue overrides, AbError *error);

/* Stable initial object indices. Collision forwards both actual owned physical
 * tables and all numeric/nil slots through the generic dispatcher. The Museum
 * method currently uses only the second physical object. Raw controllable and
 * other physical fields must already be published by their owning game phases.
 * Launch forwards its argument but does not publish launchVelocity or choose
 * flyingBird; those retain the native shot callback ordering. Prior service/
 * instance effects survive a later failure. No object/world membership changes. */
bool ab_classic_behaviors_launch(AbClassicBehaviors *behaviors, size_t object,
                                 AbVec2 velocity, AbError *error);
bool ab_classic_behaviors_collision(AbClassicBehaviors *behaviors, size_t object,
                                    size_t other, const AbBlockCollisionNotice *notice,
                                    AbError *error);
/* Physical arguments may include raw dynamically constructed objects without
 * GameObject scripts (the Mighty Eagle). Only the receiver requires an owned
 * behavior group. Preserve it independently of the two object-table arguments.
 * A noncontrollable second receiver receives the original first/second pair. */
bool ab_classic_behaviors_bird_collision(AbClassicBehaviors *behaviors, size_t receiver,
                                         size_t first, size_t second,
                                         const AbBlockCollisionNotice *notice,
                                         AbError *error);
/* GameObject.onBlockCollision forwards seven slots: other name, force, damage,
 * normal x/y and point x/y. It does not forward physical-table arguments. */
bool ab_classic_behaviors_block_collision(AbClassicBehaviors *behaviors, size_t object,
                                          const AbBlockCollisionNotice *notice,
                                          AbError *error);
bool ab_classic_behaviors_specialty(AbClassicBehaviors *behaviors, size_t object,
                                    AbError *error);
bool ab_classic_behaviors_destroy(AbClassicBehaviors *behaviors, size_t object,
                                  AbError *error);
/* Actual GameObject.debugDraw2637 delegates with no arguments. Supported Museum
 * and trajectory classes have no active debugDraw member. Keep dispatch through
 * the owned behavior groups so a reached bound method is never skipped. */
bool ab_classic_behaviors_debug_draw(AbClassicBehaviors *behaviors, size_t object,
                                     AbError *error);

typedef enum {
    AB_CLASSIC_TOUCH_START,
    AB_CLASSIC_TOUCH_UPDATE,
    AB_CLASSIC_TOUCH_END,
    AB_CLASSIC_TOUCH_CANCEL,
} AbClassicTouchKind;

/* Actual GameObject touch wrappers delegate the touch table, with seconds only
 * for update. Touch storage is borrowed for this call. The currently supported
 * Museum classes inherit empty touch methods and do not retain these arguments.
 * This does not construct TouchInteractableObject or other active handlers. */
bool ab_classic_behaviors_touch(AbClassicBehaviors *behaviors, size_t object,
                                AbClassicTouchKind kind, AbDataTable *touch,
                                float seconds, AbError *error);

#endif
