#include "ab/classic_joints.h"

#include "classic_joints_internal.h"

#include <stddef.h>
#include <string.h>

typedef struct {
    const char *name;
    uint32_t flag;
    size_t offset;
    bool boolean;
} ParameterField;

static const ParameterField fields[] = {
    {"motor", AB_JOINT_SET_MOTOR, offsetof(AbJointParameters, motor), true},
    {"motorSpeed", AB_JOINT_SET_SPEED, offsetof(AbJointParameters, speed), false},
    {"maxTorque", AB_JOINT_SET_MAXIMUM, offsetof(AbJointParameters, maximum), false},
    {"limit", AB_JOINT_SET_LIMIT, offsetof(AbJointParameters, limit), true},
    {"lowerLimit", AB_JOINT_SET_LOWER, offsetof(AbJointParameters, lower), false},
    {"upperLimit", AB_JOINT_SET_UPPER, offsetof(AbJointParameters, upper), false},
    {"frequency", AB_JOINT_SET_FREQUENCY, offsetof(AbJointParameters, frequency),
     false},
    {"dampingRatio", AB_JOINT_SET_DAMPING, offsetof(AbJointParameters, damping), false},
};

static AbJointParameters initial_parameters(const AbClassicJoint *joint) {
    const AbJointConstraint *constraint = &joint->joint.constraint;
    if (joint->type == AB_CLASSIC_JOINT_DISTANCE) {
        const AbDistanceDefinition *definition = &constraint->definition.distance;
        return (AbJointParameters){.present =
                                       AB_JOINT_SET_FREQUENCY | AB_JOINT_SET_DAMPING,
                                   .frequency = definition->frequency,
                                   .damping = definition->damping_ratio};
    }
    if (joint->type == AB_CLASSIC_JOINT_REVOLUTE) {
        const AbRevoluteDefinition *definition = &constraint->definition.revolute;
        return (AbJointParameters){.present = UINT32_C(0x3f),
                                   .motor = definition->enable_motor,
                                   .limit = definition->enable_limit,
                                   .speed = definition->motor_speed,
                                   .maximum = definition->maximum_motor_torque,
                                   .lower = definition->lower_angle,
                                   .upper = definition->upper_angle};
    }
    if (joint->type == AB_CLASSIC_JOINT_PRISMATIC) {
        const AbPrismaticDefinition *definition = &constraint->definition.prismatic;
        return (AbJointParameters){.present = UINT32_C(0x3f),
                                   .motor = definition->enable_motor,
                                   .limit = definition->enable_limit,
                                   .speed = definition->motor_speed,
                                   .maximum = definition->maximum_motor_force,
                                   .lower = definition->lower_translation,
                                   .upper = definition->upper_translation};
    }
    return (AbJointParameters){0};
}

AbDataValue ab_classic_joint_parameter(const AbClassicJointRecord *record,
                                       uint32_t parameter) {
    if (!record) {
        return (AbDataValue){0};
    }
    AbJointParameters initial = initial_parameters(&record->definition);
    const AbJointParameters *parameters = &record->parameter_updates;
    if (!(parameters->present & parameter)) {
        parameters = &initial;
    }
    for (size_t index = 0; index < sizeof(fields) / sizeof(*fields); ++index) {
        const ParameterField *field = fields + index;
        if (field->flag != parameter || !(parameters->present & parameter)) {
            continue;
        }
        const uint8_t *value = (const uint8_t *)parameters + field->offset;
        AbDataValue result = {.kind =
                                  field->boolean ? AB_DATA_BOOLEAN : AB_DATA_NUMBER};
        if (field->boolean) {
            bool flag;
            memcpy(&flag, value, sizeof(flag));
            result.bits = flag ? 1u : 0u;
        } else {
            memcpy(&result.bits, value, sizeof(result.bits));
        }
        return result;
    }
    return (AbDataValue){0};
}

static AbJointParameters read_parameters(const AbDataTable *table, uint32_t allowed) {
    AbJointParameters parameters = {0};
    for (size_t index = 0; index < sizeof(fields) / sizeof(*fields); ++index) {
        const ParameterField *field = fields + index;
        if (!(allowed & field->flag)) {
            continue;
        }
        AbDataValue key = {
            .kind = AB_DATA_STRING,
            .string = {(const uint8_t *)field->name, strlen(field->name)}};
        const AbDataValue *value = ab_data_get(table, key);
        AbDataKind kind = field->boolean ? AB_DATA_BOOLEAN : AB_DATA_NUMBER;
        if (!value || value->kind != kind) {
            continue;
        }
        parameters.present |= field->flag;
        uint8_t *destination = (uint8_t *)&parameters + field->offset;
        if (field->boolean) {
            bool flag = value->bits != 0;
            memcpy(destination, &flag, sizeof(flag));
        } else {
            memcpy(destination, &value->bits, sizeof(value->bits));
        }
    }
    return parameters;
}

static void save_parameters(AbJointParameters *saved, const AbJointParameters *input) {
    for (size_t index = 0; index < sizeof(fields) / sizeof(*fields); ++index) {
        const ParameterField *field = fields + index;
        if (input->present & field->flag) {
            size_t size = field->boolean ? sizeof(bool) : sizeof(float);
            memcpy((uint8_t *)saved + field->offset,
                   (const uint8_t *)input + field->offset, size);
        }
    }
    saved->present |= input->present;
}

bool ab_classic_joints_set_parameters(AbClassicJoints *joints, AbSpan name,
                                      const AbDataTable *table, AbError *error) {
    if (!ab_classic_joints_healthy(joints) || (name.size && !name.data) || !table ||
        table->count > table->capacity || (table->count && !table->entries)) {
        return ab_fail(error, "Invalid classic joint parameter source/registry");
    }
    AbClassicJointRecord *record = NULL;
    for (size_t index = 0; index < joints->joint_count; ++index) {
        if (ab_classic_joint_name_matches(joints->storage.joints[index].definition.name,
                                          name)) {
            record = joints->storage.joints + index;
            break;
        }
    }
    if (!record) {
        return true;
    }
    if (!ab_world_has_joint(joints->world, record->world_joint)) {
        return ab_fail(error, "Classic parameter record has no live engine joint");
    }
    AbJointKind kind =
        joints->world->storage.joints[record->world_joint].constraint.kind;
    uint32_t allowed =
        kind == AB_JOINT_DISTANCE ? AB_JOINT_SET_FREQUENCY | AB_JOINT_SET_DAMPING
        : kind == AB_JOINT_REVOLUTE || kind == AB_JOINT_PRISMATIC ? UINT32_C(0x3f)
                                                                  : 0;
    AbJointParameters parameters = read_parameters(table, allowed);
    AbClassicJointRecord *metadata = ab_classic_joints_find_metadata(joints, name);
    if (parameters.present && !metadata) {
        return ab_fail(error, "Classic parameter assignment has no named metadata");
    }
    if (!ab_world_set_joint_parameters(joints->world, record->world_joint, &parameters,
                                       error)) {
        return false;
    }
    if (metadata) {
        save_parameters(&metadata->parameter_updates, &parameters);
    }
    return true;
}
