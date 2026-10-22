#include "ab/block_event.h"

#include <string.h>

static bool valid_span(AbSpan value) {
    return !value.size || value.data;
}

static bool valid_arguments(const AbDataValue *arguments, size_t count) {
    if ((count && !arguments) || count > UINT32_C(16777216)) {
        return false;
    }
    for (size_t index = 0; index < count; ++index) {
        const AbDataValue *value = arguments + index;
        if ((unsigned)value->kind > AB_DATA_TABLE ||
            (value->kind == AB_DATA_STRING && !valid_span(value->string)) ||
            (value->kind == AB_DATA_TABLE && !value->table)) {
            return false;
        }
    }
    return true;
}

static bool named(AbSpan left, AbSpan right) {
    return left.size == right.size &&
           (!left.size || memcmp(left.data, right.data, left.size) == 0);
}

static bool find_handler(AbBlockEventScript *script, AbSpan event,
                         AbBlockEventFunction *output, AbError *error) {
    if (!script || (script->count && !script->bindings) ||
        script->count > UINT32_C(16777216)) {
        return ab_fail(error, "Invalid block event script bindings");
    }
    *output = NULL;
    for (size_t index = 0; index < script->count; ++index) {
        const AbBlockEventBinding *binding = script->bindings + index;
        if (!valid_span(binding->name)) {
            return ab_fail(error, "Invalid block event binding name");
        }
        if (named(binding->name, event)) {
            *output = binding->function;
            return true;
        }
    }
    return true;
}

bool ab_block_event(AbBlockEventObject *object, AbSpan event,
                    const AbDataValue *arguments, size_t count, AbBlockEventHost host,
                    AbError *error) {
    if (!object || !valid_span(event) || !valid_arguments(arguments, count)) {
        return ab_fail(error, "Invalid block event request");
    }
    if (!object->scripts_present) {
        return true;
    }
    if ((object->count && !object->scripts) || object->count > UINT32_C(16777216)) {
        return ab_fail(error, "Invalid attached block scripts");
    }
    for (size_t index = 0; index < object->count; ++index) {
        AbBlockEventScript *script = object->scripts[index];
        AbBlockEventFunction function;
        if (!find_handler(script, event, &function, error)) {
            return false;
        }
        if (!function) {
            continue;
        }
        if (!host.definition) {
            return ab_fail(error, "Missing block event definition provider");
        }
        const AbDataTable *definition = NULL;
        if (!host.definition(host.context, object, &definition, error) ||
            !function(script, object, definition, arguments, count, error)) {
            return false;
        }
    }
    return true;
}

bool ab_block_global_event(AbBlockEventObject *const *objects, size_t object_count,
                           AbSpan event, const AbDataValue *arguments, size_t count,
                           AbBlockEventHost host, AbError *error) {
    if ((object_count && (!objects || !host.world_truthy)) ||
        object_count > UINT32_C(16777216) || !valid_span(event) ||
        !valid_arguments(arguments, count)) {
        return ab_fail(error, "Invalid global block event request");
    }
    for (size_t index = 0; index < object_count; ++index) {
        AbBlockEventObject *object = objects[index];
        if (!object || !valid_span(object->name)) {
            return ab_fail(error, "Invalid tracked block event object");
        }
        if (!object->tracked) {
            continue;
        }
        bool present = false;
        if (!host.world_truthy(host.context, object->name, &present, error)) {
            return false;
        }
        if (!present) {
            object->tracked = false;
            continue;
        }
        if (!ab_block_event(object, event, arguments, count, host, error)) {
            return false;
        }
    }
    return true;
}
