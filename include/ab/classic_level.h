#ifndef AB_CLASSIC_LEVEL_H
#define AB_CLASSIC_LEVEL_H

#include "ab/game_collision.h"
#include "ab/character_animation.h"
#include "ab/damage_sprite.h"
#include "ab/game_frame.h"
#include "ab/game_circle.h"
#include "ab/object_properties.h"
#include "ab/level_bodies.h"
#include "ab/level_joints.h"
#include "ab/level_raycast.h"
#include "ab/level_properties.h"
#include "ab/bird_impact_world.h"
#include "ab/random.h"
#include "ab/sprite_set.h"
#include "ab/classic_level_source.h"

typedef struct AbClassicLevel AbClassicLevel;

/* Query the current owned physics world and canonical live level names. */
bool ab_classic_level_raycast(const AbClassicLevel *level, AbVec2 first, AbVec2 second,
                              AbLevelRaycast *output, AbError *error);
/* Bind owned world/names/membership to the predictor. Bird pose, launch velocity,
 * gravity, radius dispatch and logging remain explicit caller-owned inputs. */
bool ab_classic_level_predict_impact(const AbClassicLevel *level,
                                     AbBirdImpactInput *input,
                                     AbBirdImpactWorldHost *host,
                                     AbBirdImpactPrediction *output, AbError *error);

typedef struct {
    void *context;
    size_t maximum_raycasts;
    bool (*print)(void *context, size_t raycasts, AbError *error);
    bool (*observe)(void *context, AbVec2 first, AbVec2 second,
                    const AbLevelRaycast *result, AbError *error);
} AbClassicImpactServices;

/* Use the owned script/physical/catalog radius path for this fixed object,
 * together with the real named physics-world query. Numeric radius bits,
 * including nonfinite values, pass through unchanged. Other radius types reject
 * when reached; no coercion. Pose/launch velocity remain explicit inputs.
 * Print and observation retain the contracts in bird_impact_world.h. */
bool ab_classic_level_predict_object_impact(AbClassicLevel *level, size_t object,
                                            AbBirdImpactInput *input,
                                            AbClassicImpactServices *services,
                                            AbBirdImpactPrediction *output,
                                            AbError *error);

typedef struct {
    size_t objects;
    size_t contacts;
    size_t storage_bytes;
    /* Zero derives initial capacity from level data. Reserve explicit larger
     * bounds for later dynamic joints/glue. joints covers engine slots and the
     * immediate registry; glue_joints covers deferred game records. */
    size_t joints;
    size_t glue_joints;
} AbClassicLevelLimits;

enum {
    AB_STARTUP_BIRD_TIMERS = 1,
    AB_STARTUP_GOAL_TIMERS = 2,
};

typedef struct {
    float animation;
    float jump;
    float blink;
    float oink;
    uint32_t present;
} AbClassicObjectTimers;

typedef struct {
    AbVec2 velocity;
    bool sleeping, outside_boundaries;
    bool initialized;
    bool previous_awake;
} AbClassicMotionProperties;

typedef struct {
    AbPhysicsWorld *world;
    AbClassicJoints *joints;
    AbLevelBodies *bodies;
    AbGameFrame *frame;
    AbGameCollisionMatrix *collisions;
    const size_t *object_order;
    size_t order_hash_capacity;
    /* World-table prefix at the latest construction/insertion. Canonical nil
     * writes synchronize at the next insertion; query properties for current
     * membership. Retained absent slots follow the prefix so object_order stays
     * a permutation of all stable object slots. */
    size_t world_count;
    const AbLuaData *data;
    const AbDataTable *catalog;
    /* Constructor layouts are available for an APK-owned catalog. A level
     * opened from a borrowed plain catalog has no implicit layout metadata. */
    const AbLuaData *catalog_data;
    const AbSpriteSet *sprites;
    const AbClassicObjectTimers *timers;
    bool timers_initialized;
    const size_t *bird_order;
    size_t bird_count;
    size_t bird_hash_capacity;
    const size_t *goal_order;
    size_t goal_count;
    size_t goal_hash_capacity;
    AbCharacterAnimationObject *character_objects;
    const AbDamageSpriteNames *initial_sprites;
    bool sprites_initialized;
    const AbClassicMotionProperties *motion_properties;
    const bool *removed_objects;
    AbLevelProperties *properties;
} AbClassicLevelView;

