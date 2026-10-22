#include "ab/classic_joints.h"

#include "classic_joints_internal.h"

#include <string.h>

static bool valid_storage(AbClassicJointStorage storage) {
    return storage.joint_capacity <= SIZE_MAX / sizeof(*storage.joints) &&
           storage.glue_capacity <= SIZE_MAX / sizeof(*storage.glue) &&
           (!storage.joint_capacity || storage.joints) &&
           (!storage.glue_capacity || storage.glue);
}

bool ab_classic_joints_healthy(const AbClassicJoints *joints) {
    return joints && !joints->failed && joints->world && !joints->world->failed &&
           joints->world->broadphase.tree.nodes && valid_storage(joints->storage) &&
           joints->joint_count <= joints->storage.joint_capacity &&
           joints->glue_count <= joints->storage.glue_capacity;
}

bool ab_classic_joints_open(AbClassicJoints *joints, AbPhysicsWorld *world,
                            AbClassicJointStorage storage, AbError *error) {
    if (!joints) {
        return ab_fail(error, "Missing classic joint registry output");
    }
    *joints = (AbClassicJoints){.world = world, .storage = storage};
    if (!ab_classic_joints_healthy(joints)) {
        joints->failed = true;
        return ab_fail(error, "Invalid classic joint registry storage/world");
    }
    return true;
}

void ab_classic_joints_close(AbClassicJoints *joints) {
    if (joints) {
        *joints = (AbClassicJoints){0};
    }
}

bool ab_classic_joint_name_matches(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || (first.data && second.data &&
                            memcmp(first.data, second.data, first.size) == 0));
}

static void clear_metadata(AbClassicJoints *joints, AbSpan name) {
    for (size_t index = 0; index < joints->joint_count; ++index) {
        AbClassicJointRecord *record = joints->storage.joints + index;
        if (ab_classic_joint_name_matches(record->definition.name, name)) {
            record->metadata_visible = false;
        }
    }
    for (size_t index = 0; index < joints->glue_count; ++index) {
        AbClassicJointRecord *record = joints->storage.glue + index;
        if (ab_classic_joint_name_matches(record->definition.name, name)) {
            record->metadata_visible = false;
        }
    }
}

AbClassicJointRecord *ab_classic_joints_find_metadata(const AbClassicJoints *joints,
                                                      AbSpan name) {
    if (!joints || !valid_storage(joints->storage) ||
        joints->joint_count > joints->storage.joint_capacity ||
        joints->glue_count > joints->storage.glue_capacity) {
        return NULL;
    }
    for (size_t index = 0; index < joints->joint_count; ++index) {
        AbClassicJointRecord *record = joints->storage.joints + index;
        if (record->metadata_visible &&
            ab_classic_joint_name_matches(record->definition.name, name)) {
            return record;
        }
    }
    for (size_t index = 0; index < joints->glue_count; ++index) {
        AbClassicJointRecord *record = joints->storage.glue + index;
        if (record->metadata_visible &&
            ab_classic_joint_name_matches(record->definition.name, name)) {
            return record;
        }
    }
    return NULL;
}

const AbClassicJointRecord *ab_classic_joints_metadata(const AbClassicJoints *joints,
                                                       AbSpan name) {
    return ab_classic_joints_find_metadata(joints, name);
}

bool ab_classic_joints_create(AbClassicJoints *joints, const AbClassicJoint *definition,
                              size_t *world_joint, AbError *error) {
    if (!world_joint || !ab_classic_joints_healthy(joints) || !definition ||
        definition->type < AB_CLASSIC_JOINT_DISTANCE ||
        definition->type > AB_CLASSIC_JOINT_GLUE ||
        (definition->name.size && !definition->name.data)) {
        return ab_fail(error, "Invalid classic joint registration");
    }
    bool glue = definition->type == AB_CLASSIC_JOINT_GLUE;
    size_t count = glue ? joints->glue_count : joints->joint_count;
    size_t capacity =
        glue ? joints->storage.glue_capacity : joints->storage.joint_capacity;
    if (count >= capacity || (!glue && definition->joint.constraint.kind !=
                                           (AbJointKind)(definition->type - 1))) {
        return ab_fail(error, "Classic joint capacity or kind mismatch");
    }
    AbClassicJointRecord record = {
        .definition = *definition, .world_joint = SIZE_MAX, .metadata_visible = true};
    if (!glue && !joints->world->locked &&
        !ab_world_create_joint(joints->world, &definition->joint, &record.world_joint,
                               error)) {
        return false;
    }
    clear_metadata(joints, record.definition.name);
    if (glue) {
        joints->storage.glue[joints->glue_count++] = record;
    } else {
        joints->storage.joints[joints->joint_count++] = record;
    }
    *world_joint = record.world_joint;
    return true;
}

