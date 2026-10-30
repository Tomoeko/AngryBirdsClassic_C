#ifndef AB_UI_AMBIENT_APK_TEST_H
#define AB_UI_AMBIENT_APK_TEST_H

static bool ambient_camera(PreviewMenu *menu, size_t object, AbError *error) {
    return physics_replay_focus(menu, object, error);
}

static bool ambient_capture(PreviewMenu *menu, AbMetal *renderer, const char *variable,
                            AbError *error) {
    AbImage image = {0};
    float clear[4];
    menu_background_clear(menu, clear);
    bool okay = ab_metal_capture(renderer, menu->context.draw_list,
                                 (uint32_t)menu->screen.x, (uint32_t)menu->screen.y,
                                 clear, 16 * 1024 * 1024, &image, error);
    const char *path = getenv(variable);
    if (okay && path)
        okay = save(path, &image, error);
    ab_image_free(&image);
    return okay;
}

static bool ambient_widgets(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    if (!ability_test_episode_level(menu, 10, "BlackBird_2", error))
        return false;
    PreviewGame *game = menu->game;
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
    AbClassicBlockScriptView scripts = ab_classic_block_scripts_view(world.scripts);
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbClassicAmbientView ambient;
    size_t object = 0;
    while (object < level.bodies->count) {
        if (!ab_classic_ambient_view(scripts.ambient, object, &ambient))
            return ab_fail(error, "Actual ambient object view is unavailable");
        if (ambient.present)
            break;
        ++object;
    }
    if (object == level.bodies->count || !ambient.count)
        return ab_fail(error, "Actual BlackBird_2 has no applied ambient component");
    if (!ambient_camera(menu, object, error))
        return false;
    AbDataTable *physical = &level.properties->objects[object].physical;
    AbDataValue original = menu_route_field(physical, "sprite");
    bool changed = false, restored = false, captured = false;
    int16_t pcm[512];
    for (size_t frame = 0; frame < 720 && !restored; ++frame) {
        if (!menu_preview_draw(menu, 1.0f / 60, error) ||
            !ab_audio_mixer_mix(menu->mixer, 267, pcm, 512, error) ||
            !ab_classic_ambient_view(scripts.ambient, object, &ambient) ||
            !ambient.present)
            return false;
        AbDataValue current = menu_route_field(physical, "sprite");
        bool same = menu_route_equal(current, original);
        changed = changed || !same;
        restored = changed && same && !ambient.state->current_present;
        if (!same && !captured) {
            AbSpriteAtlasBinding binding;
            if (!ab_classic_visuals_binding(game->playfield.visuals, object, &binding))
                return ab_fail(error, "Actual ambient animation frame has no artwork");
            if (!ambient_capture(menu, renderer, "AB_AMBIENT_CAPTURE", error))
                return false;
            captured = true;
        }
    }
    if (!changed || !restored || !captured)
        return ab_fail(error, "Ambient animation did not play and restore its sprite");
    if (!game_session_escape(menu, error) || !game->paused)
        return false;
    AbAmbientAnimationState frozen = *ambient.state;
    for (size_t frame = 0; frame < 30; ++frame)
        if (!menu_preview_draw(menu, 1.0f / 60, error) ||
            !ab_classic_ambient_view(scripts.ambient, object, &ambient) ||
            memcmp(&frozen, ambient.state, sizeof(frozen)))
            return ab_fail(error, "Pause advanced the actual ambient animation");
    if (!game_session_escape(menu, error) || game->paused)
        return false;
    /* The source map reads physical.name on every visit, rather than retaining
     * the creation name or requiring that world's value to equal the key. */
    AbDataValue name = menu_route_field(physical, "name");
    if (!ab_level_property_set(level.properties, object, menu_span("name"),
                               menu_data("ab_absent_ambient_520"), error) ||
        !ab_classic_ambient_update(scripts.ambient, 0, error) ||
        !ab_classic_ambient_view(scripts.ambient, object, &ambient) ||
        ambient.present ||
        !ab_level_property_set(level.properties, object, menu_span("name"), name,
                               error) ||
        !ab_classic_ambient_update(scripts.ambient, 0, error) ||
        !ab_classic_ambient_view(scripts.ambient, object, &ambient) || ambient.present)
        return ab_fail(error, "Ambient world-name erasure/resume differs");
    for (size_t application = 1; application <= 2; ++application)
        if (!ab_block_scripts_apply(scripts.manager, scripts.objects + object,
                                    scripts.host, error) ||
            !ab_classic_ambient_view(scripts.ambient, object, &ambient) ||
            !ambient.present || ambient.count != application ||
            ambient.state->current_present)
            return ab_fail(error, "Ambient variant copies did not append/reset");
    const size_t dispose_order = 0;
    if (scripts.manager->initialized.count != 1 ||
        !ab_block_scripts_clear(scripts.manager, &dispose_order, 1, error) ||
        !ab_classic_ambient_view(scripts.ambient, object, &ambient) || ambient.present)
        return ab_fail(error, "Actual ambient disposal retained its object map");
    game_session_free(menu);
    menu->level_request_ready = false;
    puts("Actual ambient animation: source component, shared RNG, visible Metal "
         "frame, original-sprite restoration, pause gate, world-name erasure, "
         "repeat application and disposal.");
    return true;
}

