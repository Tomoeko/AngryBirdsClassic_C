#include "ab/classic_joint_updates.h"

#include "ab/level_joints.h"
#include "ab/string_map.h"
#include "classic_joints_internal.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    AbDataEntry fields[24];
    AbDataTable table;
} JointMetadata;

typedef struct {
    AbDataEntry fields[2];
    AbDataTable event;
} JointNotification;

struct AbClassicJointUpdates {
    AbClassicJointUpdateConfig config;
    AbStringMap *metadata;
    JointMetadata *records;
    AbGameJointEntry *entries;
    JointNotification *notifications;
    AbGameJointQueue queue;
    AbGameTailJoints tail;
    size_t used, notification_count;
    bool calling, failed;
};

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static void add(JointMetadata *metadata, const char *name, AbDataValue value) {
    if (value.kind != AB_DATA_NIL)
        metadata->fields[metadata->table.count++] = (AbDataEntry){text(name), value};
}

static void numeric(JointMetadata *metadata, const char *name, float value) {
    add(metadata, name, number(value));
}

static void boolean(JointMetadata *metadata, const char *name, bool value) {
    add(metadata, name, (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = value ? 1 : 0});
}

/* GameLua::createJointLua's measured saved fields, with the parameter owner's
 * current saved values. See the game_joint/game_joint_parameters native corpora. */
static void project(JointMetadata *metadata, const AbClassicJointRecord *record) {
    const AbClassicJoint *joint = &record->definition;
    metadata->table = (AbDataTable){metadata->fields, 0, 24, NULL};
    add(metadata, "name", (AbDataValue){.kind = AB_DATA_STRING, .string = joint->name});
    add(metadata, "end1",
        (AbDataValue){.kind = AB_DATA_STRING, .string = joint->endpoints[0]});
    add(metadata, "end2",
        (AbDataValue){.kind = AB_DATA_STRING, .string = joint->endpoints[1]});
    numeric(metadata, "type", (float)joint->type);
    numeric(metadata, "coordType", (float)joint->coordinate_type);
    numeric(metadata, "x1", joint->coordinates[0].x);
    numeric(metadata, "y1", joint->coordinates[0].y);
    numeric(metadata, "x2", joint->coordinates[1].x);
    numeric(metadata, "y2", joint->coordinates[1].y);
    add(metadata, "breakable", joint->breakable);
    add(metadata, "breakForce", joint->break_force);
    if (joint->type == AB_CLASSIC_JOINT_GLUE) {
        numeric(metadata, "destroyTimer", joint->destroy_timer);
        return;
    }
    boolean(metadata, "collideConnected", joint->joint.collide_connected);
    static const char *const parameters[] = {"motor",     "motorSpeed",  "maxTorque",
                                             "limit",     "lowerLimit",  "upperLimit",
                                             "frequency", "dampingRatio"};
    static const uint32_t flags[] = {AB_JOINT_SET_MOTOR,     AB_JOINT_SET_SPEED,
                                     AB_JOINT_SET_MAXIMUM,   AB_JOINT_SET_LIMIT,
                                     AB_JOINT_SET_LOWER,     AB_JOINT_SET_UPPER,
                                     AB_JOINT_SET_FREQUENCY, AB_JOINT_SET_DAMPING};
    for (size_t index = 0; index < 8; ++index)
        add(metadata, parameters[index],
            ab_classic_joint_parameter(record, flags[index]));
    if (joint->type == AB_CLASSIC_JOINT_REVOLUTE ||
        joint->type == AB_CLASSIC_JOINT_PRISMATIC)
        boolean(metadata, "backAndForth", joint->back_and_forth);
    if (joint->type == AB_CLASSIC_JOINT_PRISMATIC) {
        numeric(metadata, "worldAxisX", joint->world_axis.x);
        numeric(metadata, "worldAxisY", joint->world_axis.y);
    }
}

bool ab_classic_joint_updates_register(AbClassicJointUpdates *updates, AbSpan name,
                                       AbError *error) {
    if (!updates || updates->calling || updates->failed ||
        updates->used == updates->config.names)
        return ab_fail(error, "Invalid/full joint metadata registration");
    const AbClassicJointRecord *record =
        ab_classic_joints_metadata(updates->config.joints, name);
    if (!record)
        return ab_fail(error, "New joint has no visible native metadata");
    JointMetadata *metadata = updates->records + updates->used;
    project(metadata, record);
    if (!ab_string_map_set(updates->metadata, name, (uintptr_t)metadata, true, error))
        return false;
    ++updates->used;
    return true;
}

