#ifndef AB_PHYSICS_BODY_H
#define AB_PHYSICS_BODY_H

#include "ab/physics_geometry.h"

typedef enum {
    AB_BODY_STATIC,
    AB_BODY_KINEMATIC,
    AB_BODY_DYNAMIC,
} AbBodyType;

typedef struct {
    AbBodyType type;
    bool fixed_rotation;
    AbRigidTransform transform;
    AbVec2 local_center;
    AbVec2 previous_center;
    AbVec2 center;
    float previous_angle;
    float angle;
    float sweep_alpha;
    AbVec2 linear_velocity;
    float angular_velocity;
    float mass;
    float inverse_mass;
    float inertia;
    float inverse_inertia;
} AbBodyMassState;

typedef struct {
    float density;
    AbMass data;
} AbFixtureMass;

/* Explicit override only; fixture aggregation is separate. Finite inputs and
 * initialized state are required. Locked worlds and nondynamic bodies retain
 * their state. Nonpositive mass becomes one; inertia is shifted from the local
 * origin to the new center. Implicit central-inertia clamping is not performed.
 */
void ab_body_mass_set(AbBodyMassState *body, AbMass mass, bool world_locked);
/* Aggregate in the supplied fixture order. Zero-density entries are ignored
 * before reading their mass data; nonzero-density data must be finite.
 * No world-lock check is added to this target reset operation.
 */
void ab_body_mass_reset(AbBodyMassState *body, const AbFixtureMass *fixtures,
                        size_t count);

#endif
