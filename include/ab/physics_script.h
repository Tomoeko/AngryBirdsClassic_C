#ifndef AB_PHYSICS_SCRIPT_H
#define AB_PHYSICS_SCRIPT_H

#include "ab/block_event.h"

typedef struct {
    AbBlockEventObject *const *objects;
    size_t count;
    AbBlockEventHost host;
    AbSpan event;
    bool (*level_step)(void *context, float seconds, AbError *error);
    void *context;
} AbPhysicsScriptStep;

/* Original updatePhysicsStep sequence: invoke the resolved optional level
 * hook with seconds alone, then read current block-event state and dispatch
 * the same numeric argument. event is the resolved BlockEvents key, normally
 * onUpdateBlockPhysicsStep. The level hook may change the event/object/host view
 * before dispatch; the event value is then captured for that global traversal.
 * Handler implementations are first-party C. No Lua or allocation. Later
 * failure retains preceding callback changes. Numeric bits are passed unchanged.
 */
bool ab_physics_script_step(AbPhysicsScriptStep *step, float seconds, AbError *error);

#endif
