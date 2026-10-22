#ifndef AB_GAME_CONTACT_H
#define AB_GAME_CONTACT_H

#include "ab/classic_joints.h"
#include "ab/collision_damage.h"

typedef struct {
    AbSpan name;
    bool script_table_present;
} AbGameContactObject;

typedef struct {
    bool should_disable_contact;
    AbDataValue new_collision_parameters;
    AbDataValue glue_contact_pair;
} AbGameContactResponse;

typedef struct {
    AbSpan first, second;
    AbVec2 normal, point;
    AbGameContactResponse response;
} AbGameContactCall;

typedef struct {
    void *context;
    bool (*collision)(void *context, AbGameContactCall *call, AbError *error);
    bool (*velocity)(void *context, AbSpan name, AbDeferredVelocity **output,
                     AbError *error);
    AbClassicGlueBodyLookup body;
} AbGameContactHost;

typedef struct {
    const AbGameContactObject *first, *second;
    const AbWorldManifold *manifold;
    AbContactState *contact;
    AbClassicGlueQueue *glue;
    bool callbacks_disabled;
} AbGameContactInput;

/* Resolved GameLua::basicBeginContact. Skip disabled callbacks, absent fixture
 * objects, or either absent script table before inspecting geometry/hosts.
 * Otherwise call collision once with copied names, normal and first world point,
 * and a fresh response with disable=false and nil optional fields. Then clear
 * the contact's enabled bit if requested, offer a deferred velocity, and append
 * a glue pair, in that order. A disabled contact still consumes both responses.
 *
 * Optional response values act only when tables. Velocity tables require string
 * birdName and finite numeric afterCollisionVelocityX/Y; resolve its retained
 * pending state through velocity. Glue uses ab_classic_glue_queue_contact,
 * retaining pair order, duplicates and SIZE_MAX for unknown body endpoints.
 * Lua coercion/metatables and missing velocity objects are outside this typed
 * boundary. The host owns gameplay effects; this does not route collider
 * handlers, calculate damage, remove bodies or install a world listener.
 *
 * Borrow disjoint live input, names, geometry, queue and returned response tables.
 * Names and response tables remain stable through lookups; response storage must
 * outlive the collision callback's return. No allocation or engine list mutation.
 * Required geometry is finite; invalid preflight inputs preserve contact/queue.
 * A later host/response error retains preceding callback, disable, velocity and
 * lookup effects. Serialize calls and do not reenter on the same borrowed state.
 */
bool ab_game_contact_begin(const AbGameContactInput *input, AbGameContactHost host,
                           AbError *error);

typedef bool (*AbGameContactEndHost)(void *context, AbSpan first, AbSpan second,
                                     AbError *error);

/* basicEndContact checks both fixture objects and script-table memberships,
 * then requests onEndContact with the two names. Unlike begin, it does not read
 * the callback-disable flag or contact geometry, and has no response/velocity/
 * glue processing. Missing objects/tables skip before inspecting names/host.
 * Borrow stable name bytes through the call; no allocation or engine mutation.
 * The host owns callback effects. A host failure retains those effects. */
bool ab_game_contact_end(const AbGameContactObject *first,
                         const AbGameContactObject *second, AbGameContactEndHost host,
                         void *context, AbError *error);

#endif
