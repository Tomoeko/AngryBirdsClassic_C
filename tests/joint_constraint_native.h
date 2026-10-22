#ifndef AB_TEST_JOINT_CONSTRAINT_NATIVE_H
#define AB_TEST_JOINT_CONSTRAINT_NATIVE_H

#include "ab/physics_joint_constraint.h"

static bool prepare_joint(const uint32_t input[41], uint32_t sample, size_t index,
                          AbJointConstraint *joint) {
    float values[38];
    memcpy(values, input + 3, sizeof(values));
    *joint = (AbJointConstraint){.kind = (AbJointKind)input[0]};
    AbVec2 anchors[] = {{values[10], values[11]}, {values[12], values[13]}};
    AbVec3 impulse = {values[19], values[20], values[21]};
    bool motor = ((sample >> (index + 1)) & 1u) != 0;
    bool limit = ((sample >> (index + 3)) & 1u) != 0;
    switch (joint->kind) {
        case AB_JOINT_DISTANCE:
            joint->definition.distance = (AbDistanceDefinition){
                {anchors[0], anchors[1]}, values[14], values[15], values[16]};
            joint->state.distance.impulse = impulse.x;
            break;
        case AB_JOINT_WELD:
            joint->definition.weld = (AbWeldDefinition){
                {anchors[0], anchors[1]}, values[14], values[15], values[16]};
            joint->state.weld.impulse = impulse;
            break;
        case AB_JOINT_REVOLUTE:
            joint->definition.revolute =
                (AbRevoluteDefinition){.local_anchors = {anchors[0], anchors[1]},
                                       .reference_angle = values[14],
                                       .lower_angle = values[15],
                                       .upper_angle = values[16],
                                       .motor_speed = values[30],
                                       .maximum_motor_torque = values[29],
                                       .enable_motor = motor,
                                       .enable_limit = limit};
            joint->state.revolute.impulse = impulse;
            joint->state.revolute.motor_impulse = values[28];
            joint->state.revolute.limit = (AbJointLimitState)input[34];
            break;
        case AB_JOINT_PRISMATIC:
            joint->definition.prismatic =
                (AbPrismaticDefinition){.local_anchors = {anchors[0], anchors[1]},
                                        .reference_angle = values[14],
                                        .lower_translation = values[15],
                                        .upper_translation = values[16],
                                        .motor_speed = values[30],
                                        .maximum_motor_force = values[29],
                                        .enable_motor = motor,
                                        .enable_limit = limit};
            CHECK(ab_prismatic_axes_prepare((AbVec2){values[32], values[33]},
                                            &joint->definition.prismatic.local, NULL));
            joint->state.prismatic.impulse = impulse;
            joint->state.prismatic.motor_impulse = values[28];
            joint->state.prismatic.limit = (AbJointLimitState)input[34];
            break;
        default:
            CHECK(false);
    }
    return true;
}

static bool project_joint(const AbJointConstraint *joint, uint32_t output[37]) {
    float values[36] = {0};
    const AbVec2 *centers;
    const float *masses, *inertias;
    uint32_t limit = 0;
    switch (joint->kind) {
        case AB_JOINT_DISTANCE: {
            const AbDistanceState *state = &joint->state.distance;
            centers = state->local_centers;
            masses = state->inverse_mass;
            inertias = state->inverse_inertia;
            const float fields[] = {state->offsets[0].x, state->offsets[0].y,
                                    state->offsets[1].x, state->offsets[1].y,
                                    state->axis.x,       state->axis.y,
                                    state->mass,         state->gamma,
                                    state->bias,         state->impulse};
            memcpy(values + 8, fields, sizeof(fields));
            break;
        }
        case AB_JOINT_WELD: {
            const AbWeldState *state = &joint->state.weld;
            centers = state->local_centers;
            masses = state->inverse_mass;
            inertias = state->inverse_inertia;
            const float fields[] = {
                state->offsets[0].x, state->offsets[0].y, state->offsets[1].x,
                state->offsets[1].y, state->inverse.xx,   state->inverse.xy,
                state->inverse.xz,   state->inverse.xy,   state->inverse.yy,
                state->inverse.yz,   state->inverse.xz,   state->inverse.yz,
                state->inverse.zz,   state->impulse.x,    state->impulse.y,
                state->impulse.z,    state->gamma,        state->bias};
            memcpy(values + 8, fields, sizeof(fields));
            break;
        }
        case AB_JOINT_REVOLUTE: {
            const AbRevoluteState *state = &joint->state.revolute;
            centers = state->local_centers;
            masses = state->inverse_mass;
            inertias = state->inverse_inertia;
            const float fields[] = {
                state->offsets[0].x, state->offsets[0].y, state->offsets[1].x,
                state->offsets[1].y, state->matrix.xx,    state->matrix.xy,
                state->matrix.xz,    state->matrix.xy,    state->matrix.yy,
                state->matrix.yz,    state->matrix.xz,    state->matrix.yz,
                state->matrix.zz,    state->motor_mass,   state->impulse.x,
                state->impulse.y,    state->impulse.z,    state->motor_impulse};
            memcpy(values + 8, fields, sizeof(fields));
            limit = (uint32_t)state->limit;
            break;
        }
        case AB_JOINT_PRISMATIC: {
            const AbPrismaticState *state = &joint->state.prismatic;
            centers = state->local_centers;
            masses = state->inverse_mass;
            inertias = state->inverse_inertia;
            const AbPrismaticAxes local = joint->definition.prismatic.local;
            const float fields[] = {state->axis.x,
                                    state->axis.y,
                                    state->perpendicular.x,
                                    state->perpendicular.y,
                                    state->perpendicular_arms[0],
                                    state->perpendicular_arms[1],
                                    state->axis_arms[0],
                                    state->axis_arms[1],
                                    state->matrix.xx,
                                    state->matrix.xy,
                                    state->matrix.xz,
                                    state->matrix.xy,
                                    state->matrix.yy,
                                    state->matrix.yz,
                                    state->matrix.xz,
                                    state->matrix.yz,
                                    state->matrix.zz,
                                    state->motor_mass,
                                    state->impulse.x,
                                    state->impulse.y,
                                    state->impulse.z,
                                    state->motor_impulse,
                                    local.axis.x,
                                    local.axis.y,
                                    local.perpendicular.x,
                                    local.perpendicular.y};
            memcpy(values + 8, fields, sizeof(fields));
            limit = (uint32_t)state->limit;
            break;
        }
        default:
            CHECK(false);
    }
    const float common[] = {centers[0].x, centers[0].y, centers[1].x, centers[1].y,
                            masses[0],    masses[1],    inertias[0],  inertias[1]};
    memcpy(values, common, sizeof(common));
    for (size_t field = 0; field < 36; ++field) {
        output[field] = to_bits(values[field]);
    }
    output[36] = limit;
    return true;
}

#endif
