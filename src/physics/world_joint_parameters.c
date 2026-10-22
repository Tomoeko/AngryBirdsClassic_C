#include "world_internal.h"

#include <math.h>

static AbWorldSolveJoint *parameter_joint(AbPhysicsWorld *world, size_t index) {
    if (!world || world->failed || !world->broadphase.tree.nodes ||
        !ab_world_has_joint(world, index)) {
        return NULL;
    }
    AbWorldSolveJoint *joint = world->storage.joints + index;
    if (!ab_world_has_body(world, joint->first_body) ||
        !ab_world_has_body(world, joint->second_body)) {
        return NULL;
    }
    return joint;
}

static void wake_endpoints(AbPhysicsWorld *world, const AbWorldSolveJoint *joint) {
    ab_world_wake_body(&world->storage.bodies[joint->first_body].motion);
    ab_world_wake_body(&world->storage.bodies[joint->second_body].motion);
}

typedef struct {
    bool *motor;
    bool *limit;
    float *speed;
    float *maximum;
    float *lower;
    float *upper;
    float *limit_impulse;
} MotorParameters;

static MotorParameters motor_parameters(AbWorldSolveJoint *joint) {
    if (joint->constraint.kind == AB_JOINT_REVOLUTE) {
        AbRevoluteDefinition *definition = &joint->constraint.definition.revolute;
        return (MotorParameters){&definition->enable_motor,
                                 &definition->enable_limit,
                                 &definition->motor_speed,
                                 &definition->maximum_motor_torque,
                                 &definition->lower_angle,
                                 &definition->upper_angle,
                                 &joint->constraint.state.revolute.impulse.z};
    }
    AbPrismaticDefinition *definition = &joint->constraint.definition.prismatic;
    return (MotorParameters){&definition->enable_motor,
                             &definition->enable_limit,
                             &definition->motor_speed,
                             &definition->maximum_motor_force,
                             &definition->lower_translation,
                             &definition->upper_translation,
                             &joint->constraint.state.prismatic.impulse.z};
}

static bool valid_parameters(const AbJointParameters *parameters, uint32_t supported) {
    if (!parameters || (parameters->present & ~supported)) {
        return false;
    }
    const float values[] = {parameters->speed,     parameters->maximum,
                            parameters->lower,     parameters->upper,
                            parameters->frequency, parameters->damping};
    const uint32_t flags[] = {AB_JOINT_SET_SPEED,     AB_JOINT_SET_MAXIMUM,
                              AB_JOINT_SET_LOWER,     AB_JOINT_SET_UPPER,
                              AB_JOINT_SET_FREQUENCY, AB_JOINT_SET_DAMPING};
    for (size_t index = 0; index < 6; ++index) {
        if ((parameters->present & flags[index]) && !isfinite(values[index])) {
            return false;
        }
    }
    return !(parameters->present & AB_JOINT_SET_MAXIMUM) || parameters->maximum >= 0;
}

static void set_bound(AbPhysicsWorld *world, const AbWorldSolveJoint *joint,
                      MotorParameters fields, float *bound, float value) {
    if (*bound != value) {
        wake_endpoints(world, joint);
        *bound = value;
        *fields.limit_impulse = 0;
    }
}

bool ab_world_set_joint_parameters(AbPhysicsWorld *world, size_t index,
                                   const AbJointParameters *parameters,
                                   AbError *error) {
    AbWorldSolveJoint *joint = parameter_joint(world, index);
    if (!joint) {
        return ab_fail(error, "Invalid physics joint parameter assignment");
    }
    bool distance = joint->constraint.kind == AB_JOINT_DISTANCE;
    bool motor = joint->constraint.kind == AB_JOINT_REVOLUTE ||
                 joint->constraint.kind == AB_JOINT_PRISMATIC;
    uint32_t supported = distance ? AB_JOINT_SET_FREQUENCY | AB_JOINT_SET_DAMPING
                         : motor  ? UINT32_C(0x3f)
                                  : 0;
    if (!valid_parameters(parameters, supported)) {
        return ab_fail(error, "Unsupported or nonfinite physics joint parameters");
    }
    uint32_t present = parameters->present;
    if (distance) {
        AbDistanceDefinition *definition = &joint->constraint.definition.distance;
        if (present & AB_JOINT_SET_FREQUENCY) {
            definition->frequency = parameters->frequency;
        }
        if (present & AB_JOINT_SET_DAMPING) {
            definition->damping_ratio = parameters->damping;
        }
        return true;
    }
    if (!motor) {
        return true;
    }
    MotorParameters fields = motor_parameters(joint);
    if (present & AB_JOINT_SET_MOTOR) {
        wake_endpoints(world, joint);
        *fields.motor = parameters->motor;
    }
    if (present & AB_JOINT_SET_SPEED) {
        wake_endpoints(world, joint);
        *fields.speed = parameters->speed;
    }
    if (present & AB_JOINT_SET_MAXIMUM) {
        wake_endpoints(world, joint);
        *fields.maximum = parameters->maximum;
    }
    if ((present & AB_JOINT_SET_LIMIT) && *fields.limit != parameters->limit) {
        wake_endpoints(world, joint);
        *fields.limit = parameters->limit;
        *fields.limit_impulse = 0;
    }
    if (present & AB_JOINT_SET_LOWER) {
        set_bound(world, joint, fields, fields.lower, parameters->lower);
    }
    if (present & AB_JOINT_SET_UPPER) {
        set_bound(world, joint, fields, fields.upper, parameters->upper);
    }
    return true;
}

bool ab_world_set_joint_motor_speed(AbPhysicsWorld *world, size_t index, float speed,
                                    AbError *error) {
    const AbJointParameters parameters = {.present = AB_JOINT_SET_SPEED,
                                          .speed = speed};
    return ab_world_set_joint_parameters(world, index, &parameters, error);
}
