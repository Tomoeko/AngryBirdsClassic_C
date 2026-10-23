#include "ab/classic_simulation.h"

#include <stdlib.h>

struct AbClassicSimulation {
    AbClassicLevel *level;
    AbClassicForces *forces;
    AbClassicPostPhysics *post;
    AbClassicSimulationConfig config;
    AbForceQueue queue;
    AbClassicGlueQueue glue;
    AbGamePhysicsPrepare prepare;
    size_t bytes;
    float planned_time;
    bool advancing;
};

static bool call_phase(AbClassicSimulation *simulation, AbClassicSimulationStage stage,
                       float seconds, AbError *error) {
    if (!simulation->config.phase) {
        return ab_fail(error, "Missing classic simulation phase host");
    }
    const AbClassicSimulationCall call = {stage, seconds, simulation->planned_time};
    return simulation->config.phase(simulation->config.context, &call, error);
}

static bool physics_hook(void *context, AbGamePhysicsStage stage, AbPhysicsWorld *world,
                         float seconds, AbError *error) {
    (void)world;
    AbClassicSimulation *simulation = context;
    switch (stage) {
        case AB_GAME_PHYSICS_APPLY_QUEUED_FORCES:
            return ab_classic_forces_apply_queue(simulation->forces, &simulation->queue,
                                                 error);
        case AB_GAME_PHYSICS_WATER:
            return call_phase(simulation, AB_CLASSIC_SIMULATION_WATER, seconds, error);
        case AB_GAME_PHYSICS_SCRIPT_STEP:
            return call_phase(simulation, AB_CLASSIC_SIMULATION_SCRIPT_STEP, seconds,
                              error);
        case AB_GAME_PHYSICS_CLEAR_FORCE_FUNCTIONS:
            return ab_force_queue_clear(&simulation->queue, error);
    }
    return ab_fail(error, "Invalid classic simulation physics phase");
}

static bool before_step(void *context, AbPhysicsWorld *world, float seconds,
                        AbError *error) {
    AbClassicSimulation *simulation = context;
    return ab_game_physics_before_step(&simulation->prepare, world, seconds, error);
}

static bool after_step(void *context, AbPhysicsWorld *world, AbError *error) {
    (void)world;
    return call_phase(context, AB_CLASSIC_SIMULATION_AFTER_STEP, 0x1.111112p-5f, error);
}

static bool after_world_step(void *context, AbPhysicsWorld *world, AbError *error) {
    AbClassicSimulation *simulation = context;
    AbClassicLevelView level = ab_classic_level_view(simulation->level);
    if (level.world != world) {
        return ab_fail(error, "Classic glue queue world/level mismatch");
    }
    return ab_classic_joints_glue(level.joints, simulation->glue.pairs,
                                  &simulation->glue.count, error);
}

static bool after_steps(void *context, AbPhysicsWorld *world, AbError *error) {
    AbClassicSimulation *simulation = context;
    return ab_game_physics_after_steps(&simulation->prepare, world, error);
}

bool ab_classic_simulation_open(AbClassicLevel *level, AbClassicSimulationConfig config,
                                AbClassicSimulation **output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing classic simulation output");
    }
    *output = NULL;
    size_t remaining = config.limits.storage_bytes;
    if (!ab_classic_level_view(level).world || config.maximum_steps > 4096 ||
        remaining < sizeof(AbClassicSimulation)) {
        return ab_fail(error, "Invalid classic simulation source or limits");
    }
    remaining -= sizeof(AbClassicSimulation);
    if (config.limits.queued_forces > remaining / sizeof(AbQueuedForce)) {
        return ab_fail(error, "Classic simulation force queue exceeds byte limit");
    }
    remaining -= config.limits.queued_forces * sizeof(AbQueuedForce);
    if (config.limits.glue_pairs > remaining / sizeof(AbClassicGluePair)) {
        return ab_fail(error, "Classic simulation glue queue exceeds byte limit");
    }
    remaining -= config.limits.glue_pairs * sizeof(AbClassicGluePair);
    AbClassicSimulation *simulation = calloc(1, sizeof(*simulation));
    if (!simulation) {
        return ab_fail(error, "Cannot allocate classic simulation owner");
    }
    simulation->level = level;
    simulation->config = config;
    simulation->queue.capacity = config.limits.queued_forces;
    if (simulation->queue.capacity) {
        simulation->queue.entries =
            calloc(simulation->queue.capacity, sizeof(*simulation->queue.entries));
        if (!simulation->queue.entries) {
            ab_fail(error, "Cannot allocate classic simulation force queue");
            goto release_simulation;
        }
    }
    simulation->glue.capacity = config.limits.glue_pairs;
    if (simulation->glue.capacity) {
        simulation->glue.pairs =
            calloc(simulation->glue.capacity, sizeof(*simulation->glue.pairs));
        if (!simulation->glue.pairs) {
            ab_fail(error, "Cannot allocate classic simulation glue queue");
            goto release_simulation;
        }
    }
    AbClassicForceLimits force_limits = {
        .storage_bytes = remaining,
        .sound_choices = config.limits.force_sound_choices,
        .object_capacity = config.limits.object_capacity};
    if (!ab_classic_forces_open(level, force_limits, config.random, config.force,
                                config.context, &simulation->forces, error)) {
        goto release_simulation;
    }
    remaining -= ab_classic_forces_bytes(simulation->forces);
    AbClassicPostConfig post_config = {.minimum_x = config.minimum_x,
                                       .maximum_x = config.maximum_x,
                                       .audio = config.rolling,
                                       .context = config.context,
                                       .storage_bytes = remaining,
                                       .object_capacity = config.limits.object_capacity,
                                       .activity = config.activity};
    if (!ab_classic_post_physics_open(level, post_config, &simulation->post, error)) {
        goto release_simulation;
    }
    remaining -= ab_classic_post_physics_bytes(simulation->post);
    simulation->bytes = config.limits.storage_bytes - remaining;
    AbClassicForceView forces = ab_classic_forces_view(simulation->forces);
    simulation->prepare = (AbGamePhysicsPrepare){.objects = forces.objects,
                                                 .count = forces.count,
                                                 .hook = physics_hook,
                                                 .context = simulation};
    *output = simulation;
    return true;

