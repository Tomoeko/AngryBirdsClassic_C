#ifndef AB_BUBBLE_CONTACT_H
#define AB_BUBBLE_CONTACT_H

#include "ab/collision_damage.h"
#include "ab/lua_data.h"
#include "ab/physics_contact.h"

typedef struct {
    AbSpan name;
    AbDataValue in_bubble, shot, strength, kill_silently;
    AbDataValue anti_gravity_timer, sprite, sprite_scale;
    AbVec2 body_velocity;
    AbDeferredVelocity pending;
    bool dead;
} AbBubbleContactObject;

typedef struct {
    AbSpan first, second;
    AbVec2 normal, point;
    AbDataTable response;
} AbBubbleCollisionCall;

typedef struct {
    void *context;
    bool (*trapped)(void *context, AbBubbleContactObject *object, AbError *error);
    bool (*geometry)(void *context, AbWorldManifold *output, AbError *error);
    bool (*collision)(void *context, AbBubbleCollisionCall *call, AbError *error);
} AbBubbleContactHost;

/* Resolved GameLua bubble-begin state/callback boundary. The caller selects the
 * bird/collider and bubble independent of fixture order. Only Boolean true
 * in_bubble retains the bubble; otherwise zero its strength, set kill_silently
 * and mark it dead before consulting shot. A falsy shot then returns.
 *
 * For a shot collider, overwrite pending velocity with current body velocity
 * times binary32 0.1, set capture fields, and call trapped only for a new capture.
 * Obtain actual current contact geometry afterward; invoke collision with
 * collider/bubble names and the negated fixture-order world normal. Its fresh
 * response table is empty and discarded without applying disable/velocity/glue
 * responses. No contact flags or body velocities change through this helper.
 *
 * Borrow distinct stable typed objects/names. Hosts may mutate them in native
 * callback order; geometry requires a real active first world point. No Lua,
 * allocation, body destruction, dead-map insertion or world-listener selection
 * occurs here. The gameplay owner maps dead membership and other fields into
 * its live registry. Required numeric geometry/velocity must be finite. A late
 * error preserves prior object/host effects; stop/discard that frame.
 */
bool ab_bubble_contact_begin(AbBubbleContactObject *collider,
                             AbBubbleContactObject *bubble, AbBubbleContactHost host,
                             AbError *error);

#endif
