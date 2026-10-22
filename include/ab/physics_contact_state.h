#ifndef AB_PHYSICS_CONTACT_STATE_H
#define AB_PHYSICS_CONTACT_STATE_H

#include "ab/physics_contact.h"
#include "ab/physics_step.h"

enum {
    AB_CONTACT_TOUCHING = 2,
    AB_CONTACT_ENABLED = 4,
    AB_CONTACT_FILTER = 8,
};

typedef struct {
    AbManifold manifold;
    uint32_t flags;
    float friction;
    float restitution;
    float tangent_speed;
} AbContactState;

typedef struct {
    void *context;
    void (*manifold)(void *context, AbManifold *output);
    bool (*overlap)(void *context);
} AbContactEvaluator;

typedef struct {
    void *context;
    void (*begin)(void *context, AbContactState *contact);
    void (*end)(void *context, AbContactState *contact);
    void (*pre_solve)(void *context, AbContactState *contact,
                      const AbManifold *previous);
} AbContactListener;

/* Initialize material mixing and active state. Preserve inactive manifold
 * fields, as in the target constructor. Finite nonnegative friction and finite
 * restitution are required; invalid inputs leave the initialized output alone.
 */
bool ab_contact_state_initialize(AbContactState *contact, float first_friction,
                                 float second_friction, float first_restitution,
                                 float second_restitution);

/* Refresh one existing contact. A sensor uses overlap; a solid uses manifold.
 * Evaluators preserve inactive geometry and write at most two active points.
 * Feature IDs retain the first matching previous impulse pair. Solid touching
 * transitions wake both bodies before callbacks. Begin/end precede pre_solve;
 * sensors never wake bodies or call pre_solve. Listener callbacks may change
 * contact state. The previous manifold is borrowed only during pre_solve.
 * Invalid arguments reject before mutation. A malformed evaluator result can
 * reject after geometry/enable flags have changed. No allocation occurs.
 */
bool ab_contact_state_update(AbContactState *contact, AbBodyMotion *first,
                             AbBodyMotion *second, bool sensor,
                             const AbContactEvaluator *evaluator,
                             const AbContactListener *listener);

#endif
