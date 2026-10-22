#include "ab/classic_frame.h"

#include <math.h>
#include <string.h>

bool ab_classic_frame_initialize(AbClassicFrameConfig config, AbClassicFrame *output,
                                 AbError *error) {
    if (!output || !config.level || !config.simulation || !config.root ||
        !config.particles || !config.physics_enabled || !config.physics_speed_factor ||
        !config.update_available_pending || !config.update_available_result ||
        ab_classic_simulation_view(config.simulation).level != config.level ||
        !config.particles->initialized || config.particles->failed ||
        config.particles->active || config.particles->calling ||
        config.particles->config.physics_enabled != config.physics_enabled)
        return ab_fail(error, "Invalid continuous classic frame bindings");
    *output = (AbClassicFrame){.config = config, .initialized = true};
    return true;
}

static bool deliver_update(AbClassicFrame *frame, AbError *error) {
    AbClassicFrameConfig *config = &frame->config;
    if (!*config->update_available_pending)
        return true;
    *config->update_available_pending = false;
    return config->update_available
               ? config->update_available(config->context,
                                          *config->update_available_result, error)
               : ab_fail(error, "Missing reached updateAvailable source binding");
}

bool ab_classic_frame_update(AbClassicFrame *frame, float seconds,
                             AbFrameClockPlan *output, AbError *error) {
    if (!frame || !frame->initialized || frame->failed || frame->updating || !output ||
        !isfinite(seconds))
        return ab_fail(error, "Invalid or reentrant continuous classic frame");
    AbClassicFrameConfig *config = &frame->config;
    AbFrameClockInput timing = {.seconds = seconds,
                                .physics_enabled = *config->physics_enabled};
    if (timing.physics_enabled) {
        AbDataValue speed = *config->physics_speed_factor;
        if (speed.kind == AB_DATA_STRING)
            return ab_fail(error, "Physics speed string coercion is not bound");
        timing.speed_factor_present = speed.kind == AB_DATA_NUMBER;
        if (timing.speed_factor_present)
            memcpy(&timing.speed_factor, &speed.bits, sizeof(timing.speed_factor));
    }
    frame->updating = true;
    AbFrameClockPlan plan;
    AbDataValue second = {.kind = AB_DATA_NUMBER};
    memcpy(&second.bits, &seconds, sizeof(seconds));
    bool okay =
        ab_classic_frame_particles_begin(config->particles, seconds, error) &&
        ab_classic_simulation_advance(config->simulation, &timing, &plan, error) &&
        deliver_update(frame, error) &&
        ab_classic_root_update(config->root, seconds, second, error) &&
        ab_classic_frame_particles_end(config->particles, error);
    frame->updating = false;
    frame->failed = !okay;
    if (okay)
        *output = plan;
    return okay;
}
