#include "joint_solver_internal.h"
#include "ab/rotation.h"

#include <math.h>

bool ab_joint_solver_offset(AbVec2 anchor, const AbJointSolverBody *body,
                            AbVec2 *output) {
    AbRotation rotation;
    if (!ab_rotation_from_angle(body->angle, &rotation)) {
        return false;
    }
    float y = anchor.y - body->local_center.y;
    float x = anchor.x - body->local_center.x;
    float rotated_y = y * rotation.cosine;
    rotated_y += x * rotation.sine;
    float rotated_x = y * rotation.sine;
    rotated_x = -rotated_x;
    rotated_x += x * rotation.cosine;
    *output = (AbVec2){rotated_x, rotated_y};
    return isfinite(output->x) && isfinite(output->y);
}

AbSymmetric3 ab_joint_solver_matrix(const AbJointSolverBody bodies[2],
                                    const AbVec2 offsets[2]) {
    float first = bodies[0].inverse_inertia;
    float second = bodies[1].inverse_inertia;
    float masses = bodies[0].inverse_mass + bodies[1].inverse_mass;
    float xx = masses;
    xx += first * (offsets[0].y * offsets[0].y);
    xx += second * (offsets[1].y * offsets[1].y);
    float yy = masses;
    yy += first * (offsets[0].x * offsets[0].x);
    yy += second * (offsets[1].x * offsets[1].x);
    float xy = second * (offsets[1].x * offsets[1].y);
    xy = -xy;
    xy += first * ((-offsets[0].y) * offsets[0].x);
    float xz = second * offsets[1].y;
    xz = -xz;
    xz += first * (-offsets[0].y);
    float yz = second * offsets[1].x;
    yz += first * offsets[0].x;
    return (AbSymmetric3){xx, xy, xz, yy, yz, first + second};
}
