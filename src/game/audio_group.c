#include "ab/audio_group.h"

#include <string.h>

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static bool dense_group(const AbDataTable *group) {
    if (!group || group->count > group->capacity ||
        group->count > UINT32_C(0x1000000) || (group->count && !group->entries)) {
        return false;
    }
    for (size_t index = 0; index < group->count; ++index) {
        const AbDataValue *value = ab_data_get(group, number((float)(index + 1)));
        if (!value || value->kind == AB_DATA_NIL) {
            return false;
        }
    }
    return true;
}

bool ab_audio_group_name(const AbDataTable *groups, AbDataValue requested,
                         AbRandom48 *random, AbDataValue *output, AbError *error) {
    if (!groups || !output || groups->count > groups->capacity ||
        (groups->count && !groups->entries) ||
        (unsigned)requested.kind > AB_DATA_STRING ||
        (requested.kind == AB_DATA_STRING && requested.string.size &&
         !requested.string.data)) {
        return ab_fail(error, "Invalid audio-group selection storage or request");
    }
    const AbDataValue *value = ab_data_get(groups, requested);
    if (!value || value->kind == AB_DATA_NIL) {
        *output = requested;
        return true;
    }
    if (value->kind != AB_DATA_TABLE || !dense_group(value->table)) {
        return ab_fail(error, "Unsupported sparse or non-table audio group");
    }
    float selected;
    AbScriptRandomResult status =
        ab_script_random(random, 2, 1, (int32_t)value->table->count, &selected);
    if (status != AB_SCRIPT_RANDOM_OK) {
        return ab_fail(error, status == AB_SCRIPT_RANDOM_EMPTY_INTERVAL
                                  ? "Empty audio group consumed a random draw"
                                  : "Invalid audio-group random state");
    }
    const AbDataValue *choice = ab_data_get(value->table, number(selected));
    *output = choice ? *choice : (AbDataValue){0};
    return true;
}
