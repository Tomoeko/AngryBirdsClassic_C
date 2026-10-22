#ifndef AB_PHYSICS_FIXTURE_INTERNAL_H
#define AB_PHYSICS_FIXTURE_INTERNAL_H

#include "ab/physics_fixture.h"

bool ab_fixture_shape_bounds(const AbPhysicsShape *shape, AbRigidTransform transform,
                             AbAabb *bounds);

#endif
