#include "ab/physics_joint_constraint.h"

#include "internal.h"

#include <math.h>

static bool finite_point(AbVec2 point) {
    return isfinite(point.x) && isfinite(point.y);
}

static bool finite_frame(AbRigidTransform frame) {
    return finite_point(frame.position) && isfinite(frame.sine) &&
           isfinite(frame.cosine);
}

bool ab_joint_constraint_coordinate(const AbJointConstraint *joint,
                                    const AbJointBodyFrame bodies[2], float *output,
                                    AbError *error) {
    if (!joint || !bodies || !output) {
        return ab_fail(error, "Invalid joint coordinate storage");
    }
    float value;
    if (joint->kind == AB_JOINT_REVOLUTE) {
        if (!isfinite(bodies[0].angle) || !isfinite(bodies[1].angle) ||
            !isfinite(joint->definition.revolute.reference_angle)) {
            return ab_fail(error, "Nonfinite revolute coordinate input");
        }
        value = bodies[1].angle - bodies[0].angle;
        value -= joint->definition.revolute.reference_angle;
    } else if (joint->kind == AB_JOINT_PRISMATIC) {
        const AbPrismaticDefinition *definition = &joint->definition.prismatic;
        if (!finite_point(definition->local_anchors[0]) ||
            !finite_point(definition->local_anchors[1]) ||
            !finite_point(definition->local.axis) ||
            !finite_frame(bodies[0].transform) || !finite_frame(bodies[1].transform)) {
            return ab_fail(error, "Nonfinite prismatic coordinate input");
        }
        AbVec2 first =
            ab_physics_point(definition->local_anchors[0], bodies[0].transform);
        AbVec2 second =
            ab_physics_point(definition->local_anchors[1], bodies[1].transform);
        AbVec2 axis = ab_physics_rotate(definition->local.axis, bodies[0].transform);
        float y = second.y - first.y;
        float x = second.x - first.x;
        value = y * axis.y;
        value += x * axis.x;
    } else {
        return ab_fail(error, "Unsupported joint coordinate kind");
    }
    if (!isfinite(value)) {
        return ab_fail(error, "Joint coordinate arithmetic overflow");
    }
    *output = value;
    return true;
}
