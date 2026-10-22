#include "world_internal.h"

#include <math.h>
#include <string.h>

static bool finite_point(AbVec2 point) {
    return isfinite(point.x) && isfinite(point.y);
}

static bool finite_anchors(const AbVec2 anchors[2]) {
    return finite_point(anchors[0]) && finite_point(anchors[1]);
}

bool ab_world_joint_definition_valid(const AbWorldSolveJoint *joint) {
    if (!joint || joint->first_body == joint->second_body) {
        return false;
    }
    switch (joint->constraint.kind) {
        case AB_JOINT_DISTANCE: {
            const AbDistanceDefinition *definition =
                &joint->constraint.definition.distance;
            return finite_anchors(definition->local_anchors) &&
                   isfinite(definition->length) && definition->length >= 0 &&
                   isfinite(definition->frequency) &&
                   isfinite(definition->damping_ratio);
        }
        case AB_JOINT_WELD: {
            const AbWeldDefinition *definition = &joint->constraint.definition.weld;
            return finite_anchors(definition->local_anchors) &&
                   isfinite(definition->reference_angle) &&
                   isfinite(definition->frequency) &&
                   isfinite(definition->damping_ratio);
        }
        case AB_JOINT_REVOLUTE: {
            const AbRevoluteDefinition *definition =
                &joint->constraint.definition.revolute;
            return finite_anchors(definition->local_anchors) &&
                   isfinite(definition->reference_angle) &&
                   isfinite(definition->lower_angle) &&
                   isfinite(definition->upper_angle) &&
                   isfinite(definition->motor_speed) &&
                   isfinite(definition->maximum_motor_torque) &&
                   definition->maximum_motor_torque >= 0;
        }
        case AB_JOINT_PRISMATIC: {
            const AbPrismaticDefinition *definition =
                &joint->constraint.definition.prismatic;
            return finite_anchors(definition->local_anchors) &&
                   finite_point(definition->local.axis) &&
                   finite_point(definition->local.perpendicular) &&
                   isfinite(definition->reference_angle) &&
                   isfinite(definition->lower_translation) &&
                   isfinite(definition->upper_translation) &&
                   isfinite(definition->motor_speed) &&
                   isfinite(definition->maximum_motor_force) &&
                   definition->maximum_motor_force >= 0;
        }
        default:
            return false;
    }
}

void ab_world_joint_reset(AbWorldSolveJoint *joint) {
    memset(&joint->constraint.state, 0, sizeof(joint->constraint.state));
    joint->island = false;
}