release_simulation:
    ab_classic_simulation_close(simulation);
    return false;
}

void ab_classic_simulation_close(AbClassicSimulation *simulation) {
    if (simulation) {
        ab_classic_post_physics_close(simulation->post);
        ab_classic_forces_close(simulation->forces);
        free(simulation->queue.entries);
        free(simulation->glue.pairs);
        free(simulation);
    }
}

size_t ab_classic_simulation_bytes(const AbClassicSimulation *simulation) {
    return simulation ? simulation->bytes : 0;
}

AbClassicSimulationView ab_classic_simulation_view(AbClassicSimulation *simulation) {
    return simulation ? (AbClassicSimulationView){.level = simulation->level,
                                                  .forces = ab_classic_forces_view(
                                                      simulation->forces),
                                                  .post = ab_classic_post_physics_view(
                                                      simulation->post),
                                                  .queue = &simulation->queue,
                                                  .glue = &simulation->glue}
                      : (AbClassicSimulationView){0};
}

bool ab_classic_simulation_recreate_circle(AbClassicSimulation *simulation,
                                           size_t object, AbError *error) {
    if (!simulation || simulation->advancing)
        return ab_fail(error, "Invalid replacement simulation circle binding");
    return ab_classic_forces_recreate_circle(simulation->forces, object, error) &&
           ab_classic_post_physics_recreate_circle(simulation->post, object, error);
}

bool ab_classic_simulation_register_circle(AbClassicSimulation *simulation,
                                           size_t object, AbError *error) {
    if (!simulation || simulation->advancing ||
        ab_classic_forces_view(simulation->forces).count != object ||
        ab_classic_post_physics_view(simulation->post).count != object)
        return ab_fail(error, "Invalid new simulation circle binding");
    return ab_classic_forces_register_circle(simulation->forces, object, error) &&
           ab_classic_post_physics_register_circle(simulation->post, object, error);
}

bool ab_classic_simulation_advance(AbClassicSimulation *simulation,
                                   const AbFrameClockInput *timing,
                                   AbFrameClockPlan *output, AbError *error) {
    if (!simulation || simulation->advancing || !timing || !output) {
        return ab_fail(error, "Invalid classic simulation advance");
    }
    const AbFrameClockInput elapsed = *timing;
    AbClassicLevelView level = ab_classic_level_view(simulation->level);
    AbClassicForceView forces = ab_classic_forces_view(simulation->forces);
    if (forces.count != level.bodies->count ||
        ab_classic_post_physics_view(simulation->post).count != level.bodies->count)
        return ab_fail(error, "Unregistered classic simulation object");
    AbFrameClockPlan plan;
    if (!ab_frame_clock_advance(&level.frame->clock, &elapsed,
                                simulation->config.maximum_steps, &plan)) {
        return ab_fail(error, "Invalid classic simulation clock or step budget");
    }
    simulation->advancing = true;
    simulation->prepare.objects = forces.objects;
    simulation->prepare.count = forces.count;
    simulation->planned_time = plan.next.time;
    simulation->prepare.water_seconds = plan.physics_seconds;
    AbGameFrameControls controls = {
        .velocity_iterations = simulation->config.velocity_iterations,
        .position_iterations = simulation->config.position_iterations,
        .maximum_steps = simulation->config.maximum_steps,
        .context = simulation,
        .after_step = after_step,
        .before_step = before_step,
        .after_steps = after_steps,
        .after_world_step = after_world_step};
    bool okay = ab_game_frame_advance(level.frame, level.world, &elapsed, &controls,
                                      &plan, error) &&
                ab_classic_post_physics_update(simulation->post,
                                               elapsed.physics_enabled, error);
    simulation->advancing = false;
    if (okay) {
        *output = plan;
    }
    return okay;
}