typedef struct {
    const char *level_path;
    const char *catalog_path;
    const char *const *sprite_paths;
    size_t sprite_sheet_count;
    size_t resource_bytes;
    float placement_scale;
    float physics_scale;
    AbVec2 ground_position;
    bool derive_ground_position;
    bool derive_world_gravity;
    /* Actual Museum scaleFactor selection after reading this level. Explicit
     * placement_scale remains available for isolated research fixtures. */
    bool derive_placement_scale;
    float physics_to_world;
    AbRandom48 *random;
    void *missing_context;
    void (*missing_definition)(void *context, AbSpan definition);
    /* Prepare the selected level's resource groups after its data is decoded,
     * before any body geometry or sprite query. Borrow both inputs only for
     * this call. Failure discards the load; resource-host effects remain owned
     * by the caller. This boundary runs once during loading, never in a frame. */
    void *prepare_context;
    bool (*prepare)(void *context, const AbLuaData *level, const AbLuaData *catalog,
                    AbError *error);
    /* Optional construction-time sprite dimensions from active resources.
     * When supplied, sprite_paths/count must be empty and the resulting level
     * owns no sprite set. The callback/context are borrowed only during load;
     * return native resource bounds, including zero for missing names. Object
     * construction validates geometry; invalid queries must report failure. */
    void *sprite_context;
    bool (*sprite_size)(void *context, AbSpan name, AbVec2 *size, AbError *error);
    /* Optional verified shared blocks/key owner. Borrow until this level closes;
     * catalog_path must select blocks.lua. Its storage is bounded separately. */
    const AbClassicLevelSource *source;
} AbClassicLevelAssets;

/* Register a new direct native circle in an unused stable level slot. Publish
 * raw physical world membership and both pose samples; keep the creation name
 * borrowed. Previously used names reject, including removed names. World string
 * iteration retains the native growing/deletion order with pinned keys and no
 * collection phase. Synchronize preceding canonical nil writes before insertion.
 * Native controllable-vector registration and source birds/dynamic membership
 * belong to their enclosing owner and are separate from this raw operation.
 *
 * No renderer, script, behavior, character timers, definition or clone fields
 * are initialized. Existing fixed-slot adapters must register the new identity
 * before their next pass; this API alone does not extend them. Source callbacks
 * may not create objects during a locked world or active borrowed owner pass.
 * No allocation beyond existing bounded world growth. The output index commits
 * on success; late failure retains prior effects and requires session discard. */
bool ab_classic_level_create_circle(AbClassicLevel *level, AbGameCircleInput input,
                                    size_t *output, AbError *error);

/* Own the fixed world/solver/object/order/history arrays for an ordinary classic
 * level. Source data and strings remain borrowed until close; the sprite callback
 * is borrowed only during open. World settings and storage limits are explicit
 * caller inputs. storage_bytes bounds this owner, fixed arrays and world-name
 * map; world slot
 * metadata and broadphase allocations are separately bounded by the supplied
 * capacities and world limits. Unsupported level geometry/chains reject.
 *
 * Create in constructor order, recover growing runtime-table order, optionally
 * make controllable bodies static in that order, then seed both pose samples
 * and ordinary numeric character state. Recover separate initial goal/bird
 * orders. Ground strength is the script's explicit thirty. Present catalog
 * strengths must be finite numbers. Character flags collapse native nil/false
 * to false; poses, strength and allowRoll derive from bodies/catalog. Timers
 * remain disabled until the explicit phase below. Clock/history start at zero.
 * Publish native material/mass/motion metadata and resolved ordinary collision
 * fields into the canonical physical tables. Preserve creation-time script mass
 * when a queued body becomes static. The generated ground retains its separate
 * material/strength/defence assignments. An APK load also publishes the prepared
 * damage/blink/smile names. Mutable animation/damage-range construction and
 * arbitrary object hooks remain separate work.
 * Create initial joints in static constructor iteration order, resolving named
 * bodies before controllables are queued. Own immediate/deferred metadata and
 * engine/solver arrays. Contact/timer glue production, creation events and later
 * game-driven joint/body removal remain separate. This does not bind
 * sprites/audio, sling, camera, events or controller timing. Own the initial
 * name/definition/dimension/cached-position/controllable property projection and
 * separate Lua-world membership;
 * its diagnostics require an explicit caller binding before a missing lookup.
 * Output is NULL on failure;
 * all partial world/array allocations are released. No original code executes. */
