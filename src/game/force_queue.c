#include "ab/force_queue.h"

#include <math.h>

static bool valid_queue(const AbForceQueue *queue) {
    return queue && queue->count <= queue->capacity &&
           (!queue->capacity || queue->entries);
}

static bool valid_force(AbQueuedForce force) {
    return (!force.object.size || force.object.data) && isfinite(force.force.x) &&
           isfinite(force.force.y) && isfinite(force.point.x) &&
           isfinite(force.point.y);
}

bool ab_force_queue_append(AbForceQueue *queue, AbQueuedForce force, AbError *error) {
    if (!valid_queue(queue) || queue->count == queue->capacity || !valid_force(force)) {
        return ab_fail(error, "Invalid or full queued force storage");
    }
    queue->entries[queue->count++] = force;
    return true;
}

bool ab_force_queue_apply(const AbForceQueue *queue, AbForceQueueHost host,
                          AbError *error) {
    if (!valid_queue(queue) || (queue->count && (!host.exists || !host.apply))) {
        return ab_fail(error, "Invalid queued force pass");
    }
    for (size_t index = 0; index < queue->count; ++index) {
        const AbQueuedForce *force = queue->entries + index;
        bool present = false;
        if (!valid_force(*force)) {
            return ab_fail(error, "Invalid captured force at %zu", index);
        }
        if (!host.exists(host.context, force->object, &present, error) ||
            (present && !host.apply(host.context, force, error))) {
            return false;
        }
    }
    return true;
}

bool ab_force_queue_clear(AbForceQueue *queue, AbError *error) {
    if (!valid_queue(queue)) {
        return ab_fail(error, "Invalid queued force cleanup");
    }
    queue->count = 0;
    return true;
}
