#include "ab/game_physics_prepare.h"

#include <math.h>

static bool ready_world(const AbPhysicsWorld *world) {
    return world && !world->failed && !world->locked && world->broadphase.tree.nodes;
}

static bool call_hook(AbGamePhysicsPrepare *prepare, AbGamePhysicsStage stage,
                      AbPhysicsWorld *world, float seconds, AbError *error) {
    return !prepare->hook ||
           prepare->hook(prepare->context, stage, world, seconds, error);
}

bool ab_game_physics_before_step(void *context, AbPhysicsWorld *world, float seconds,
                                 AbError *error) {
    AbGamePhysicsPrepare *prepare = context;
    if (!prepare || !ready_world(world) || !isfinite(seconds) || seconds <= 0 ||
        !isfinite(prepare->water_seconds)) {
        return ab_fail(error, "Invalid game physics preparation");
    }
    const float water_seconds = prepare->water_seconds;
    if (!call_hook(prepare, AB_GAME_PHYSICS_APPLY_QUEUED_FORCES, world, 0, error)) {
        return false;
    }
    if (!ready_world(world) || (prepare->count && !prepare->objects) ||
        prepare->count > UINT32_C(16777216)) {
        return ab_fail(error, "Invalid game physics object view");
    }
    for (size_t index = 0; index < prepare->count; ++index) {
        const AbGamePhysicsObject *object = prepare->objects + index;
        if (object->removed && *object->removed) {
            continue;
        }
        bool gravity_enabled = object->gravity_enabled ? *object->gravity_enabled
                                                       : object->forces.gravity_enabled;
        if (!object->prepare_forces && gravity_enabled) {
            continue;
        }
        if (!ab_world_has_body(world, object->body)) {
            return ab_fail(error, "Missing physics preparation body at %zu", index);
        }
        AbBodyMotion *body = &world->storage.bodies[object->body].motion;
        if (!ab_object_friction_apply(body, &object->friction, error)) {
            return false;
        }
        const AbObjectForceStep step = {.forces = &object->forces,
                                        .gravity = world->definition.gravity,
                                        .gravity_enabled = object->gravity_enabled,
                                        .interval = object->interval,
                                        .sound = object->sound,
                                        .random = object->random,
                                        .host = object->resource_host,
                                        .context = object->resource_context};
        if (!ab_object_force_step(body, &step, seconds, error)) {
            return false;
        }
    }
    return call_hook(prepare, AB_GAME_PHYSICS_WATER, world, water_seconds, error) &&
           call_hook(prepare, AB_GAME_PHYSICS_SCRIPT_STEP, world, seconds, error);
}

bool ab_game_physics_after_steps(void *context, AbPhysicsWorld *world, AbError *error) {
    AbGamePhysicsPrepare *prepare = context;
    if (!prepare || !ready_world(world)) {
        return ab_fail(error, "Invalid game physics cleanup");
    }
    return call_hook(prepare, AB_GAME_PHYSICS_CLEAR_FORCE_FUNCTIONS, world, 0, error);
}
