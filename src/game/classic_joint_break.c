#include "ab/classic_joints.h"

#include "classic_joints_internal.h"

#include <math.h>
#include <string.h>

static bool valid_name(AbSpan name) {
    return !name.size || name.data;
}

bool ab_classic_joints_break(AbClassicJoints *joints, AbSpan object, float force,
                             AbClassicJointBreakRequest request, void *context,
                             AbError *error) {
    if (!ab_classic_joints_healthy(joints) || !valid_name(object) || !isfinite(force) ||
        !request) {
        return ab_fail(error, "Invalid classic joint breakage input/registry");
    }
    for (size_t index = 0; index < joints->joint_count; ++index) {
        const AbClassicJoint *definition = &joints->storage.joints[index].definition;
        if (!valid_name(definition->name) || !valid_name(definition->endpoints[0]) ||
            !valid_name(definition->endpoints[1])) {
            joints->failed = true;
            return ab_fail(error, "Invalid classic joint breakage record names");
        }
        if (!ab_classic_joint_name_matches(definition->endpoints[0], object) &&
            !ab_classic_joint_name_matches(definition->endpoints[1], object)) {
            continue;
        }
        const AbClassicJointRecord *metadata =
            ab_classic_joints_find_metadata(joints, definition->name);
        if (!metadata) {
            joints->failed = true;
            return ab_fail(error, "Classic joint breakage has no named metadata");
        }
        const AbClassicJoint *saved = &metadata->definition;
        if (saved->breakable.kind != AB_DATA_BOOLEAN || !saved->breakable.bits) {
            continue;
        }
        float threshold = 0;
        if (saved->break_force.kind == AB_DATA_NUMBER) {
            memcpy(&threshold, &saved->break_force.bits, sizeof(threshold));
        }
        if (!isfinite(threshold)) {
            joints->failed = true;
            return ab_fail(error, "Non-finite classic joint breakage threshold");
        }
        if (force > threshold && !request(context, definition->name, error)) {
            joints->failed = true;
            return false;
        }
    }
    return true;
}
