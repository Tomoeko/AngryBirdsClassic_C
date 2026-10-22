#ifndef AB_TEST_CLASSIC_FRAME_PARTICLES_H
#define AB_TEST_CLASSIC_FRAME_PARTICLES_H

#include "ab/classic_frame_particles.h"

typedef struct {
    AbClassicFrameParticles *frame;
    float normal_time, menu_time;
    size_t commands, calls;
} FrameParticleCheck;

/* A controlled observer proves the recovered phase boundary, not Spotlight
 * implementation. Selected Level1's actual native Spotlight pointer is null. */
static bool frame_particle_spotlight(void *context, float seconds, AbError *error) {
    FrameParticleCheck *check = context;
    AbClassicFrameParticles *frame = check->frame;
    const AbParticle *normal = ab_particles_at(frame->config.particles, false, 0);
    const AbParticle *menu = ab_particles_at(frame->config.particles, true, 0);
    (void)error;
    CHECK(seconds == frame->seconds && frame->calling && normal && menu &&
          normal->time == check->normal_time && menu->time == check->menu_time &&
          frame->config.draws->count == check->commands &&
          !ab_classic_frame_particles_begin(frame, 0, NULL) &&
          !ab_classic_frame_particles_end(frame, NULL) &&
          !ab_classic_frame_particles_draw_menu(frame, NULL));
    ++check->calls;
    return true;
}

static bool frame_particle_checks(AbParticles *particles) {
    CHECK(!ab_particles_count(particles, false) &&
          !ab_particles_count(particles, true) && !ab_particles_level_count(particles));
    AbParticleEmission emission = {
        .definition = span("redBuff"), .amount = 1, .position = {100, -20}};
    CHECK(ab_particles_emit(particles, &emission, NULL) &&
          ab_particles_emit_level(particles, &emission, NULL));
    emission.alternate = true;
    emission.position.y = 100;
    CHECK(ab_particles_emit(particles, &emission, NULL) &&
          ab_particles_count(particles, false) == 1 &&
          ab_particles_count(particles, true) == 1 &&
          ab_particles_level_count(particles) == 1);
    ParticleDrawSource source = {
        .region = {.width = 13, .height = 17, .pivot_x = 3, .pivot_y = -2},
        .image = {.width = 64, .height = 64}};
    AbSpriteProvider provider = {.context = &source,
                                 .sprite = draw_sprite,
                                 .composite = NULL,
                                 .texture = draw_texture};
    AbParticleDrawStyle style = {.provider = &provider,
                                 .camera = {{0, -40}, {1, 1}},
                                 .width = 480,
                                 .height = 320,
                                 .color = {1, 1, 1, 1},
                                 .opacity = 1};
    AbDrawCommand commands[8], saved[8];
    AbDrawList list = {commands, 0, 8};
    float menu_scale = 1.375f;
    bool physics = true;
    AbClassicFrameSpotlight *spotlight = NULL;
    AbClassicFrameParticles frame = {0};
    AbClassicFrameParticleConfig config = {particles, &physics, &menu_scale,
                                           &style,    &list,    &spotlight};
    CHECK(ab_classic_frame_particles_initialize(config, &frame, NULL) &&
          !ab_classic_frame_particles_end(&frame, NULL) &&
          !ab_classic_frame_particles_draw_menu(&frame, NULL) &&
          ab_classic_frame_particles_begin(&frame, 0.0625f, NULL) &&
          !ab_classic_frame_particles_begin(&frame, 0, NULL) &&
          ab_particles_draw_world(particles, style, &list, NULL) &&
          ab_particles_draw_level(particles, style, 0, &list, NULL) &&
          ab_classic_frame_particles_draw_menu(&frame, NULL) && frame.menu_drawn &&
          list.count == 3);
    memcpy(saved, commands, 3 * sizeof(*commands));
    /* Read the live flag after the root, rather than retaining its earlier true. */
    physics = false;
    CHECK(ab_classic_frame_particles_end(&frame, NULL) && !frame.active &&
          !frame.failed && frame.menu_drawn && list.count == 3 &&
          !memcmp(saved, commands, 3 * sizeof(*commands)) &&
          ab_particles_at(particles, false, 0)->time == 0 &&
          ab_particles_level_at(particles, 0)->time == 0 &&
          ab_particles_at(particles, true, 0)->time == 0.0625f);
    list.count = 0;
    FrameParticleCheck check = {&frame, 0.03125f, 0.09375f, 2, 0};
    AbClassicFrameSpotlight observed = {&check, frame_particle_spotlight};
    spotlight = &observed;
    CHECK(ab_classic_frame_particles_begin(&frame, 0.03125f, NULL) &&
          !frame.menu_drawn && ab_particles_draw_world(particles, style, &list, NULL) &&
          list.count == 1);
    memcpy(saved, commands, sizeof(*commands));
    physics = true;
    CHECK(ab_classic_frame_particles_end(&frame, NULL) && !frame.menu_drawn &&
          !frame.active && check.calls == 1 && list.count == 2 &&
          !memcmp(saved, commands, sizeof(*commands)) &&
          ab_particles_level_at(particles, 0)->time == 0.03125f);
    AbDrawCommand current[1];
    AbDrawList current_list = {current, 0, 1};
    CHECK(ab_particles_draw_menu(particles, style, menu_scale, &current_list, NULL) &&
          current_list.count == 1 && !memcmp(commands + 1, current, sizeof(*current)));
    /* Failure occurs after real updates and automatic menu draw. */
    observed.move = NULL;
    AbError late = {0};
    CHECK(ab_classic_frame_particles_begin(&frame, 0.03125f, NULL) &&
          !ab_classic_frame_particles_end(&frame, &late) &&
          strstr(late.message, "Missing reached native Spotlight") && frame.failed &&
          frame.active && !frame.calling && list.count == 3 &&
          ab_particles_at(particles, false, 0)->time == 0.0625f &&
          ab_particles_at(particles, true, 0)->time == 0.125f &&
          !ab_classic_frame_particles_begin(&frame, 0, NULL) &&
          !ab_classic_frame_particles_end(&frame, NULL));
    CHECK(ab_classic_frame_particles_initialize(config, &frame, NULL) &&
          ab_classic_frame_particles_begin(&frame, 0.03125f, NULL));
    list.capacity = list.count;
    CHECK(!ab_classic_frame_particles_draw_menu(&frame, NULL) && frame.menu_drawn &&
          frame.failed && !frame.calling && list.count == 3 &&
          ab_particles_at(particles, true, 0)->time == 0.125f);
    return true;
}

#endif
