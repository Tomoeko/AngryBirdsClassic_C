#include "ab/object_removal_notify.h"

#include <string.h>

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static AbDataValue key(const char *text) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)text, strlen(text)}};
}

static bool field(AbDataValue table, AbDataValue name, AbDataValue *output,
                  AbError *error) {
    if (table.kind != AB_DATA_TABLE || !table.table ||
        table.table->count > table.table->capacity ||
        (table.table->count && !table.table->entries)) {
        return ab_fail(error, "Object removal notification indexes a non-table");
    }
    const AbDataValue *found = ab_data_get(table.table, name);
    *output = found ? *found : (AbDataValue){0};
    return true;
}

static bool call(AbObjectRemovalNotifyHost *host, AbObjectRemovalNotice notice,
                 AbUiPointerValue function, AbObjectRemovalObject *object,
                 AbError *error) {
    if (function.kind != AB_UI_POINTER_FUNCTION || !function.function || !host->call) {
        return ab_fail(error, "Missing callable object removal notification binding");
    }
    return host->call(host->context, notice, function, object, error);
}

static bool definition_notice(AbObjectRemovalObject *object,
                              AbObjectRemovalNotifyHost *host, AbError *error) {
    AbDataValue blocks = {0}, raw = {0}, name = {0};
    if (!field(host->block_table, key("blocks"), &blocks, error) ||
        !field(blocks, object->definition, &raw, error) ||
        !field(raw, key("onRemoved"), &name, error)) {
        return false;
    }
    if (!truthy(name)) {
        return true;
    }
    if (!host->binding) {
        return ab_fail(error, "Missing object removal environment lookup");
    }
    AbUiPointerValue function = {0};
    if (!host->binding(host->context, name, &function, error)) {
        return false;
    }
    if (!ab_ui_pointer_value_truthy(&function)) {
        return true;
    }
    /* The original repeats both raw-definition key and binding reads. */
    if (!field(raw, key("onRemoved"), &name, error) ||
        !host->binding(host->context, name, &function, error)) {
        return false;
    }
    return call(host, AB_OBJECT_REMOVAL_DEFINITION, function, object, error);
}

bool ab_object_removal_notify(AbObjectRemovalObject *object,
                              AbObjectRemovalNotifyHost *host, AbError *error) {
    if (!object || !host) {
        return ab_fail(error, "Missing object removal notification input");
    }
    if (!definition_notice(object, host, error)) {
        return false;
    }
    if (!truthy(host->level_name)) {
        return true;
    }
    if (!host->level) {
        return ab_fail(error, "Missing object removal level lookup");
    }
    AbObjectRemovalLevel *level = NULL;
    if (!host->level(host->context, host->level_name, &level, error)) {
        return false;
    }
    if (!level) {
        return true;
    }
    if (!host->level(host->context, host->level_name, &level, error)) {
        return false;
    }
    if (!level) {
        return ab_fail(error, "Missing repeated object removal level entry");
    }
    if (!ab_ui_pointer_value_truthy(&level->on_remove)) {
        return true;
    }
    if (!host->level(host->context, host->level_name, &level, error)) {
        return false;
    }
    if (!level) {
        return ab_fail(error, "Missing final object removal level entry");
    }
    return call(host, AB_OBJECT_REMOVAL_LEVEL, level->on_remove, object, error);
}
