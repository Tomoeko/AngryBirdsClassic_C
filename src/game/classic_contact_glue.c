#include "ab/classic_joints.h"

bool ab_classic_glue_queue_contact(AbClassicGlueQueue *queue, AbDataValue response,
                                   AbClassicGlueBodyLookup lookup, void *context,
                                   AbError *error) {
    if (!queue || queue->count > queue->capacity ||
        queue->capacity > SIZE_MAX / sizeof(*queue->pairs) ||
        (queue->capacity && !queue->pairs)) {
        return ab_fail(error, "Invalid classic contact glue queue");
    }
    if (response.kind != AB_DATA_TABLE) {
        return true;
    }
    const AbDataTable *table = response.table;
    if (!table || table->count > table->capacity || (table->count && !table->entries) ||
        !lookup) {
        return ab_fail(error, "Invalid classic contact glue response/lookup");
    }
    const AbDataValue *first = ab_data_get(
        table, (AbDataValue){.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3f800000)});
    const AbDataValue *second = ab_data_get(
        table, (AbDataValue){.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x40000000)});
    if (!first || !second || first->kind != AB_DATA_STRING ||
        second->kind != AB_DATA_STRING || (first->string.size && !first->string.data) ||
        (second->string.size && !second->string.data)) {
        return ab_fail(error, "Classic contact glue requires two string endpoints");
    }
    if (queue->count == queue->capacity) {
        return ab_fail(error, "Classic contact glue queue capacity exceeded");
    }
    AbClassicGluePair pair;
    if (!lookup(context, first->string, &pair.first_body, error) ||
        !lookup(context, second->string, &pair.second_body, error)) {
        return false;
    }
    queue->pairs[queue->count++] = pair;
    return true;
}
