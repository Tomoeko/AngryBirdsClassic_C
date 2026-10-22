#ifndef AB_CLASSIC_FRAME_PARTICLES_H
#define AB_CLASSIC_FRAME_PARTICLES_H

#include "ab/particle_draw.h"

typedef struct {
    void *context;
    bool (*move)(void *context, float seconds, AbError *error);
} AbClassicFrameSpotlight;

typedef struct {
    AbParticles *particles;
    const bool *physics_enabled;
    const float *menu_scale;
    const AbParticleDrawStyle *style;
    AbDrawList *draws;
    /* Borrow the actual optional service pointer, reread after menu drawing. */
    AbClassicFrameSpotlight *const *spotlight;
} AbClassicFrameParticleConfig;

typedef struct {
    AbClassicFrameParticleConfig config;
    float seconds;
    bool initialized, active, calling, failed, menu_drawn;
} AbClassicFrameParticles;

/* Bind GameLua.update's particle scheduling and drawMenuParticlesInAdvance.
 * Borrow the real engine, live physics flag/menu scale, inherited draw material,
 * command list and optional Spotlight service pointer. No bootstrap defaults,
 * app delta cap, native physics, Lua root, GPU submission or audio are supplied.
 * Calls allocate nothing. Earlier updates/commands survive a late failure;
 * stop the owner after failure. Reentry rejects without disturbing the caller.
 */
bool ab_classic_frame_particles_initialize(AbClassicFrameParticleConfig config,
                                           AbClassicFrameParticles *output,
                                           AbError *error);
/* Call at ELF VA0x3240ac (rebased analysis0x3340ac), before physics/root, to clear
 * the menu-drawn flag
 * and retain this frame's original unscaled seconds. Finite used-time domain.
 * World/game/UI drawing during the root must snapshot poses into commands
 * before end updates the retained particle engine. */
bool ab_classic_frame_particles_begin(AbClassicFrameParticles *frame, float seconds,
                                      AbError *error);
/* Actual explicit menu draw marks the native flag BEFORE drawing. This may run
 * multiple times within the root. It draws the current alternate-list poses. */
bool ab_classic_frame_particles_draw_menu(AbClassicFrameParticles *frame,
                                          AbError *error);
/* Immediately after the root returns: reread physics; if enabled, update normal
 * and level particles. Always update menu particles. If not explicitly drawn,
 * append their new poses, then move an actual reached Spotlight. Automatic draw
 * leaves the flag false. Keep GPU submission after this boundary. */
bool ab_classic_frame_particles_end(AbClassicFrameParticles *frame, AbError *error);

#endif