bool ab_classic_joint_updates_refresh(AbClassicJointUpdates *updates, AbError *error) {
    if (!updates || updates->failed || updates->calling ||
        !ab_classic_joints_healthy(updates->config.joints))
        return ab_fail(error, "Invalid joint update refresh");
    size_t cursor = 0, count = 0;
    for (;;) {
        AbStringMapEntry entry;
        if (!ab_string_map_next(updates->metadata, &cursor, &entry, error))
            return false;
        if (!entry.present)
            break;
        const AbClassicJointRecord *record =
            ab_classic_joints_metadata(updates->config.joints, entry.key);
        if (!record) {
            if (!ab_string_map_set(updates->metadata, entry.key, 0, false, error))
                return false;
            continue;
        }
        updates->entries[count++] =
            (AbGameJointEntry){entry.key, record->definition.back_and_forth};
    }
    updates->tail.count = count;
    return true;
}

static bool initial_metadata(AbClassicJointUpdates *updates, AbError *error) {
    const AbLuaData *data = updates->config.level_data;
    AbLevelJointRequirements requirements;
    if (!ab_level_joint_requirements(data, &requirements, error))
        return false;
    const AbDataValue *value = ab_data_get(data->globals, text("joints"));
    if (!value || value->kind == AB_DATA_NIL)
        return true;
    size_t count = value->table->count;
    size_t retained =
        sizeof(*updates) +
        updates->config.names * (sizeof(JointMetadata) + sizeof(AbGameJointEntry)) +
        updates->config.requests * (sizeof(AbSpan) + sizeof(JointNotification)) +
        ab_string_map_bytes(updates->metadata);
    size_t remaining = updates->config.storage_bytes - retained;
    if (count > remaining / sizeof(size_t))
        return ab_fail(error, "Initial joint order exceeds storage");
    remaining -= count * sizeof(size_t);
    if (requirements.hash_capacity > remaining / sizeof(AbStringOrderNode))
        return ab_fail(error, "Initial joint hash scratch exceeds storage");
    size_t *indices = count ? calloc(count, sizeof(*indices)) : NULL;
    AbStringOrderNode *nodes = requirements.hash_capacity
                                   ? calloc(requirements.hash_capacity, sizeof(*nodes))
                                   : NULL;
    if ((count && !indices) || (requirements.hash_capacity && !nodes)) {
        free(indices);
        free(nodes);
        return ab_fail(error, "Cannot allocate initial joint order scratch");
    }
    AbStringOrderWorkspace order = {indices, count, nodes, requirements.hash_capacity};
    size_t live = 0;
    bool okay = ab_data_string_order_pruned(value->table, requirements.hash_capacity,
                                            &order, &live, error);
    for (size_t index = 0; okay && index < live; ++index) {
        const AbDataTable *source = value->table->entries[indices[index]].value.table;
        const AbDataValue *name = ab_data_get(source, text("name"));
        okay = name && name->kind == AB_DATA_STRING &&
               ab_classic_joint_updates_register(updates, name->string, error);
        if (!okay && (!error || !error->message[0]))
            ab_fail(error, "Initial joint has no registered name");
    }
    free(indices);
    free(nodes);
    return okay;
}

bool ab_classic_joint_updates_open(AbClassicJointUpdateConfig config,
                                   AbClassicJointUpdates **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing joint updates output");
    *output = NULL;
    size_t per_name = sizeof(JointMetadata) + sizeof(AbGameJointEntry);
    if (!ab_classic_joints_healthy(config.joints) || config.joints->world->locked ||
        !config.level_data || !config.level_data->globals || !config.events ||
        !config.print_destroy || config.destroyed_event.kind == AB_DATA_NIL ||
        !config.names || config.names > UINT32_C(0x1000000) || !config.requests ||
        config.storage_bytes < sizeof(AbClassicJointUpdates) ||
        config.names >
            (config.storage_bytes - sizeof(AbClassicJointUpdates)) / per_name)
        return ab_fail(error, "Invalid joint updates bindings or storage");
    size_t remaining =
        config.storage_bytes - sizeof(AbClassicJointUpdates) - config.names * per_name;
    size_t per_request = sizeof(AbSpan) + sizeof(JointNotification);
    if (config.requests > remaining / per_request)
        return ab_fail(error, "Joint destruction queue exceeds storage");
    remaining -= config.requests * per_request;
    size_t nodes = 1;
    while (nodes < config.names)
        nodes *= 2;
    AbClassicJointUpdates *updates = calloc(1, sizeof(*updates));
    if (!updates)
        return ab_fail(error, "Cannot allocate joint updates");
    updates->config = config;
    updates->records = calloc(config.names, sizeof(*updates->records));
    updates->entries = calloc(config.names, sizeof(*updates->entries));
    updates->queue.names = calloc(config.requests, sizeof(*updates->queue.names));
    updates->notifications = calloc(config.requests, sizeof(*updates->notifications));
    updates->queue.capacity = config.requests;
    updates->tail = (AbGameTailJoints){updates->entries, 0, &updates->queue};
    bool okay = updates->records && updates->entries && updates->queue.names &&
                updates->notifications &&
                ab_string_map_open(0, nodes, remaining, &updates->metadata, error) &&
                initial_metadata(updates, error) &&
                ab_classic_joint_updates_refresh(updates, error);
    if (!okay) {
        if (!error || !error->message[0])
            ab_fail(error, "Cannot allocate joint update storage");
        ab_classic_joint_updates_close(updates);
        return false;
    }
    *output = updates;
    return true;
}

