#ifndef AB_JOINT_SOLVER_INTERNAL_H
#define AB_JOINT_SOLVER_INTERNAL_H

#include "ab/physics_joint.h"
#include "ab/physics_matrix.h"

bool ab_joint_solver_offset(AbVec2 anchor, const AbJointSolverBody *body,
                            AbVec2 *output);

AbSymmetric3 ab_joint_solver_matrix(const AbJointSolverBody bodies[2],
                                    const AbVec2 offsets[2]);

#endif