bool ab_classic_level_open(const AbLevelBodySource *source,
                           AbWorldDefinition definition, AbClassicLevelLimits limits,
                           bool queue_controllables, AbClassicLevel **output,
                           AbError *error);

/* MuseumMode's initial timer assignments in runtime object-table order: literal
 * controllable birds draw animation/jump [10,30]/10; truthy level goals draw
 * blink/oink [5,30]/10. Optionally apply each bird's zero rotation/static type
 * after its timer pair, then reset its pose history. Call once before advancing
 * the frame. No allocation, seed selection, hooks, tutorials, sprite selection or
 * later controller work runs. Those external consumers must retain shared RNG
 * order. Generated timers also seed the owned character update state. The
 * timers view remains a startup snapshot; subsequent character updates own
 * their changing timers separately. Queue changes resynchronize initial poses.
 * Failure can follow partial timer/world/RNG changes; discard the owner.
 */
bool ab_classic_level_initialize_objects(AbClassicLevel *level, AbRandom48 *random,
                                         bool queue_controllables, AbError *error);

/* Apply resolved native position/rotation assignment to one fixed object. Its
 * character jump position/rotation and physical x/y/angle are canonical script
 * pose fields; update only the assigned component of rendered/history state. Preserve
 * other cached components and body velocities/awake state. Removed bodies are no-ops.
 * This does not step/interpolate physics or resolve a name. A later world failure can
 * retain prior physics changes. See object_transform.h for numeric constraints.
 */
bool ab_classic_level_set_position(AbClassicLevel *level, size_t object,
                                   AbVec2 position, AbError *error);
bool ab_classic_level_set_rotation(AbClassicLevel *level, size_t object, float angle,
                                   AbError *error);

/* Unbind an initial physical object's fixed slot after successful physical-map
 * removal. Keep source data/indices and cached poses alive for borrowed views;
 * later force, history, publication, character and scene passes skip this row.
 * This does not destroy a body or clear script maps/joints/teleporters. The
 * complete native removal function and its owner perform those operations.
 * Locked-world orphan bodies may remain live. Idempotent; no allocation or host
 * callback. A slot cannot be reused/reinserted through this boundary. */
bool ab_classic_level_forget_object(AbClassicLevel *level, size_t object,
                                    AbError *error);

/* Direct AbCharacterAnimations.apply_jump callback; context is this level.
 * Apply the prepared position/rotation events in order after the numeric jump
 * phase has committed frame.next. No additional RNG, effects, hats or allocation.
 * Invalid plans reject before mutation; later failure retains earlier events.
 */
bool ab_classic_level_apply_jump(void *context, size_t object,
                                 const AbBirdJumpFrame *frame, AbError *error);

/* Resolved updateObjectProperties callback: assign the supplied script pose,
 * velocity, sleeping and boundary fields. Pose uses canonical character fields;
 * renderer/history state remains untouched. Numeric words copy without coercion
 * or normalization, including a signed body angle distinct from rendered angle.
 * Motion fields start uninitialized and become present on first publication.
 * The caller runs the native eligibility/argument plan at the recovered frame
 * phase. No physics changes, effects or allocation. Invalid binding rejects
 * before writes. Later object lifecycle and full outer-frame work are separate.
 */
