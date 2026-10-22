#include "ab/process_manager.h"

#include "ab/object_property_access.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AbDataTable table;
    AbDataEntry entries[2];
} ProcessNotice;

struct AbProcessManager {
    AbProcessManagerConfig config;
    AbDataTable state, *generations;
    AbDataEntry state_entries[6], *entries;
    ProcessNotice *notices;
    size_t generation_count, notice_count, bytes;
    bool updating, resetting, failed;
};

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue number(float value) {
    AbDataValue output = {.kind = AB_DATA_NUMBER};
    memcpy(&output.bits, &value, sizeof(value));
    return output;
}

static AbDataValue table(AbDataTable *value) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = value};
}

static AbDataValue field(const AbDataTable *source, const char *name) {
    const AbDataValue *value = ab_data_get(source, text(name));
    return value ? *value : (AbDataValue){0};
}

static bool valid_table(const AbDataTable *value) {
    return value && value->count <= value->capacity &&
           (!value->count || value->entries);
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool literal(AbDataValue value, bool wanted) {
    return value.kind == AB_DATA_BOOLEAN && !!value.bits == wanted;
}

static bool same(AbDataValue a, AbDataValue b) {
    if (a.kind != b.kind)
        return false;
    switch (a.kind) {
        case AB_DATA_NIL:
            return true;
        case AB_DATA_BOOLEAN:
            return !!a.bits == !!b.bits;
        case AB_DATA_NUMBER: {
            float x, y;
            memcpy(&x, &a.bits, sizeof(x));
            memcpy(&y, &b.bits, sizeof(y));
            return x == y;
        }
        case AB_DATA_STRING:
            return a.string.size == b.string.size &&
                   (!a.string.size ||
                    (a.string.data && b.string.data &&
                     !memcmp(a.string.data, b.string.data, a.string.size)));
        case AB_DATA_TABLE:
            return a.table == b.table;
        case AB_DATA_OBJECT:
            return a.object == b.object;
    }
    return false;
}

static bool put(AbDataTable *target, const char *name, AbDataValue value,
                AbError *error) {
    AbObjectPropertyAccess access = {.physics_object = table(target)};
    return ab_object_property_set(&access, text(name).string, value, error);
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Nonnumeric reached ProcessManager field");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) ||
           ab_fail(error, "Nonfinite reached ProcessManager field");
}

static AbDataTable *current_actions(AbProcessManager *manager, AbError *error) {
    AbDataValue value = field(&manager->state, "actions");
    for (size_t index = 0; index < manager->generation_count; ++index) {
        AbDataTable *candidate = manager->generations + index;
        if (value.kind == AB_DATA_TABLE && value.table == candidate &&
            valid_table(candidate))
            return candidate;
    }
    ab_fail(error, "Unsupported replaced ProcessManager actions table");
    return NULL;
}

static AbProcessAction *binding(AbProcessManager *manager, AbDataValue value,
                                AbError *error) {
    for (size_t index = 0; index < manager->config.binding_count; ++index) {
        AbProcessAction *action = manager->config.bindings[index];
        if (value.kind == AB_DATA_TABLE && value.table == action->table &&
            valid_table(action->table))
            return action;
    }
    ab_fail(error, "Unprepared ProcessManager action identity");
    return NULL;
}

static bool fresh_actions(AbProcessManager *manager, AbError *error) {
    if (manager->generation_count == manager->config.generations)
        return ab_fail(error, "ProcessManager actions generation limit exhausted");
    AbDataTable *actions = manager->generations + manager->generation_count;
    *actions = (AbDataTable){manager->entries +
                                 manager->generation_count * manager->config.actions,
                             0, manager->config.actions, NULL};
    ++manager->generation_count;
    return put(&manager->state, "actions", table(actions), error);
}

bool ab_process_manager_open(AbProcessManagerConfig config, const AbDataTable *info,
                             AbProcessManager **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing ProcessManager output");
    *output = NULL;
    if (!config.events || !config.finished_event || !config.actions ||
        config.actions > (1u << 24) || !config.generations ||
        (config.binding_count && !config.bindings) || (info && !valid_table(info)) ||
        config.storage_bytes < sizeof(AbProcessManager))
        return ab_fail(error, "Invalid ProcessManager storage or bindings");
    for (size_t index = 0; index < config.binding_count; ++index) {
        if (!config.bindings[index] || !valid_table(config.bindings[index]->table))
            return ab_fail(error, "Invalid prepared ProcessManager action");
        for (size_t earlier = 0; earlier < index; ++earlier)
            if (config.bindings[index]->table == config.bindings[earlier]->table)
                return ab_fail(error, "Repeated prepared ProcessManager identity");
    }
    size_t remaining = config.storage_bytes - sizeof(AbProcessManager);
    if (config.generations > remaining / sizeof(AbDataTable))
        return ab_fail(error, "ProcessManager generations exceed byte limit");
    remaining -= config.generations * sizeof(AbDataTable);
    if (config.actions > remaining / sizeof(AbDataEntry) / config.generations)
        return ab_fail(error, "ProcessManager actions exceed byte limit");
    size_t entries = config.generations * config.actions;
    remaining -= entries * sizeof(AbDataEntry);
    if (config.notifications > remaining / sizeof(ProcessNotice))
        return ab_fail(error, "ProcessManager notices exceed byte limit");
    AbProcessManager *manager = calloc(1, sizeof(*manager));
    if (!manager)
        return ab_fail(error, "Cannot allocate ProcessManager");
    manager->config = config;
    manager->generations = calloc(config.generations, sizeof(*manager->generations));
    manager->entries = calloc(entries, sizeof(*manager->entries));
    if (config.notifications)
        manager->notices = calloc(config.notifications, sizeof(*manager->notices));
    manager->bytes =
        config.storage_bytes - remaining + config.notifications * sizeof(ProcessNotice);
    if (!manager->generations || !manager->entries ||
        (config.notifications && !manager->notices)) {
        ab_process_manager_close(manager);
        return ab_fail(error, "Cannot allocate ProcessManager retained storage");
    }
    manager->state = (AbDataTable){manager->state_entries, 0, 6, NULL};
    bool okay =
        put(&manager->state, "x", field(info, "x"), error) &&
        put(&manager->state, "y", field(info, "y"), error) &&
        put(&manager->state, "name", text("ProcessManager"), error) &&
        put(&manager->state, "elapsed", number(0), error) &&
        fresh_actions(manager, error) &&
        put(&manager->state, "running", (AbDataValue){.kind = AB_DATA_BOOLEAN}, error);
    if (!okay) {
        ab_process_manager_close(manager);
        return false;
    }
    *output = manager;
    return true;
}

void ab_process_manager_close(AbProcessManager *manager) {
    if (manager) {
        free(manager->generations);
        free(manager->entries);
        free(manager->notices);
        free(manager);
    }
}

size_t ab_process_manager_bytes(const AbProcessManager *manager) {
    return manager ? manager->bytes : 0;
}

AbDataTable *ab_process_manager_state(AbProcessManager *manager) {
    return manager ? &manager->state : NULL;
}

bool ab_process_manager_start(AbProcessManager *manager, AbError *error) {
    if (!manager || manager->failed)
        return ab_fail(error, "Invalid or stopped ProcessManager start");
    bool okay = put(&manager->state, "running",
                    (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1}, error) &&
                put(&manager->state, "elapsed", number(0), error);
    manager->failed = manager->failed || !okay;
    return !manager->failed;
}

bool ab_process_manager_insert(AbProcessManager *manager, AbProcessAction *action,
                               AbError *error) {
    if (!manager || manager->failed || !action)
        return ab_fail(error, "Invalid or stopped ProcessManager insertion");
    AbDataTable *actions = current_actions(manager, error);
    bool okay = actions && binding(manager, table(action->table), error) == action;
    if (okay && actions->count == actions->capacity)
        okay = ab_fail(error, "ProcessManager action capacity exhausted");
    if (okay) {
        actions->entries[actions->count] =
            (AbDataEntry){number((float)actions->count + 1), table(action->table)};
        ++actions->count;
    }
    manager->failed = manager->failed || !okay;
    return !manager->failed;
}

static bool finish(AbProcessAction *action, AbError *error) {
    return !action->on_finish || action->on_finish(action->context, error);
}

static bool notify(AbProcessManager *manager, AbProcessAction *action, bool reset_one,
                   AbError *error) {
    if (manager->notice_count == manager->config.notifications)
        return ab_fail(error, "ProcessManager retained notice capacity exhausted");
    ProcessNotice *notice = manager->notices + manager->notice_count++;
    notice->entries[0] = (AbDataEntry){text("id"), *manager->config.finished_event};
    notice->entries[1] =
        (AbDataEntry){reset_one ? number(1) : text("target"), table(action->table)};
    notice->table = (AbDataTable){notice->entries, 2, 2, NULL};
    return ab_event_manager_notify(manager->config.events, &notice->table, error);
}

static bool log_action(AbProcessManager *manager, AbProcessAction *action,
                       AbProcessLog kind, AbError *error) {
    return !manager->config.log ||
           manager->config.log(manager->config.context, kind,
                               field(action->table, "name"), error);
}

static bool reset(AbProcessManager *manager, AbError *error) {
    if (!put(&manager->state, "running", (AbDataValue){.kind = AB_DATA_BOOLEAN},
             error) ||
        !put(&manager->state, "elapsed", number(0), error))
        return false;
    AbDataTable *captured = current_actions(manager, error);
    if (!captured)
        return false;
    for (size_t index = 0; index < captured->count; ++index) {
        AbProcessAction *action =
            binding(manager, captured->entries[index].value, error);
        if (!action || !log_action(manager, action, AB_PROCESS_RESET_LOG, error) ||
            !finish(action, error) || !notify(manager, action, false, error))
            return false;
    }
    return fresh_actions(manager, error);
}

bool ab_process_manager_reset(AbProcessManager *manager, AbError *error) {
    if (!manager || manager->failed || manager->resetting)
        return ab_fail(error, "Invalid or reentrant ProcessManager reset");
    manager->resetting = true;
    bool okay = reset(manager, error);
    manager->resetting = false;
    manager->failed = manager->failed || !okay;
    return !manager->failed;
}

bool ab_process_manager_reset_process(AbProcessManager *manager, AbDataValue id,
                                      AbError *error) {
    if (!manager || manager->failed || manager->resetting)
        return ab_fail(error, "Invalid or reentrant ProcessManager resetProcess");
    manager->resetting = true;
    AbDataTable *captured = current_actions(manager, error);
    bool okay = captured != NULL;
    for (size_t index = 0; okay && index < captured->count; ++index) {
        AbProcessAction *action =
            binding(manager, captured->entries[index].value, error);
        if (!action) {
            okay = false;
        } else if (id.kind != AB_DATA_NIL && same(field(action->table, "id"), id)) {
            okay = put(action->table, "finished",
                       (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1}, error) &&
                   finish(action, error) &&
                   log_action(manager, action, AB_PROCESS_RESET_ONE_LOG, error) &&
                   notify(manager, action, true, error);
        }
    }
    manager->resetting = false;
    manager->failed = manager->failed || !okay;
    return !manager->failed;
}

static bool call_action(AbProcessAction *action, float elapsed, float seconds,
                        AbError *error) {
    float duration;
    if (!numeric(field(action->table, "duration"), &duration, error))
        return false;
    if (!action->action)
        return ab_fail(error, "Missing reached ProcessManager action function");
    return action->action(action->context, field(action->table, "item"), elapsed,
                          duration, seconds, field(action->table, "meta"), error);
}

static bool update(AbProcessManager *manager, float seconds, AbDataValue *skip_input,
                   AbError *error) {
    if (!literal(field(&manager->state, "running"), true))
        return true;
    float elapsed;
    if (!numeric(field(&manager->state, "elapsed"), &elapsed, error) ||
        !isfinite(seconds) || !isfinite(elapsed + seconds))
        return ab_fail(error, "Unsupported ProcessManager elapsed arithmetic");
    if (!put(&manager->state, "elapsed", number(elapsed + seconds), error))
        return false;
    if (!skip_input)
        return ab_fail(error, "Missing actual ProcessManager skipInput global");
    *skip_input = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    AbDataTable *captured = current_actions(manager, error);
    if (!captured)
        return false;
    bool all_finished = true;
    for (size_t index = 0; index < captured->count; ++index) {
        AbProcessAction *action =
            binding(manager, captured->entries[index].value, error);
        float current, start, duration;
        if (!action || !numeric(field(&manager->state, "elapsed"), &current, error) ||
            !numeric(field(action->table, "start"), &start, error) ||
            !isfinite(current - start))
            return ab_fail(error, "Unsupported ProcessManager action timing");
        float time = current - start;
        if (literal(field(action->table, "finished"), false))
            all_finished = false;
        if (!numeric(field(action->table, "duration"), &duration, error))
            return false;
        if (duration < time && literal(field(action->table, "finished"), false)) {
            time = duration;
            if (!call_action(action, time, seconds, error) ||
                !put(action->table, "finished",
                     (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1}, error) ||
                !finish(action, error))
                return false;
        }
        if (!numeric(field(action->table, "start"), &start, error) ||
            !numeric(field(&manager->state, "elapsed"), &current, error))
            return false;
        if (start <= current && !truthy(field(action->table, "finished")) &&
            !call_action(action, time, seconds, error))
            return false;
    }
    AbDataTable *current = current_actions(manager, error);
    return current && (!current->count || !all_finished ||
                       ab_process_manager_reset(manager, error));
}

bool ab_process_manager_update(AbProcessManager *manager, float seconds,
                               AbDataValue *skip_input, AbError *error) {
    if (!manager || manager->failed || manager->updating || manager->resetting)
        return ab_fail(error, "Invalid or reentrant ProcessManager update");
    manager->updating = true;
    bool okay = update(manager, seconds, skip_input, error);
    manager->updating = false;
    manager->failed = manager->failed || !okay;
    return !manager->failed;
}
