#include "ab/level_raycast.h"

static bool fixture_name(const AbLevelBodies *bodies, const bool *removed,
                         const AbWorldFixture *fixture, AbDataValue *output,
                         AbError *error) {
    if (!fixture->user_data) {
        *output = (AbDataValue){0};
        return true;
    }
    for (size_t index = 0; index < bodies->count; ++index) {
        const AbLevelObject *object = bodies->objects + index;
        if (fixture->user_data != (uintptr_t)object) {
            continue;
        }
        if ((removed && removed[index]) || object->physics.body != fixture->body ||
            (object->name.size && !object->name.data)) {
            return ab_fail(error, "Stale level raycast object binding");
        }
        *output = (AbDataValue){.kind = AB_DATA_STRING, .string = object->name};
        return true;
    }
    return ab_fail(error, "Unknown level raycast fixture user data");
}

bool ab_level_bodies_raycast(const AbPhysicsWorld *world, const AbLevelBodies *bodies,
                             const bool *removed, AbVec2 first, AbVec2 second,
                             AbLevelRaycast *output, AbError *error) {
    if (!bodies || bodies->count > bodies->capacity ||
        (bodies->count && !bodies->objects) || !output) {
        return ab_fail(error, "Invalid level raycast request");
    }
    AbWorldRaycastHit hit = {0};
    bool found = false;
    if (!ab_world_raycast_nearest(world, (AbRay){first, second, 1}, &hit, &found,
                                  error)) {
        return false;
    }
    if (!found) {
        output->hit = false;
        return true;
    }
    AbLevelRaycast result = {.hit = true, .point = hit.point, .normal = hit.normal};
    if (!fixture_name(bodies, removed, world->storage.fixtures + hit.fixture,
                      &result.name, error)) {
        return false;
    }
    *output = result;
    return true;
}
