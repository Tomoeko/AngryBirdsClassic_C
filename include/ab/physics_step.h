#ifndef AB_PHYSICS_STEP_H
#define AB_PHYSICS_STEP_H

#include "ab/physics_body.h"

#include <stdint.h>

typedef struct {
    AbBodyMassState mass;
    AbVec2 force;
    float torque;
    float linear_damping;
    float angular_damping;
    float gravity_scale;
    float sleep_time;
    bool awake;
    bool sleep_allowed;
    bool bullet;
    bool active;
    uintptr_t user_data;
} AbBodyMotion;

typedef struct {
    AbBodyType type;
    AbVec2 position;
    float angle;
    AbVec2 linear_velocity;
    float angular_velocity;
    float linear_damping;
    float angular_damping;
    float gravity_scale;
    bool sleep_allowed;
    bool awake;
    bool fixed_rotation;
    bool bullet;
    bool active;
    uintptr_t user_data;
} AbBodyDefinition;

/* Definition fields are explicit; no game-specific defaults are inferred.
 * Initialize a body without fixtures, contacts, joints, or world membership.
 * Invalid/nonfinite values, negative damping, and unsupported angles leave
 * output unchanged. Static/kinematic definitions retain supplied velocities.
 */
bool ab_body_initialize(const AbBodyDefinition *definition, AbBodyMotion *output);

/* GameLua's velocity/impulse body semantics, allowed during contact callbacks.
 * Static velocity assignments and nondynamic impulses are no-ops. A positive
 * squared assigned speed wakes only a sleeping body (including target float
 * underflow in that test); a dynamic impulse wakes even for a zero impulse.
 * NULL point selects transform origin, not mass center. Retain native operation
 * order for angular impulse. Required inputs/read state and results are finite;
 * inverse mass/inertia must be nonnegative. Failure preserves the entire body.
 */
bool ab_body_velocity_set(AbBodyMotion *body, AbVec2 velocity);
bool ab_body_angular_velocity_set(AbBodyMotion *body, float velocity);
bool ab_body_impulse_apply(AbBodyMotion *body, AbVec2 impulse, const AbVec2 *point);

typedef struct {
    float seconds;
    AbVec2 gravity;
    float maximum_translation;
    float maximum_translation_squared;
    size_t position_iterations;
    bool allow_sleep;
} AbFreeIslandStep;

/* Contact/joint-free island integration only. Membership and order are supplied
 * by the caller; awake flags do not filter the selected bodies. Preserve forces
 * except when the entire island sleeps. Both translation limits are explicit
 * native controls. No allocation occurs. Numeric inputs and intermediate values
 * must be finite, damping nonnegative, and angles in the rotation module's range.
 * Invalid controls/types reject before mutation. An unsupported updated angle
 * can stop after earlier bodies have changed. Arrays and step cannot overlap.
 */
bool ab_island_step_free(AbBodyMotion *bodies, size_t count,
                         const AbFreeIslandStep *step);

#endif
