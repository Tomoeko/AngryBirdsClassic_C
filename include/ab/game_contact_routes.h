#ifndef AB_GAME_CONTACT_ROUTES_H
#define AB_GAME_CONTACT_ROUTES_H

#include "ab/base.h"
#include "ab/game_collision.h"
#include "ab/physics_contact.h"

typedef enum {
    AB_GAME_CONTACT_BEGIN,
    AB_GAME_CONTACT_END,
    AB_GAME_CONTACT_PRE_SOLVE,
    AB_GAME_CONTACT_STAGES,
} AbGameContactStage;

typedef enum {
    AB_GAME_CONTACT_NO_HANDLER,
    AB_GAME_CONTACT_BASIC_BEGIN,
    AB_GAME_CONTACT_BASIC_END,
    AB_GAME_CONTACT_PORTAL_BEGIN,
    AB_GAME_CONTACT_PORTAL_END,
    AB_GAME_CONTACT_BUBBLE_BEGIN,
} AbGameContactHandler;

typedef struct {
    uint32_t handlers[AB_GAME_CONTACT_STAGES][AB_GAME_COLLIDER_TYPES]
                     [AB_GAME_COLLIDER_TYPES];
} AbGameContactRoutes;

/* Semantic handler tokens from GameLua setupCollisionTable and its three
 * symmetric setters. Initialize from no handlers, or set up an existing table
 * while preserving pairs/stages the original setup does not write. In
 * particular, setup leaves every pre-solve entry and bird/bubble end handler
 * untouched. Collision eligibility is a separate matrix; disabled pairs can
 * still have handlers. Values beyond the named tokens belong to the caller.
 *
 * These are routing data, not native member-function pointers or callbacks.
 * The outer listener resolves tokens and owns handler/context lifetimes and
 * effects. Setup does not implement portal/bubble behavior or dispatch events.
 * No allocation, numeric simulation or world mutation. Invalid calls preserve
 * output; serialized caller-owned storage is required.
 */
bool ab_game_contact_routes_initialize(AbGameContactRoutes *routes);
bool ab_game_contact_routes_setup(AbGameContactRoutes *routes);
bool ab_game_contact_routes_set(AbGameContactRoutes *routes, AbGameContactStage stage,
                                uint32_t first, uint32_t second, uint32_t handler);

typedef struct {
    AbGameContactStage stage;
    const AbGameCollisionObject *first, *second;
    size_t contact;
    const AbManifold *previous;
    bool begin_disabled;
} AbGameContactDispatchInput;

typedef struct {
    AbGameContactStage stage;
    uint32_t handler;
    size_t contact;
    const AbManifold *previous;
} AbGameContactDispatchCall;

typedef bool (*AbGameContactDispatchHost)(void *context,
                                          const AbGameContactDispatchCall *call,
                                          AbError *error);

/* Resolved outer BeginContact/EndContact/PreSolve dispatch. Only begin consults
 * begin_disabled. All stages skip absent fixture objects before route lookup;
 * script-table membership and enabled/filter state are not inspected here.
 * Select the ordered collider pair's stage token; no-handler skips the host.
 * Pass the contact identity unchanged and previous only for pre-solve, including
 * NULL. The enclosing listener resolves fixture objects and contact storage.
 *
 * Borrow inputs/tables through the call; no allocation or world mutation. The
 * host owns handler effects, which may change routing for a later dispatch.
 * Invalid required inputs reject before the host; host failure retains its
 * effects. This does not invoke original delegates or implement their bodies.
 */
bool ab_game_contact_dispatch(const AbGameContactRoutes *routes,
                              const AbGameContactDispatchInput *input,
                              AbGameContactDispatchHost host, void *context,
                              AbError *error);

#endif