static bool ambient_particle_widgets(PreviewMenu *menu, AbMetal *renderer,
                                     AbError *error) {
    if (!ability_test_episode_level(menu, 10, "BlackBird_7", error))
        return false;
    PreviewGame *game = menu->game;
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
    AbClassicBlockScriptView scripts = ab_classic_block_scripts_view(world.scripts);
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbClassicAmbientView ambient;
    size_t object = 0;
    while (object < level.bodies->count) {
        if (!ab_classic_ambient_view(scripts.ambient_particles, object, &ambient))
            return ab_fail(error, "Actual ambient particle view is unavailable");
        if (ambient.present)
            break;
        ++object;
    }
    if (object == level.bodies->count || ambient.count != 1 || !ambient.particles ||
        !ambient_camera(menu, object, error))
        return ab_fail(error, "Actual BlackBird_7 has no applied ambient particle");
    AbParticles *particles = ab_classic_particles_engine(game->particles);
    AbSpan expected = ambient.particles[0].particles;
    bool emitted = false;
    int16_t pcm[512];
    for (size_t frame = 0; frame < 720 && !emitted; ++frame) {
        if (!menu_preview_draw(menu, 1.0f / 60, error) ||
            !ab_audio_mixer_mix(menu->mixer, 267, pcm, 512, error))
            return false;
        for (size_t index = 0; index < ab_particles_count(particles, false); ++index) {
            const AbParticle *particle = ab_particles_at(particles, false, index);
            if (particle->definition.size != expected.size ||
                memcmp(particle->definition.data, expected.data, expected.size))
                continue;
            if (!particle->sprite_handle)
                return ab_fail(error, "Ambient particle omitted its actual artwork");
            emitted = true;
        }
    }
    if (!emitted ||
        !ambient_capture(menu, renderer, "AB_AMBIENT_PARTICLE_CAPTURE", error))
        return ab_fail(error, "Actual ambient particle did not emit on Metal");
    if (!game_session_escape(menu, error) || !game->paused)
        return false;
    AbAmbientParticle frozen = ambient.particles[0];
    for (size_t frame = 0; frame < 30; ++frame)
        if (!menu_preview_draw(menu, 1.0f / 60, error) ||
            !ab_classic_ambient_view(scripts.ambient_particles, object, &ambient) ||
            memcmp(&frozen, ambient.particles, sizeof(frozen)))
            return ab_fail(error, "Pause advanced an actual ambient particle timer");
    if (!game_session_escape(menu, error) || game->paused)
        return false;
    game_session_free(menu);
    menu->level_request_ready = false;
    puts("Actual ambient particles: preloaded source effect, copied timers, shared "
         "RNG, real world/body coordinates, emitted artwork on Metal and pause gate.");
    return true;
}

#endif
