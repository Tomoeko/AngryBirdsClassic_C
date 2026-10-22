#ifndef AB_GAME_UPDATE_TAIL_H
#define AB_GAME_UPDATE_TAIL_H

#include "ab/base.h"
#include "ab/object_update.h"
#include "ab/physics_geometry.h"

typedef struct {
    uintptr_t identity;
    bool script_present;
} AbGameTailObject;

typedef struct {
    AbGameTailObject *object;
    AbObjectUpdates *updates;
    void *receiver;
} AbGameTailHook;

typedef struct {
    AbGameTailHook *items;
    size_t count;
} AbGameTailHooks;

typedef struct {
    AbSpan *names;
    size_t count, capacity;
} AbGameJointQueue;

typedef struct {
    AbSpan name;
    bool back_and_forth;
} AbGameJointEntry;

typedef struct {
    const AbGameJointEntry *entries;
    size_t count;
    AbGameJointQueue *destroy;
} AbGameTailJoints;

/* Append one borrowed name without deduplicating. context is an
 * AbGameJointQueue; this signature also binds to a classic joint break request.
 * Strings must remain alive through the controller's destruction pass. No
 * allocation occurs; invalid input/capacity failure preserves the queue. */
bool ab_game_joint_defer(void *context, AbSpan name, AbError *error);

typedef struct {
    AbVec2 cursor, old_cursor;
    float eagle_darkness, time_since_start;
    bool darkness_present, time_since_present;
    bool bird_fired, mighty_timer_present, red_special_available;
    bool skip_input, skip_trigger_specialty;
    AbGameTailHooks *hooks;
    AbGameTailJoints *joints;
} AbGameTailState;

typedef enum {
    AB_GAME_TAIL_SLING = 1,
    AB_GAME_TAIL_WOBBLE,
    AB_GAME_TAIL_ANIMATION,
    AB_GAME_TAIL_ROCKET,
    AB_GAME_TAIL_BIRD,
    AB_GAME_TAIL_BOUNDARIES,
    AB_GAME_TAIL_OBJECT_EFFECT,
    AB_GAME_TAIL_MIGHTY_TIMER,
    AB_GAME_TAIL_CAMERA,
    AB_GAME_TAIL_BOOMERANG,
    AB_GAME_TAIL_CHARACTER_ANIMATIONS,
    AB_GAME_TAIL_FLOATING_SCORES,
    AB_GAME_TAIL_SCORE,
    AB_GAME_TAIL_SPECIAL_FEATHERS,
    AB_GAME_TAIL_PHYSICS_ENABLED,
    AB_GAME_TAIL_LEVEL_EFFECTS,
    AB_GAME_TAIL_BLOCK_SCRIPTS,
    AB_GAME_TAIL_OBJECT_SCRIPT,
    AB_GAME_TAIL_GFX_EFFECTS,
    AB_GAME_TAIL_DRAW,
    AB_GAME_TAIL_HOOK_EXISTS,
    AB_GAME_TAIL_CHECK_JOINT_LIMIT = 23,
    AB_GAME_TAIL_DESTROY_JOINT,
} AbGameTailOperation;

typedef struct {
    AbGameTailOperation operation;
    AbGameTailObject *object;
    float seconds, time;
    /* Host replies for BIRD, BOUNDARIES, PHYSICS_ENABLED and HOOK_EXISTS. */
    bool result;
    AbSpan joint_name;
} AbGameTailCall;

typedef bool (*AbGameTailHost)(void *context, AbGameTailCall *call, AbError *error);

typedef struct {
    void *context;
    bool (*begin)(void *context, AbError *error);
    bool (*next)(void *context, AbGameTailObject **object, AbError *error);
} AbGameTailWorld;

/* Begin each source pairs loop separately. next returns NULL at its end; each
 * returned row remains stable throughout that visit's host calls. The producer
 * resumes from the retained key in the current table after callback mutations.
 * It must report an invalidated key instead of substituting snapshot order. */
bool ab_game_update_tail_world(AbGameTailState *state, AbGameTailWorld world,
                               float seconds, float time, AbGameTailHost host,
                               void *context, AbError *error);

/* Original ordinary GameUpdate tail beginning at SlingshotSystem.gameUpdate.
 * This route has no flying-bird special case or grenades.
 * Objects borrow a stable original traversal order/lifetime
 * for both passes; membership/order mutations are outside this contract.
 * Execute actual controller/render effects through the host. It may mutate
 * state and object.script_present: later stages read them at the original time.
 * Optional hooks borrow a separate stable native name-map traversal and run
 * after eagle darkness, before object animation/rocket/bird passes. HOOK_EXISTS
 * is the pure truthiness query for that name's current world entry. A missing
 * entry drops only the registered binding, preserving the object's property.
 * Live hooks invoke the captured registered callable with receiver/seconds.
 * Hook arrays/order/lifetimes remain stable; map insertion/reinsertion and
 * traversal recovery are separate. Hooks may mutate ordinary state/callbacks.
 * Optional joints borrow the current visible metadata in original table order.
 * After boomerang update, request limit checks for back_and_forth entries, then
 * visit destruction names in append order before character animation. The host
 * runs actual limit checks or the destruction event/native removal boundary.
 * Entry order/membership remain stable; the host may append destruction names
 * within the supplied capacity while flushing, matching the original ipairs
 * loop. Clear the queue only after every request succeeds. A failed flush can
 * retain earlier host effects and its original queue contents; stop/discard it.
 * Preserve eagle-darkness arithmetic, object callback gates, phase order,
 * cursor copy before effects/draw and trailing skip/time housekeeping.
 * No allocation or original code. Finite used numbers/results and count <=2^24
 * are required. Invalid top-level arguments reject before work; later failure
 * retains prior host/state changes, so the caller must stop/discard the frame. */
bool ab_game_update_tail(AbGameTailState *state, AbGameTailObject *objects,
                         size_t count, float seconds, float time, AbGameTailHost host,
                         void *context, AbError *error);

#endif
