#include "ab/game_frame.h"

#include <math.h>

bool ab_game_frame_object_reset(const AbPhysicsWorld *world, size_t body,
                                uint32_t collider, AbGameFrameObject *output,
                                AbError *error) {
    if (!output || !ab_world_has_body(world, body)) {
        return ab_fail(error, "Missing frame object body or output");
    }
    const AbBodyMassState *mass = &world->storage.bodies[body].motion.mass;
    AbBodyPose pose = {mass->transform.position, mass->angle};
    if (!isfinite(pose.position.x) || !isfinite(pose.position.y) ||
        !isfinite(pose.angle)) {
        return ab_fail(error, "Invalid frame object reset pose");
    }
    *output = (AbGameFrameObject){body, collider, {pose, pose}, pose};
    return true;
}

static bool capture_history(AbGameFrame *frame, AbPhysicsWorld *world, AbError *error) {
    frame->history_index = 1 - frame->history_index;
    for (size_t index = 0; index < frame->count; ++index) {
        if (frame->removed_objects && frame->removed_objects[index]) {
            continue;
        }
        AbGameFrameObject *object = frame->objects + index;
        if (ab_world_has_body(world, object->body) &&
            !ab_body_pose_capture(&world->storage.bodies[object->body].motion,
                                  object->collider,
                                  object->history + frame->history_index)) {
            return ab_fail(error, "Cannot capture frame object %zu", index);
        }
    }
    return true;
}

static bool interpolate_objects(AbGameFrame *frame, const AbPhysicsWorld *world,
                                float fraction, AbError *error) {
    for (size_t index = 0; index < frame->count; ++index) {
        if (frame->removed_objects && frame->removed_objects[index]) {
            continue;
        }
        AbGameFrameObject *object = frame->objects + index;
        if (ab_world_has_body(world, object->body) &&
            world->storage.bodies[object->body].motion.awake &&
            !ab_body_pose_interpolate(object->history, frame->history_index, fraction,
                                      &object->rendered)) {
            return ab_fail(error, "Cannot interpolate frame object %zu", index);
        }
    }
    return true;
}

bool ab_game_frame_advance(AbGameFrame *frame, AbPhysicsWorld *world,
                           const AbFrameClockInput *timing,
                           const AbGameFrameControls *controls,
                           AbFrameClockPlan *output, AbError *error) {
    if (!frame || !world || world->failed || world->locked ||
        !world->broadphase.tree.nodes || !controls || !output ||
        frame->history_index > 1 || (frame->count && !frame->objects)) {
        return ab_fail(error, "Invalid game frame storage");
    }
    for (size_t index = 0; index < frame->count; ++index) {
        if ((!frame->removed_objects || !frame->removed_objects[index]) &&
            frame->objects[index].body >= world->storage.body_capacity) {
            return ab_fail(error, "Invalid frame body binding at index %zu", index);
        }
    }
    AbFrameClockPlan plan;
    if (!ab_frame_clock_advance(&frame->clock, timing, controls->maximum_steps,
                                &plan)) {
        return ab_fail(error, "Invalid game frame clock or step budget");
    }
    const float timestep = 0x1.111112p-5f;
    const float history_threshold = 0x1.111112p-4f;
    float remaining = plan.physics_seconds + frame->clock.accumulator;
    for (uint32_t step = 0; step < plan.steps; ++step) {
        if ((controls->before_step &&
             !controls->before_step(controls->context, world, timestep, error)) ||
            !ab_world_step(world, timestep, controls->velocity_iterations,
                           controls->position_iterations, error) ||
            (controls->after_world_step &&
             !controls->after_world_step(controls->context, world, error)) ||
            !ab_world_clear_forces(world, error) ||
            (controls->after_step &&
             !controls->after_step(controls->context, world, error))) {
            return false;
        }
        remaining -= timestep;
        if (remaining <= history_threshold && !capture_history(frame, world, error)) {
            return false;
        }
    }
    if (timing->physics_enabled &&
        (!ab_world_clear_forces(world, error) ||
         (controls->after_steps &&
          !controls->after_steps(controls->context, world, error)))) {
        return false;
    }
    if (plan.interpolation_present &&
        !interpolate_objects(frame, world, plan.interpolation, error)) {
        return false;
    }
    frame->clock = plan.next;
    *output = plan;
    return true;
}
