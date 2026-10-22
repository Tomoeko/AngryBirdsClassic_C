#include "ab/camera_sequence.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(
        table, (AbDataValue){.kind = AB_DATA_STRING,
                             .string = {(const uint8_t *)name, strlen(name)}});
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool named(AbDataValue value, const char *name) {
    size_t size = strlen(name);
    return value.kind == AB_DATA_STRING && value.string.size == size &&
           value.string.data && memcmp(value.string.data, name, size) == 0;
}

static bool numeric(AbDataValue value, float *output) {
    if (value.kind != AB_DATA_NUMBER) {
        return false;
    }
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output);
}

static bool read_stage(const AbDataTable *properties, AbCameraSequenceStage *output,
                       AbError *error) {
    if (!properties || (properties->count && !properties->entries)) {
        return ab_fail(error, "Invalid camera stage properties");
    }
    AbDataValue action = field(properties, "action");
    if (action.kind != AB_DATA_STRING || !action.string.size || !action.string.data) {
        return ab_fail(error, "Camera sequence action must be a nonempty identifier");
    }
    output->action = action.string;
    output->properties = properties;
    output->interrupt_on_input = truthy(field(properties, "interruptOnUserInput"));
    AbDataValue condition = field(properties, "condition");
    if (named(condition, "noMovingObjects")) {
        output->condition = AB_CAMERA_CONDITION_NO_MOVING;
        if (!numeric(field(properties, "minDuration"), &output->minimum_duration)) {
            return ab_fail(error, "Unsupported camera minimum duration");
        }
    } else if (named(condition, "untilSpecialtyTriggered")) {
        output->condition = AB_CAMERA_CONDITION_SPECIALTY;
    } else if (truthy(condition)) {
        return ab_fail(error,
                       "Camera sequence condition requires an explicit host binding");
    }
    AbDataValue duration = field(properties, "duration");
    output->duration_present = truthy(duration);
    if (output->duration_present && !numeric(duration, &output->duration)) {
        return ab_fail(error, "Unsupported camera sequence duration");
    }
    return true;
}

bool ab_camera_sequence_read(const AbDataTable *table, size_t allocation_limit,
                             AbCameraSequence *output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing prepared camera sequence output");
    }
    *output = (AbCameraSequence){0};
    if (!table || (table->count && !table->entries) ||
        table->count >= UINT32_C(0x1000000) ||
        table->count > allocation_limit / sizeof(AbCameraSequenceStage)) {
        return ab_fail(error, "Camera sequence exceeds input or allocation bounds");
    }
    AbCameraSequence result = {.count = table->count};
    if (result.count) {
        result.stages = calloc(result.count, sizeof(*result.stages));
        if (!result.stages) {
            return ab_fail(error, "Cannot allocate prepared camera sequence stages");
        }
    }
    for (size_t index = 0; index < result.count; ++index) {
        float number = (float)(index + 1);
        AbDataValue key = {.kind = AB_DATA_NUMBER};
        memcpy(&key.bits, &number, sizeof(number));
        const AbDataValue *value = ab_data_get(table, key);
        if (!value || value->kind != AB_DATA_TABLE || !value->table ||
            !read_stage(value->table, result.stages + index, error)) {
            ab_camera_sequence_free(&result);
            return ab_fail(error, "Expected a dense supported camera sequence");
        }
    }
    *output = result;
    return true;
}

void ab_camera_sequence_free(AbCameraSequence *sequence) {
    if (sequence) {
        free(sequence->stages);
        *sequence = (AbCameraSequence){0};
    }
}
