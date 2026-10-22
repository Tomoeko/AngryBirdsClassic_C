#include "ab/object_transform.h"

#include "ab/object_body.h"

static bool valid_world(const AbPhysicsWorld *world) {
    return world && !world->failed && !world->locked && world->broadphase.tree.nodes;
}

bool ab_object_position_set(AbPhysicsWorld *world, AbGameFrameObject *frame,
                            AbVec2 *script_position, AbVec2 position, AbError *error) {
    if (!valid_world(world) || !frame || !script_position) {
        return ab_fail(error, "Invalid object position binding");
    }
    if (!ab_world_has_body(world, frame->body)) {
        return true;
    }
    float angle = world->storage.bodies[frame->body].motion.mass.angle;
    if (!ab_world_set_transform(world, frame->body, position, angle, error)) {
        return false;
    }
    *script_position = position;
    frame->rendered.position = position;
    frame->history[0].position = position;
    frame->history[1].position = position;
    return true;
}

bool ab_object_rotation_set(AbPhysicsWorld *world, AbGameFrameObject *frame,
                            float *script_angle, float angle, AbError *error) {
    if (!valid_world(world) || !frame || !script_angle) {
        return ab_fail(error, "Invalid object rotation binding");
    }
    if (!ab_world_has_body(world, frame->body)) {
        return true;
    }
    float normalized;
    AbVec2 position = world->storage.bodies[frame->body].motion.mass.transform.position;
    if (!ab_game_angle_wrap(angle, &normalized) ||
        !ab_world_set_transform(world, frame->body, position, normalized, error)) {
        return ab_fail(error, "Cannot apply object rotation");
    }
    *script_angle = normalized;
    frame->rendered.angle = normalized;
    frame->history[0].angle = normalized;
    frame->history[1].angle = normalized;
    return true;
}
