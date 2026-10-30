#ifndef AB_OBJECT_BODY_H
#define AB_OBJECT_BODY_H

#include "ab/object_physics.h"
#include "ab/physics_world.h"

typedef struct {
    size_t body;
    size_t fixture;
    AbDataValue collider;
    bool collisions_enabled;
    bool gravity_enabled;
    bool controllable;
    AbObjectDimensions dimensions;
} AbObjectBody;

/* GameLua setRotation's binary32 remainder by twice binary32 pi, followed by
 * addition of that period for negative remainders. Negative zero is retained;
 * rounding can produce the period itself for very small negative inputs.
 * Finite inputs only, nearest rounding, gradual underflow. Invalid inputs leave
 * output unchanged; floating-point status flags are outside this contract.
 */
bool ab_game_angle_wrap(float angle, float *output);

/* Create the ordinary circle/box/convex-polygon physics body: zero initial angle and
 * velocity, angular damping one, then fixture attachment, sensor and optional
 * override-angle changes. Shape/material comes from ab_object_physics. Body
 * user data is zero; fixture user data is supplied by the caller. The returned
 * gravity flag is game metadata, not a change to the engine gravity scale.
 * This covers physics creation only. Ghost/force-adder effects, Lua object
 * tables, renderers, scripts, bird queues and gameplay callbacks remain outside
 * this interface. World storage must have a free body and fixture slot. No
 * allocation occurs. Validation preserves output; a later numeric/capacity
 * failure may leave a failed world with a partly created body until close.
 */
bool ab_object_body_create(AbPhysicsWorld *world, const AbObjectDefinition *definition,
                           AbVec2 position, AbVec2 sprite_size, float physics_scale,
                           uintptr_t fixture_user_data, AbObjectBody *output,
                           AbError *error);

/* Direct createCircleLua geometry, without catalog/material resolution, sprite
 * sizing, sensors or angle overrides. Collider is basic; collision/gravity are
 * enabled. Caller owns the surrounding native object/table/vector registration.
 * Finite nonnegative radius/material inputs only; signed zero is retained.
 * Output commits on success; late world failures retain partial work. */
bool ab_object_body_create_circle(AbPhysicsWorld *world, AbVec2 position, float radius,
                                  float density, float friction, float restitution,
                                  uintptr_t fixture_user_data, AbObjectBody *output,
                                  AbError *error);

/* GameObject.setRadius/recreate: replace its single circle fixture, retaining
 * the body, joints, pose and velocity. Rebuild the fixture with its current
 * material/user identity, the default filter and no sensor. Native recreation
 * destroys contacts/proxies and recomputes mass. It is not a body replacement
 * or an in-place shape edit. Finite nonnegative radius; allocation is bounded
 * by existing world storage. A late failure requires discarding the session. */
bool ab_object_body_resize_circle(AbPhysicsWorld *world, AbObjectBody *object,
                                  float radius, AbError *error);

/* Museum-mode ground uses explicit 2000-by-10 physics units, zero density,
 * friction 0.8, zero restitution, and the basic collider. The caller supplies
 * the recovered ground position; deriving level bounds is a separate task.
 */
bool ab_ground_body_create(AbPhysicsWorld *world, AbVec2 position,
                           uintptr_t fixture_user_data, AbObjectBody *output,
                           AbError *error);

/* Museum queue physics initialization: set the current-position rotation to
 * zero, then make the body static. Queue ordering, animation timers, selection
 * and movement onto the sling are separate game-state operations.
 */
bool ab_object_body_queue(AbPhysicsWorld *world, const AbObjectBody *object,
                          AbError *error);

#endif
