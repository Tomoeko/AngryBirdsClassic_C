#include "ab/level_joints.h"

#include <math.h>
#include <string.h>

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(
        table, (AbDataValue){.kind = AB_DATA_STRING,
                             .string = {(const uint8_t *)name, strlen(name)}});
    return value ? *value : (AbDataValue){0};
}

static bool joint_table(const AbLuaData *level, const AbDataTable **table) {
    if (!level || !level->globals) {
        return false;
    }
    AbDataValue value = field(level->globals, "joints");
    *table = value.kind == AB_DATA_TABLE ? value.table : NULL;
    return value.kind == AB_DATA_NIL ||
           (*table && (*table)->count <= (*table)->capacity &&
            (!(*table)->count || (*table)->entries));
}

static bool joint_type(const AbDataTable *table, unsigned *type) {
    if (!table || table->count > table->capacity || (table->count && !table->entries)) {
        return false;
    }
    AbDataValue value = field(table, "type");
    float number;
    memcpy(&number, &value.bits, sizeof(number));
    if (value.kind != AB_DATA_NUMBER || !isfinite(number) || number < 1 || number > 5 ||
        floorf(number) != number) {
        return false;
    }
    *type = (unsigned)number;
    return true;
}

bool ab_level_joint_requirements(const AbLuaData *level,
                                 AbLevelJointRequirements *output, AbError *error) {
    const AbDataTable *table;
    if (!output || !joint_table(level, &table)) {
        return ab_fail(error, "Invalid classic level joint table");
    }
    AbLevelJointRequirements requirements = {0};
    if (table && table->count) {
        const AbDataTableLayout *layout = ab_lua_table_layout(level, table);
        if (!layout || !layout->no_nil_assignments ||
            table->count > layout->hash_capacity ||
            layout->hash_capacity > UINT32_MAX ||
            (layout->hash_capacity & (layout->hash_capacity - 1))) {
            return ab_fail(error, "Unsupported classic joint constructor layout");
        }
        requirements.hash_capacity = layout->hash_capacity;
        for (size_t index = 0; index < table->count; ++index) {
            const AbDataEntry *entry = table->entries + index;
            unsigned type;
            if (entry->key.kind != AB_DATA_STRING ||
                (entry->key.string.size && !entry->key.string.data) ||
                entry->value.kind != AB_DATA_TABLE ||
                !joint_type(entry->value.table, &type)) {
                return ab_fail(error, "Unsupported classic level joint entry");
            }
            if (type == AB_CLASSIC_JOINT_GLUE) {
                ++requirements.deferred;
            } else {
                ++requirements.immediate;
            }
        }
    }
    *output = requirements;
    return true;
}

static bool resolve_endpoint(const AbClassicJoints *joints, const AbLevelBodies *bodies,
                             AbSpan name, size_t *body_index, AbJointBodyFrame *frame) {
    for (size_t index = 0; index < bodies->count; ++index) {
        const AbLevelObject *object = bodies->objects + index;
        if (object->name.size != name.size ||
            (name.size && (!object->name.data || !name.data ||
                           memcmp(object->name.data, name.data, name.size) != 0))) {
            continue;
        }
        if (!ab_world_has_body(joints->world, object->physics.body)) {
            return false;
        }
        *body_index = object->physics.body;
        const AbBodyMassState *body =
            &joints->world->storage.bodies[*body_index].motion.mass;
        *frame = (AbJointBodyFrame){body->transform, body->angle};
        return true;
    }
    return false;
}

static bool create_joint(AbClassicJoints *joints, const AbLevelBodies *bodies,
                         const AbDataTable *table, AbError *error) {
    unsigned type;
    if (!joint_type(table, &type)) {
        return ab_fail(error, "Unsupported initial classic joint type");
    }
    AbClassicJointSource source = {.table = table};
    if (type != AB_CLASSIC_JOINT_GLUE) {
        const char *const names[] = {"end1", "end2"};
        for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
            AbDataValue name = field(table, names[endpoint]);
            if (name.kind != AB_DATA_STRING ||
                !resolve_endpoint(joints, bodies, name.string,
                                  source.body_indices + endpoint,
                                  source.bodies + endpoint)) {
                return ab_fail(error, "Missing named classic joint endpoint");
            }
        }
    }
    AbClassicJoint definition;
    size_t slot;
    return ab_classic_joint_read(&source, &definition, error) &&
           ab_classic_joints_create(joints, &definition, &slot, error);
}

bool ab_level_joints_create(AbClassicJoints *joints, const AbLuaData *level,
                            const AbLevelBodies *bodies, AbStringOrderWorkspace *order,
                            AbError *error) {
    if (!joints || joints->failed || !joints->world || joints->world->failed ||
        joints->world->locked || joints->joint_count || joints->glue_count || !bodies ||
        bodies->count > bodies->capacity || (bodies->count && !bodies->objects) ||
        !order) {
        return ab_fail(error, "Invalid initial classic joint destination");
    }
    AbLevelJointRequirements requirements;
    const AbDataTable *table;
    if (!ab_level_joint_requirements(level, &requirements, error) ||
        !joint_table(level, &table)) {
        return false;
    }
    if (!table || !table->count) {
        return true;
    }
    if (requirements.immediate > joints->storage.joint_capacity ||
        requirements.deferred > joints->storage.glue_capacity ||
        !ab_data_string_order(table, requirements.hash_capacity, order, error)) {
        return ab_fail(error, "Initial classic joint storage/order is too small");
    }
    for (size_t cursor = 0; cursor < table->count; ++cursor) {
        const AbDataEntry *entry = table->entries + order->indices[cursor];
        if (!create_joint(joints, bodies, entry->value.table, error)) {
            return false;
        }
    }
    return true;
}
