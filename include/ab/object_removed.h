#ifndef AB_OBJECT_REMOVED_H
#define AB_OBJECT_REMOVED_H

#include "ab/lua_data.h"
#include "ab/string_map.h"

typedef struct {
    AbDataValue objects;
    AbStringMap *level_goals;
    AbDataValue seconds, time, score, last_score_time, old_score;
} AbObjectRemovedState;

typedef struct {
    void *context;
    /* Resolve/call the cached object's current exitedFunction, with the object
     * as receiver and the supplied dt value. Missing/non-functions must fail. */
    bool (*exited)(void *context, AbDataValue object, AbDataValue seconds,
                   AbError *error);
    /* The captured private notification upvalue receives one object argument.
     * Its actual definition/level dispatch may use ab_object_removal_notify. */
    bool (*notify)(void *context, AbDataValue object, AbError *error);
    bool (*check_goals)(void *context, AbDataValue *result, AbError *error);
} AbObjectRemovedHost;

/* Gamelogic.onObjectRemoved, prototype 120975, for pinned string names and plain
 * world/dynamic tables. A falsy current world value returns immediately.
 * Controllable objects only call exitedFunction when mighty bait/eagle is
 * truthy, then return; ordinary controllable birds perform no cleanup here.
 * Other objects notify first, then clear the current world, current dynamic
 * and current goal binding in that order. A truthy goal check copies current
 * time to lastScoreTime and current score to oldScore without numeric coercion.
 * deadBlocks is untouched. This notification does not destroy a physical body.
 *
 * Callback changes to later bindings/values remain live. Preserve the cached
 * object and name storage even after erasure. Tables need capacity for nil-key
 * writes; no mixed-key iteration, allocation, metatables, Lua or reentry. Missing
 * hosts fail when reached. Earlier changes survive failure; discard on error.
 * Physical/session ownership and concrete callback effects are separate work.
 */
bool ab_object_removed(AbObjectRemovedState *state, AbSpan name,
                       AbObjectRemovedHost *host, AbError *error);

#endif
