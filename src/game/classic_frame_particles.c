#include "ab/classic_frame_particles.h"

#include <math.h>

bool ab_classic_frame_particles_initialize(AbClassicFrameParticleConfig config,
                                           AbClassicFrameParticles *output,
                                           AbError *error) {
    if (!output || !config.particles || !config.physics_enabled || !config.menu_scale ||
        !config.style || !config.draws || !config.spotlight)
        return ab_fail(error, "Invalid native frame particle bindings");
    *output = (AbClassicFrameParticles){.config = config, .initialized = true};
    return true;
}

bool ab_classic_frame_particles_begin(AbClassicFrameParticles *frame, float seconds,
                                      AbError *error) {
    if (!frame || !frame->initialized || frame->failed || frame->calling ||
        frame->active || !isfinite(seconds))
        return ab_fail(error, "Invalid or reentrant particle frame begin");
    frame->seconds = seconds;
    frame->menu_drawn = false;
    frame->active = true;
    return true;
}

bool ab_classic_frame_particles_draw_menu(AbClassicFrameParticles *frame,
                                          AbError *error) {
    if (!frame || !frame->initialized || frame->failed || frame->calling ||
        !frame->active)
        return ab_fail(error, "Invalid or reentrant explicit menu particle draw");
    frame->calling = true;
    frame->menu_drawn = true;
    AbClassicFrameParticleConfig *config = &frame->config;
    bool okay = ab_particles_draw_menu(config->particles, *config->style,
                                       *config->menu_scale, config->draws, error);
    frame->calling = false;
    frame->failed = !okay;
    return okay;
}

bool ab_classic_frame_particles_end(AbClassicFrameParticles *frame, AbError *error) {
    if (!frame || !frame->initialized || frame->failed || frame->calling ||
        !frame->active)
        return ab_fail(error, "Invalid or reentrant particle frame end");
    frame->calling = true;
    AbClassicFrameParticleConfig *config = &frame->config;
    bool okay = true;
    if (*config->physics_enabled)
        okay = ab_particles_update(config->particles, frame->seconds, false, error) &&
               ab_particles_update_level(config->particles, frame->seconds, error);
    if (okay)
        okay = ab_particles_update(config->particles, frame->seconds, true, error);
    if (okay && !frame->menu_drawn)
        okay = ab_particles_draw_menu(config->particles, *config->style,
                                      *config->menu_scale, config->draws, error);
    if (okay && *config->spotlight) {
        AbClassicFrameSpotlight *spotlight = *config->spotlight;
        okay = spotlight->move
                   ? spotlight->move(spotlight->context, frame->seconds, error)
                   : ab_fail(error, "Missing reached native Spotlight move service");
    }
    frame->calling = false;
    frame->active = !okay;
    frame->failed = !okay;
    return okay;
}
