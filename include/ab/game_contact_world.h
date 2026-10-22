#ifndef AB_GAME_CONTACT_WORLD_H
#define AB_GAME_CONTACT_WORLD_H

#include "ab/game_contact.h"
#include "ab/game_contact_routes.h"

typedef struct {
    AbGameCollisionObject collision;
    AbGameContactObject script;
} AbGameContactWorldObject;

typedef bool (*AbGameContactWorldLookup)(void *context, uintptr_t object,
                                         AbGameContactWorldObject *output,
                                         AbError *error);

typedef struct {
    const AbGameContactRoutes *routes;
    AbClassicGlueQueue *glue;
    void *object_context;
    AbGameContactWorldLookup object;
    AbGameContactHost basic;
    /* End/special handlers use basic.context. The object resolver is a pure
     * lookup; returned names remain live through callback completion. */
    AbGameContactEndHost end;
    AbGameContactDispatchHost special;
    bool begin_disabled;
} AbGameContactWorldConfig;

typedef struct {
    AbPhysicsWorld *world;
    AbGameContactWorldConfig config;
    AbWorldListener previous;
    AbError error;
    bool processing, failed;
} AbGameContactWorld;

/* Install caller-owned stable listener storage on a borrowed healthy unlocked
 * world. Existing begin/end/pre-solve listeners reject; filter, post-solve and
 * destruction callbacks retain their previous context through forwarding.
 * Configuration is copied; routing, queues, callback contexts and returned names
 * remain borrowed until close. No allocation or initial world/object changes occur.
 * Failure preserves output and the previous listener. Never move an open owner.
 *
 * Resolve nonzero fixture user data to game/script metadata, select the native
 * event route and invoke the basic begin/end boundaries. Ordinary begin derives
 * its first world point/normal from actual contact/body state. Zero-point sensor
 * geometry is explicitly unsupported at that basic boundary. Other handler
 * tokens go to special; an absent required host is an error, never a skip.
 * Gameplay effects and pending-velocity flushing remain host work.
 *
 * Callback failures retain preceding effects, store the first error and mark
 * the world failed. Stop/discard that simulation. Calls are serialized; hosts
 * must not reenter this listener, move its storage or change world.listener.
 * Close before the world or borrowed configuration; close during a locked step
 * or callback rejects. The owner is inert after successful close.
 */
bool ab_game_contact_world_open(AbPhysicsWorld *world, AbGameContactWorldConfig config,
                                AbGameContactWorld *output, AbError *error);
bool ab_game_contact_world_close(AbGameContactWorld *contacts, AbError *error);

#endif