bool ab_classic_level_publish_properties(AbClassicLevel *level, size_t object,
                                         const AbObjectProperties *properties,
                                         AbError *error);

/* Plan publication using the owned old-awake flag, initially false in ordinary
 * native circle/box construction. Publishing preserves that flag. After all
 * native object work, finish_object records the body's current awake flag for
 * the next visit. Keep these phases separate: intervening callbacks may change
 * awake state. Bounds and outer object traversal/scheduling remain explicit.
 * Both operations require a healthy unlocked world and stable live body.
 */
bool ab_classic_level_property_plan(AbClassicLevel *level, size_t object,
                                    int32_t minimum_x, int32_t maximum_x,
                                    AbObjectPropertyPlan *output, AbError *error);
bool ab_classic_level_finish_object(AbClassicLevel *level, size_t object,
                                    AbError *error);

/* Load the pinned APK's packed catalog/level and explicit SPRT sheets, then
 * create the owned level above using actual region dimensions. Resource bytes
 * bound retained decoded data/metadata together, and the temporary library read;
 * codec workspaces have their own bounds. No private key file, extracted source,
 * Lua VM or original executable code is used. Imported catalog tables outlive
 * level data. APK bytes remain borrowed until close. Optional derivation uses
 * MuseumMode's ground bounds and level gravity; other world settings, scales and
 * resource paths are explicit selections. Complete resource-group, profile and
 * level-start policy remains separate. A supplied random state enables the timer
 * phase above and commits its consumed state only when the entire load succeeds;
 * NULL leaves those timers uninitialized. No implicit random seed is chosen.
 * Prepare a borrowed initial sprite-name snapshot from the owned catalog's
 * constructor layouts, reusing startup scratch. This requires string-keyed
 * range tables within the level's index/node capacities. No sprite action,
 * asset presence validation, image decoding or GPU upload runs in this phase.
 * Body construction uses either owned explicit sprite metadata or the supplied
 * sprite-size callback; the latter leaves view.sprites NULL.
 * The open-with-borrowed-source path leaves that snapshot uninitialized. */
bool ab_classic_level_load(const AbZip *archive, const AbClassicLevelAssets *assets,
                           AbWorldDefinition definition, AbClassicLevelLimits limits,
                           bool queue_controllables, AbClassicLevel **output,
                           AbError *error);
void ab_classic_level_close(AbClassicLevel *level);

/* Borrow stable state until close. Do not move/free arrays, change their capacity
 * or separate the collision filter from its owner context. World and bird orders
 * are initial runtime-table orders; bird indices refer to the body-object array.
 * Launch selection uses separate start numbers. Deletion/reinsertion needs
 * separate table recovery.
 * Update frame bindings when body slots are removed/reused. NULL gives an empty
 * view. The owner must remain alive while any view or fixture user data is used. */
AbClassicLevelView ab_classic_level_view(AbClassicLevel *level);

typedef struct {
    AbSpan name;
    bool started;
} AbClassicWorldCursor;

/* Actual current world-table string traversal. Zero/reset the cursor at each
 * fresh source pairs(world) boundary; retain it across that loop's callbacks.
 * Synchronize canonical value presence, then resume after the retained current
 * key in its current hash slot. Creation/removal between visits is supported;
 * insertion may change later order or discard a removed current key, which
 * fails like native next rather than restarting. No allocation/effects/RNG.
 * Borrowed names remain level-owned. Table replacement, foreign keys/values
 * and mutations bypassing canonical owned creation/removal are unsupported.
 * Success sets SIZE_MAX/false at end; failure preserves cursor/outputs and
 * preceding metadata changes. Serialize and retain the level until loop end. */
bool ab_classic_level_world_next(AbClassicLevel *level, AbClassicWorldCursor *cursor,
                                 size_t *object, bool *present, AbError *error);

#endif
