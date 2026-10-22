#include "ab/block_script.h"

#include <string.h>

static bool valid_list(AbBlockScriptList list) {
    return list.count <= list.capacity && list.capacity <= UINT32_C(16777216) &&
           (!list.capacity || list.items);
}

static bool valid_manager(const AbBlockScriptManager *manager) {
    return manager && valid_list(manager->registered) &&
           valid_list(manager->initialized) && manager->count <= manager->capacity &&
           manager->capacity <= UINT32_C(16777216) &&
           (!manager->capacity || manager->tracked);
}

static bool valid_span(AbSpan value) {
    return !value.size || value.data;
}

static bool named(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || memcmp(first.data, second.data, first.size) == 0);
}

static bool initialize_script(AbBlockScriptManager *manager, AbBlockScript *script,
                              AbError *error) {
    for (size_t index = 0; index < manager->initialized.count; ++index) {
        if (manager->initialized.items[index] == script) {
            return true;
        }
    }
    if (manager->initialized.count == manager->initialized.capacity) {
        return ab_fail(error, "Initialized block scripts exceed their capacity");
    }
    manager->initialized.items[manager->initialized.count++] = script;
    return !script->initialize || script->initialize(script, error);
}

static bool track_object(AbBlockScriptManager *manager, AbBlockEventObject *object,
                         AbError *error) {
    if (!valid_span(object->name)) {
        return ab_fail(error, "Invalid applied block script object name");
    }
    size_t found = manager->count;
    bool pruned = false;
    for (size_t index = 0; index < manager->count; ++index) {
        const AbTrackedBlockScriptObject *entry = manager->tracked + index;
        if (!entry->object || !valid_span(entry->name) ||
            !named(entry->name, entry->object->name)) {
            return ab_fail(error, "Unsupported renamed tracked block object");
        }
        pruned = pruned || !entry->object->tracked;
        if (named(entry->name, object->name)) {
            found = index;
        }
    }
    if ((found == manager->count || !manager->tracked[found].object->tracked) &&
        pruned) {
        return ab_fail(error, "Unsupported block script insertion after pruning");
    }
    if (found == manager->count) {
        if (manager->count == manager->capacity) {
            return ab_fail(error, "Tracked block objects exceed their capacity");
        }
        ++manager->count;
    }
    manager->tracked[found] = (AbTrackedBlockScriptObject){object->name, object};
    object->tracked = true;
    return true;
}

bool ab_block_script_register(AbBlockScriptManager *manager, AbBlockScript *script,
                              AbError *error) {
    if (!valid_manager(manager) || !script || !script->can_apply ||
        manager->registered.count == manager->registered.capacity) {
        return ab_fail(error, "Invalid or full block script registration");
    }
    manager->registered.items[manager->registered.count++] = script;
    return true;
}

bool ab_block_scripts_update(AbBlockScriptManager *manager, AbBlockScriptUpdate *update,
                             float seconds, float time, AbError *error) {
    if (!valid_manager(manager) || !update) {
        return ab_fail(error, "Invalid block script update");
    }
    for (size_t index = 0; index < manager->registered.count; ++index) {
        if (!valid_list(manager->registered)) {
            return ab_fail(error, "Invalid mutated block script registrations");
        }
        AbBlockScript *script = manager->registered.items[index];
        if (!script) {
            return ab_fail(error, "Missing registered block script");
        }
        if (script->update && !script->update(script, seconds, time, error)) {
            return false;
        }
    }
    AbDataValue arguments[2] = {{.kind = AB_DATA_NUMBER}, {.kind = AB_DATA_NUMBER}};
    memcpy(&arguments[0].bits, &seconds, sizeof(seconds));
    memcpy(&arguments[1].bits, &time, sizeof(time));
    return ab_block_global_event(update->objects, update->count, update->event,
                                 arguments, 2, update->host, error);
}