void ab_classic_joint_updates_close(AbClassicJointUpdates *updates) {
    if (!updates)
        return;
    ab_string_map_close(updates->metadata);
    free(updates->records);
    free(updates->entries);
    free(updates->queue.names);
    free(updates->notifications);
    free(updates);
}

AbGameTailJoints *ab_classic_joint_updates_tail(AbClassicJointUpdates *updates) {
    return updates ? &updates->tail : NULL;
}

size_t ab_classic_joint_updates_notifications(const AbClassicJointUpdates *updates) {
    return updates ? updates->notification_count : 0;
}

bool ab_classic_joint_updates_defer(void *context, AbSpan name, AbError *error) {
    AbClassicJointUpdates *updates = context;
    if (!updates || updates->failed)
        return ab_fail(error, "Missing/failed deferred joint update owner");
    return ab_game_joint_defer(&updates->queue, name, error);
}

static bool destroy(AbClassicJointUpdates *updates, AbSpan name, AbError *error) {
    AbClassicJointUpdateConfig *config = &updates->config;
    if (!config->print_destroy(config->context, name, error))
        return false;
    uintptr_t handle = 0;
    bool present = false;
    if (!ab_string_map_get(updates->metadata, name, &handle, &present, error))
        return false;
    const AbClassicJointRecord *record =
        ab_classic_joints_metadata(config->joints, name);
    if (record && !present)
        return ab_fail(error, "Joint factory omitted its metadata registration");
    if (updates->notification_count == config->requests)
        return ab_fail(error, "Joint destruction notifications exceed storage");
    JointNotification *notification =
        updates->notifications + updates->notification_count++;
    notification->fields[0] = (AbDataEntry){text("id"), config->destroyed_event};
    notification->event = (AbDataTable){notification->fields, 1, 2, NULL};
    if (record) {
        JointMetadata *metadata = (JointMetadata *)handle;
        project(metadata, record);
        notification->fields[1] = (AbDataEntry){
            text("joint"), {.kind = AB_DATA_TABLE, .table = &metadata->table}};
        notification->event.count = 2;
    }
    /* common_logic.lua prototype 24125 notifies before calling the saved native
     * destroyJoint. Repeated queue entries still notify with an absent joint. */
    return ab_event_manager_notify(config->events, &notification->event, error) &&
           ab_classic_joints_remove_named(config->joints, name, error) &&
           ab_string_map_set(updates->metadata, name, 0, false, error);
}

bool ab_classic_joint_updates_call(AbClassicJointUpdates *updates, AbGameTailCall *call,
                                   AbError *error) {
    if (!updates || !call || updates->calling || updates->failed ||
        !ab_classic_joints_healthy(updates->config.joints) ||
        updates->config.joints->world->locked)
        return ab_fail(error, "Invalid/reentrant joint update service");
    updates->calling = true;
    bool okay = call->operation == AB_GAME_TAIL_DESTROY_JOINT
                    ? destroy(updates, call->joint_name, error)
                : call->operation == AB_GAME_TAIL_CHECK_JOINT_LIMIT
                    ? ab_classic_joints_check_limits(updates->config.joints,
                                                     call->joint_name, error)
                    : ab_fail(error, "Unexpected joint update operation");
    updates->calling = false;
    updates->failed = !okay;
    return okay;
}
