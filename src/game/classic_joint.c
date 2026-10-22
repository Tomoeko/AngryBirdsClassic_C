#include "ab/classic_joint.h"

#include "classic_joint_internal.h"

#include <math.h>
#include <string.h>

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(
        table, (AbDataValue){.kind = AB_DATA_STRING,
                             .string = {(const uint8_t *)name, strlen(name)}});
    return value ? *value : (AbDataValue){0};
}

static float number(const AbDataTable *table, const char *name, float fallback) {
    AbDataValue value = field(table, name);
    float result = fallback;
    if (value.kind == AB_DATA_NUMBER) {
        memcpy(&result, &value.bits, sizeof(result));
    }
    return result;
}

static bool boolean(const AbDataTable *table, const char *name, bool fallback) {
    AbDataValue value = field(table, name);
    return value.kind == AB_DATA_BOOLEAN ? value.bits != 0 : fallback;
}

static bool finite_point(AbVec2 point) {
    return isfinite(point.x) && isfinite(point.y);
}

static bool read_identity(const AbClassicJointSource *source, AbClassicJoint *joint) {
    AbDataValue name = field(source->table, "name");
    AbDataValue first = field(source->table, "end1");
    AbDataValue second = field(source->table, "end2");
    float type = number(source->table, "type", 0);
    if (name.kind != AB_DATA_STRING || first.kind != AB_DATA_STRING ||
        second.kind != AB_DATA_STRING || type < 1 || type > 5 || floorf(type) != type ||
        (type != 5 && source->body_indices[0] == source->body_indices[1])) {
        return false;
    }
    joint->name = name.string;
    joint->type = (AbClassicJointType)(int)type;
    joint->endpoints[0] = first.string;
    joint->endpoints[1] = second.string;
    if (joint->type != AB_CLASSIC_JOINT_GLUE) {
        joint->joint.first_body = source->body_indices[0];
        joint->joint.second_body = source->body_indices[1];
        joint->joint.constraint.kind = (AbJointKind)((int)type - 1);
    }
    float coordinate = number(source->table, "coordType", 0);
    coordinate = floorf(coordinate + 0.5f);
    if (!isfinite(coordinate) || coordinate < 0 || coordinate > 2) {
        return false;
    }
    joint->coordinate_type = (int)coordinate;
    joint->coordinates[0] =
        (AbVec2){number(source->table, "x1", NAN), number(source->table, "y1", NAN)};
    joint->coordinates[1] =
        (AbVec2){number(source->table, "x2", NAN), number(source->table, "y2", NAN)};
    joint->joint.collide_connected = boolean(source->table, "collideConnected", false);
    if (joint->type == AB_CLASSIC_JOINT_GLUE) {
        joint->joint = (AbWorldSolveJoint){0};
    }
    joint->breakable = field(source->table, "breakable");
    if (joint->breakable.kind != AB_DATA_BOOLEAN) {
        joint->breakable = (AbDataValue){0};
    }
    joint->break_force = field(source->table, "breakForce");
    if (joint->break_force.kind != AB_DATA_NUMBER) {
        joint->break_force = (AbDataValue){0};
    }
    return finite_point(joint->coordinates[0]) && finite_point(joint->coordinates[1]) &&
           isfinite(number(source->table, "breakForce", 0));
}

static bool read_constraint(const AbDataTable *table, AbClassicJoint *joint) {
    if (joint->type == AB_CLASSIC_JOINT_GLUE) {
        joint->destroy_timer = number(table, "destroyTimer", 1);
        return isfinite(joint->destroy_timer);
    }
    AbJointConstraint *constraint = &joint->joint.constraint;
    if (constraint->kind == AB_JOINT_DISTANCE) {
        constraint->definition.distance.frequency = number(table, "frequency", 4);
        constraint->definition.distance.damping_ratio =
            number(table, "dampingRatio", 0.5f);
        return isfinite(constraint->definition.distance.frequency) &&
               isfinite(constraint->definition.distance.damping_ratio);
    }
    if (constraint->kind == AB_JOINT_WELD) {
        return true;
    }
    bool prismatic = constraint->kind == AB_JOINT_PRISMATIC;
    float lower = number(table, "lowerLimit", 0);
    float upper = number(table, "upperLimit", prismatic ? 5 : 0x1.921fb6p+1f);
    float speed = number(table, "motorSpeed", 0);
    float maximum = number(table, "maxTorque", 10000);
    bool motor = boolean(table, "motor", prismatic);
    bool limit = boolean(table, "limit", prismatic);
    joint->back_and_forth = boolean(table, "backAndForth", prismatic);
    if (!isfinite(lower) || !isfinite(upper) || !isfinite(speed) ||
        !isfinite(maximum) || maximum < 0) {
        return false;
    }
    if (prismatic) {
        joint->world_axis =
            (AbVec2){number(table, "worldAxisX", 0), number(table, "worldAxisY", 0)};
        constraint->definition.prismatic =
            (AbPrismaticDefinition){.local.axis = joint->world_axis,
                                    .lower_translation = lower,
                                    .upper_translation = upper,
                                    .motor_speed = speed,
                                    .maximum_motor_force = maximum,
                                    .enable_motor = motor,
                                    .enable_limit = limit};
        return finite_point(constraint->definition.prismatic.local.axis);
    }
    constraint->definition.revolute =
        (AbRevoluteDefinition){.lower_angle = lower,
                               .upper_angle = upper,
                               .motor_speed = speed,
                               .maximum_motor_torque = maximum,
                               .enable_motor = motor,
                               .enable_limit = limit};
    return true;
}

bool ab_classic_joint_read(const AbClassicJointSource *source, AbClassicJoint *output,
                           AbError *error) {
    if (!source || !output || !source->table) {
        return ab_fail(error, "Invalid classic joint source/storage");
    }
    AbClassicJoint joint = {0};
    if (!read_identity(source, &joint) || !read_constraint(source->table, &joint)) {
        return ab_fail(error, "Unsupported classic joint definition");
    }
    for (size_t index = 0; joint.type != AB_CLASSIC_JOINT_GLUE && index < 2; ++index) {
        AbRigidTransform frame = source->bodies[index].transform;
        if (!finite_point(frame.position) || !isfinite(frame.sine) ||
            !isfinite(frame.cosine)) {
            return ab_fail(error, "Nonfinite classic joint body frame");
        }
    }
    if (joint.type != AB_CLASSIC_JOINT_GLUE &&
        !ab_classic_joint_geometry(source, &joint, error)) {
        return ab_fail(error, "Unsupported classic joint definition or geometry");
    }
    *output = joint;
    return true;
}