bool ab_block_scripts_apply(AbBlockScriptManager *manager, AbBlockEventObject *object,
                            AbBlockEventHost host, AbError *error) {
    if (!valid_manager(manager) || !object || !host.definition ||
        (object->capacity && !object->scripts) ||
        (object->scripts_present && object->count > object->capacity)) {
        return ab_fail(error, "Invalid block script application");
    }
    const AbDataTable *definition = NULL;
    if (!host.definition(host.context, object, &definition, error)) {
        return false;
    }
    if (!object->scripts_present) {
        object->scripts_present = true;
        object->count = 0;
    }
    for (size_t index = 0; index < manager->registered.count; ++index) {
        AbBlockScript *script = manager->registered.items[index];
        bool applicable = false;
        if (!script || !script->can_apply) {
            return ab_fail(error, "Missing block script applicability function");
        }
        if (!script->can_apply(script, definition, &applicable, error)) {
            return false;
        }
        if (!applicable) {
            continue;
        }
        if (!initialize_script(manager, script, error)) {
            return false;
        }
        if (object->count == object->capacity) {
            return ab_fail(error, "Attached block scripts exceed their capacity");
        }
        object->scripts[object->count++] = &script->events;
        if ((script->apply && !script->apply(script, object, definition, error)) ||
            !track_object(manager, object, error)) {
            return false;
        }
    }
    return true;
}

bool ab_block_script_order(const AbBlockScriptManager *manager,
                           AbBlockScriptOrder *order, AbError *error) {
    if (!valid_manager(manager) || !order || manager->count > order->entry_capacity ||
        manager->count > order->object_capacity ||
        (manager->count && (!order->entries || !order->objects))) {
        return ab_fail(error, "Invalid block script order storage");
    }
    for (size_t index = 0; index < manager->count; ++index) {
        const AbTrackedBlockScriptObject *entry = manager->tracked + index;
        if (!entry->object || !valid_span(entry->name) ||
            !named(entry->name, entry->object->name)) {
            return ab_fail(error, "Unsupported tracked block script name");
        }
        order->entries[index] =
            (AbDataEntry){{.kind = AB_DATA_STRING, .string = entry->name},
                          {.kind = AB_DATA_BOOLEAN, .bits = 1}};
    }
    AbDataTable table = {order->entries, manager->count, order->entry_capacity, NULL};
    size_t capacity = 0;
    if (!ab_data_string_order_growing(&table, &order->strings, &capacity, error)) {
        return false;
    }
    for (size_t index = 0; index < manager->count; ++index) {
        order->objects[index] = manager->tracked[order->strings.indices[index]].object;
    }
    order->count = manager->count;
    order->hash_capacity = capacity;
    return true;
}

bool ab_block_scripts_clear(AbBlockScriptManager *manager, const size_t *dispose_order,
                            size_t count, AbError *error) {
    if (!valid_manager(manager) || count != manager->initialized.count ||
        (count && !dispose_order)) {
        return ab_fail(error, "Invalid block script disposal order");
    }
    for (size_t index = 0; index < count; ++index) {
        if (dispose_order[index] >= count) {
            return ab_fail(error, "Invalid initialized block script index");
        }
        for (size_t previous = 0; previous < index; ++previous) {
            if (dispose_order[index] == dispose_order[previous]) {
                return ab_fail(error, "Duplicate initialized block script index");
            }
        }
    }
    for (size_t index = 0; index < count; ++index) {
        AbBlockScript *script = manager->initialized.items[dispose_order[index]];
        if (!script || !script->dispose) {
            return ab_fail(error, "Missing required block script disposal function");
        }
        if (!script->dispose(script, error)) {
            return false;
        }
    }
    for (size_t index = 0; index < manager->count; ++index) {
        if (!manager->tracked[index].object) {
            return ab_fail(error, "Missing tracked block script object");
        }
        manager->tracked[index].object->tracked = false;
    }
    manager->initialized.count = 0;
    manager->count = 0;
    return true;
}