static bool remove_immediate(AbClassicJoints *joints, size_t index, AbError *error) {
    size_t world_joint = joints->storage.joints[index].world_joint;
    if (!joints->world->locked && !ab_world_has_joint(joints->world, world_joint)) {
        return ab_fail(error, "Classic joint record has no live engine joint");
    }
    clear_metadata(joints, joints->storage.joints[index].definition.name);
    if (!joints->world->locked &&
        !ab_world_destroy_joint(joints->world, world_joint, error)) {
        joints->failed = true;
        return false;
    }
    --joints->joint_count;
    memmove(joints->storage.joints + index, joints->storage.joints + index + 1,
            (joints->joint_count - index) * sizeof(*joints->storage.joints));
    return true;
}

bool ab_classic_joints_remove(AbClassicJoints *joints, size_t world_joint,
                              AbError *error) {
    if (!ab_classic_joints_healthy(joints)) {
        return ab_fail(error, "Invalid classic joint removal world/registry");
    }
    for (size_t index = 0; index < joints->joint_count; ++index) {
        if (joints->storage.joints[index].world_joint == world_joint) {
            return remove_immediate(joints, index, error);
        }
    }
    return true;
}

bool ab_classic_joints_remove_named(AbClassicJoints *joints, AbSpan name,
                                    AbError *error) {
    if (!ab_classic_joints_healthy(joints) || (name.size && !name.data)) {
        return ab_fail(error, "Invalid classic named joint removal world/registry");
    }
    for (size_t index = 0; index < joints->joint_count; ++index) {
        if (ab_classic_joint_name_matches(joints->storage.joints[index].definition.name,
                                          name)) {
            return remove_immediate(joints, index, error);
        }
    }
    for (size_t index = 0; index < joints->glue_count; ++index) {
        if (ab_classic_joint_name_matches(joints->storage.glue[index].definition.name,
                                          name)) {
            clear_metadata(joints, name);
            --joints->glue_count;
            memmove(joints->storage.glue + index, joints->storage.glue + index + 1,
                    (joints->glue_count - index) * sizeof(*joints->storage.glue));
            break;
        }
    }
    return true;
}

static bool record_names_valid(const AbClassicJoint *definition) {
    return (!definition->name.size || definition->name.data) &&
           (!definition->endpoints[0].size || definition->endpoints[0].data) &&
           (!definition->endpoints[1].size || definition->endpoints[1].data);
}

static bool mark_second_endpoint(AbClassicJoints *joints,
                                 AbGameObjectCoreRegistry *objects,
                                 const AbClassicJoint *definition, AbError *error) {
    uintptr_t handle = 0;
    bool present = false;
    if (!ab_string_map_get(objects->objects, definition->endpoints[1], &handle,
                           &present, error)) {
        return false;
    }
    if (!present) {
        return true;
    }
    AbGameObjectCore *second = (AbGameObjectCore *)handle;
    if (!second) {
        return ab_fail(error, "Missing physical glue endpoint metadata");
    }
    second->delayed_destroy = true;
    const AbClassicJointRecord *metadata =
        ab_classic_joints_metadata(joints, definition->name);
    if (!metadata) {
        return ab_fail(error, "Removed glue joint has no named metadata");
    }
    second->destroy_timer = metadata->definition.destroy_timer;
    return true;
}

static bool forget_records(AbClassicJoints *joints, AbGameObjectCoreRegistry *objects,
                           AbClassicJointRecord *records, size_t *count, AbSpan object,
                           bool deferred, AbError *error) {
    size_t index = 0;
    while (index < *count) {
        const AbClassicJoint *definition = &records[index].definition;
        if (!record_names_valid(definition)) {
            return ab_fail(error, "Invalid removed-object joint record names");
        }
        bool first = ab_classic_joint_name_matches(definition->endpoints[0], object);
        if (!first &&
            !ab_classic_joint_name_matches(definition->endpoints[1], object)) {
            ++index;
            continue;
        }
        if (deferred && first &&
            !mark_second_endpoint(joints, objects, definition, error)) {
            return false;
        }
        clear_metadata(joints, definition->name);
        --*count;
        memmove(records + index, records + index + 1,
                (*count - index) * sizeof(*records));
    }
    return true;
}

bool ab_classic_joints_forget_object(AbClassicJoints *joints,
                                     AbGameObjectCoreRegistry *objects, AbSpan object,
                                     AbError *error) {
    if (!ab_classic_joints_healthy(joints) || !objects || !objects->objects ||
        objects->world != joints->world || (object.size && !object.data)) {
        return ab_fail(error, "Invalid removed-object joint registry/world");
    }
    return forget_records(joints, objects, joints->storage.glue, &joints->glue_count,
                          object, true, error) &&
           forget_records(joints, objects, joints->storage.joints, &joints->joint_count,
                          object, false, error);
}
