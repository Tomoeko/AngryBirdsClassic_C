#include "ab/slingshot_body.h"

#include <math.h>

static bool valid_sling_body(const AbPhysicsWorld *world, const AbObjectBody *object) {
    return world && !world->failed && !world->locked && object &&
           ab_world_has_body(world, object->body) &&
           ab_world_has_fixture(world, object->fixture) &&
           world->storage.fixtures[object->fixture].body == object->body;
}

bool ab_slingshot_transfer_body(AbPhysicsWorld *world, const AbObjectBody *object,
                                const AbSlingTransferInput *input,
                                AbSlingTransferFrame *frame, AbError *error) {
    if (!valid_sling_body(world, object) || !frame) {
        return ab_fail(error, "Invalid sling-transfer body");
    }
    AbSlingTransferFrame result;
    float rotation;
    if (!ab_slingshot_transfer_frame(input, &result) ||
        !ab_game_angle_wrap(result.rotation, &rotation)) {
        return ab_fail(error, "Unsupported sling-transfer arithmetic");
    }
    float current_angle = world->storage.bodies[object->body].motion.mass.angle;
    if (!ab_world_set_transform(world, object->body, result.position, current_angle,
                                error) ||
        !ab_world_set_transform(world, object->body, result.position, rotation,
                                error)) {
        return false;
    }
    if (result.complete) {
        AbVec2 final = {input->target.x - 0.1f, input->target.y - 0.1f};
        if (!ab_world_set_transform(world, object->body, final, rotation, error) ||
            !ab_world_set_body_type(world, object->body, AB_BODY_DYNAMIC, error) ||
            !ab_world_set_transform(world, object->body, final, 0, error)) {
            return false;
        }
    }
    *frame = result;
    return true;
}

bool ab_slingshot_hold_body(AbPhysicsWorld *world, const AbObjectBody *object,
                            AbVec2 rubber_band, AbError *error) {
    if (!valid_sling_body(world, object) || !isfinite(rubber_band.x) ||
        !isfinite(rubber_band.y)) {
        return ab_fail(error, "Invalid held-bird body or position");
    }
    AbBodyMotion *body = &world->storage.bodies[object->body].motion;
    if (!ab_world_set_transform(world, object->body, rubber_band, body->mass.angle,
                                error) ||
        !ab_body_velocity_set(body, (AbVec2){0}) ||
        !ab_world_set_transform(world, object->body, rubber_band, 0, error) ||
        !ab_body_angular_velocity_set(body, 0)) {
        return ab_fail(error, "Could not hold bird in the slingshot");
    }
    return true;
}
