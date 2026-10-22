#ifndef AB_PHYSICS_POSITION_INTERNAL_H
#define AB_PHYSICS_POSITION_INTERNAL_H

#include "ab/physics_solver.h"

bool ab_contact_position_point_rate(const AbPositionContact *contact, size_t point,
                                    AbPositionBody *first, AbPositionBody *second,
                                    float *minimum_separation, float correction_rate);
bool ab_position_constraint_solve(AbPositionState *positions,
                                  const AbPositionConstraint *constraint,
                                  float *minimum_separation, float correction_rate);

#endif
