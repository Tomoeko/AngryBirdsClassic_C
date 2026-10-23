#ifndef AB_UI_POWERUPS_APK_TEST_H
#define AB_UI_POWERUPS_APK_TEST_H

static bool powerup_test_tick(PreviewMenu *menu, size_t frames, AbError *error) {
    int16_t pcm[512];
    for (size_t frame = 0; frame < frames; ++frame)
        if (!menu_preview_draw(menu, 1.0f / 60, error) ||
            !ab_audio_mixer_mix(menu->mixer, 267, pcm, 512, error))
            return false;
    return true;
}

static bool powerup_king_widgets(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    for (unsigned infinite = 0; infinite < 2; ++infinite) {
        menu->infinite_powerups = infinite != 0;
        if (!ability_test_level(menu, "Level1", error))
            return false;
        PreviewGame *game = menu->game;
        PreviewPowerups *powers = game->powerups;
        const AbPowerupMetadata *row = ab_powerup_catalog_at(menu->powerup_catalog, 1);
        if (!powers || !row || powers->bar.slider.count != 4)
            return ab_fail(error, "Live powerup bar has %zu source controls",
                           powers ? powers->bar.slider.count : 0);
        if (!ab_settings_inventory_set(menu->settings, row->name,
                                       menu_route_number(infinite ? 0 : 3),
                                       menu_route_number(0), error) ||
            !powerup_test_tick(menu, 245, error))
            return false;
        if (!ab_ui_slider_is_open(&powers->bar.slider) &&
            !game_test_click_widget(menu, &powers->bar.slider.toggle, error))
            return false;
        if (!powerup_test_tick(menu, 45, error))
            return false;
        if (!ab_ui_slider_is_open(&powers->bar.slider)) {
            AbVec2 toggle, pause;
            if (!game_test_center(&powers->bar.slider.toggle, &toggle, error) ||
                !game_test_center(&game->pause->button.widget, &pause, error))
                return false;
            return ab_fail(error,
                           "Powerup toggle stayed closed: %.1f,%.1f pause %.1f,%.1f "
                           "visible=%d notifications=%zu paused=%d keep=%d",
                           toggle.x, toggle.y, pause.x, pause.y,
                           ab_ui_pointer_value_truthy(
                               &powers->bar.slider.box.widget.frame->fields.visible),
                           game->notifications.frame->child_count, game->paused,
                           menu->keep_powerups_open);
        }
        if (!game_test_click_widget(menu, &powers->bar.powerups[1].button, error)) {
            if (!error->message[0])
                ab_fail(error, "King Sling mouse dispatch failed");
            return false;
        }
        float balance;
        if (!powers->effect_count || powers->king_timer <= 0 ||
            ab_powerup_uses_count(game->uses) != 1 || game->shot.birds_shot != 0 ||
            !ab_settings_inventory_amount(menu->settings, row->name, &balance, error) ||
            balance != (infinite ? 0 : 2))
            return ab_fail(error, "Powerup HUD lost its effect or inventory policy");
        if (infinite && !menu_equal(powers->bar.powerups[1].counter.state.fields.text,
                                    "\xe2\x88\x9e"))
            return ab_fail(error, "Infinite HUD counter did not use its source text");
        size_t textures = ab_asset_textures_count(menu->textures);
        if (!menu_preview_resize(menu, (AbVec2){1280, 720}, error) ||
            !powerup_test_tick(menu, 8, error) ||
            !menu_preview_resize(menu, (AbVec2){1024, 768}, error) ||
            ab_asset_textures_count(menu->textures) != textures)
            return ab_fail(error, "Powerup resize changed its loaded resources");
        if (!powerup_test_tick(menu, 105, error))
            return false;
        float power;
        bool available;
        if (!ab_classic_sling_view(game->playfield.sling).king ||
            powers->effect_count || game->active.items[0] ||
            !ab_classic_sling_power(game->playfield.sling, &power, error) ||
            power != 1.0f + 0.3f ||
            !ab_classic_powerup_manager_can_activate(&powers->manager, 1, &available,
                                                     error) ||
            available)
            return ab_fail(error, "King Sling did not finish its actual activation");
        for (size_t phase = 0; phase < 3; ++phase) {
            AbVec2 target = ab_classic_sling_view(game->playfield.sling).band.rest;
            if (phase) {
                target.x -= 4;
                target.y += 1.5f;
            }
            AbCameraViewport viewport;
            AbVec2 point;
            AbUiPointerReply reply;
            if (!ab_camera_system_query_viewport(
                    ab_classic_camera_system(game->playfield.camera), &viewport) ||
                !ab_coordinate_point(&viewport.rendering, AB_SPACE_PHYSICS,
                                     AB_SPACE_SCREEN, target, &point) ||
                !menu_preview_pointer(menu,
                                      menu_span(phase == 0   ? "LPRESS"
                                                : phase == 1 ? "LHOLD"
                                                             : "LRELEASE"),
                                      point, &reply, error) ||
                !powerup_test_tick(menu, 1, error))
                return false;
        }
        if (!powerup_test_tick(menu, 12, error) || game->shot.birds_shot != 1)
            return ab_fail(error, "Powered mouse shot did not launch");
        AbParticles *particles = ab_classic_particles_engine(game->particles);
        bool trail = false;
        for (size_t index = 0; index < ab_particles_count(particles, false); ++index) {
            const AbParticle *particle = ab_particles_at(particles, false, index);
            trail |= menu_route_equal(
                (AbDataValue){.kind = AB_DATA_STRING, .string = particle->definition},
                menu_data("superSlingTrail"));
        }
        if (!trail)
            return ab_fail(error, "King Sling shot did not emit its source trail");
        AbImage image = {0};
        float clear[4];
        menu_background_clear(menu, clear);
        bool okay = ab_metal_capture(renderer, menu->context.draw_list,
                                     (uint32_t)game->screen.x, (uint32_t)game->screen.y,
                                     clear, 16 * 1024 * 1024, &image, error);
        const char *capture = infinite ? getenv("AB_POWERUP_CAPTURE") : NULL;
        if (okay && capture)
            okay = save(capture, &image, error);
        ab_image_free(&image);
        game_session_free(menu);
        menu->level_request_ready = false;
        if (!okay)
            return false;
    }
    menu->infinite_powerups = false;
    menu->keep_powerups_open = false;
    puts("Actual powerup HUD: mouse slider and King Sling, activation camera, "
         "notification/audio/particles, stronger mouse launch and trail on Metal; "
         "finite inventory consumes once and infinite inventory remains unchanged.");
    return true;
}

static bool powerup_seed_widgets(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    static const struct {
        size_t episode;
        const char *level, *bird;
        float density;
    } scenes[] = {
        {1, "Level1", "RedBird", 2},       {1, "Level1", "RedBird", 2},
        {1, "Level8", "SmallBlueBird", 3}, {1, "Level15", "YellowBird", 1.5f},
        {1, "Level88", "BlackBird", 2},    {3, "LevelP3_224", "BoomerangBird", 1.5f}};
    for (size_t scene = 0; scene < sizeof(scenes) / sizeof(*scenes); ++scene) {
        bool infinite = scene != 0;
        menu->infinite_powerups = infinite;
        if (!ability_test_episode_level(menu, scenes[scene].episode,
                                        scenes[scene].level, error))
            return false;
        PreviewGame *game = menu->game;
        PreviewPowerups *powers = game->powerups;
        const AbPowerupMetadata *row = ab_powerup_catalog_at(menu->powerup_catalog, 0);
        AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
        size_t count = level.bodies->count;
        if (!ab_settings_inventory_set(menu->settings, row->name,
                                       menu_route_number(infinite ? 0 : 3),
                                       menu_route_number(0), error) ||
            !powerup_test_tick(menu, 245, error))
            return false;
        size_t object;
        if (!ab_classic_sling_next_bird(
                game->playfield.sling,
                ab_classic_sling_view(game->playfield.sling).current_bird_index,
                &object, error) ||
            object == SIZE_MAX)
            return ab_fail(error, "Super Seeds test has no queued bird");
        if (!menu_route_equal(
                menu_route_field(&level.properties->objects[object].physical,
                                 "definition"),
                menu_data(scenes[scene].bird)))
            return ab_fail(error,
                           "Super Seeds fixture selected another queued bird in %s",
                           scenes[scene].level);
        float radius = level.bodies->objects[object].physics.dimensions.radius;
        float mass = level.bodies->objects[object].initial_mass;
        AbDataTable *bird = &level.properties->objects[object].physical;
        AbDataValue original_particles;
        if (!ab_level_property_get(level.properties, object, menu_span("particles"),
                                   &original_particles, error))
            return false;
        if (!ab_ui_slider_is_open(&powers->bar.slider) &&
            !game_test_click_widget(menu, &powers->bar.slider.toggle, error))
            return false;
        if (!powerup_test_tick(menu, 45, error) ||
            !game_test_click_widget(menu, &powers->bar.powerups[0].button, error))
            return false;
        float balance;
        if (!powers->effect_count || powers->seed_stage != 1 ||
            ab_powerup_uses_count(game->uses) != 1 || game->shot.birds_shot != 0 ||
            !ab_settings_inventory_amount(menu->settings, row->name, &balance, error) ||
            balance != (infinite ? 0 : 2))
            return ab_fail(error,
                           "Super Seeds lost its activation or inventory policy");
        if (!powerup_test_tick(menu, 150, error))
            return false;
        bool available;
        float expected = radius * 1.5f;
        if (powers->seed_stage || powers->effect_count || game->active.items[0] ||
            level.bodies->count != count || level.removed_objects[object] ||
            !menu_route_truthy(menu_route_field(bird, "usedPowerPotion")) ||
            menu_route_field(bird, "superseed_timer").kind != AB_DATA_NIL ||
            level.bodies->objects[object].physics.dimensions.radius != expected ||
            fabsf(level.bodies->objects[object].initial_mass -
                  mass * (2.25f * scenes[scene].density)) > 0.0001f ||
            !ab_classic_powerup_manager_can_activate(&powers->manager, 0, &available,
                                                     error) ||
            available)
            return ab_fail(error,
                           "Super Seeds did not finish growth on its actual bird");
        AbClassicDynamicsView dynamics = ab_classic_dynamics_view(
            ab_classic_world_runtime_view(game->world).dynamics);
        AbGameObjectCoreRegistry *registry =
            ab_classic_object_removal_registry(dynamics.physical);
        if (registry->bird_count != level.bird_count)
            return ab_fail(error, "Super Seeds accumulated duplicate native bird rows");
        if (ab_classic_shots_bird(game->playfield.shots, object)->mass !=
            level.bodies->objects[object].initial_mass)
            return ab_fail(error, "Super Seeds launch retained its previous body mass");
        if (scene == 1 && getenv("AB_SEED_CAPTURE")) {
            AbImage image = {0};
            float clear[4];
            menu_background_clear(menu, clear);
            bool okay = ab_metal_capture(renderer, menu->context.draw_list, 1024, 768,
                                         clear, 16 * 1024 * 1024, &image, error) &&
                        save(getenv("AB_SEED_CAPTURE"), &image, error);
            ab_image_free(&image);
            if (!okay)
                return false;
        }
        for (size_t phase = 0; phase < 3; ++phase) {
            AbVec2 target = ab_classic_sling_view(game->playfield.sling).band.rest;
            if (phase) {
                target.x -= 4;
                target.y += 1.5f;
            }
            AbCameraViewport viewport;
            AbVec2 point;
            AbUiPointerReply reply;
            if (!ab_camera_system_query_viewport(
                    ab_classic_camera_system(game->playfield.camera), &viewport) ||
                !ab_coordinate_point(&viewport.rendering, AB_SPACE_PHYSICS,
                                     AB_SPACE_SCREEN, target, &point) ||
                !menu_preview_pointer(menu,
                                      menu_span(phase == 0   ? "LPRESS"
                                                : phase == 1 ? "LHOLD"
                                                             : "LRELEASE"),
                                      point, &reply, error) ||
                !powerup_test_tick(menu, 1, error))
                return false;
        }
        if (!powerup_test_tick(menu, 12, error) || game->shot.birds_shot != 1 ||
            !menu_route_truthy(menu_route_field(bird, "shotWithPowerPotion")))
            return ab_fail(error, "Super Seeds actual mouse launch failed");
        bool trail = false;
        AbParticles *particles = ab_classic_particles_engine(game->particles);
        for (size_t index = 0; index < ab_particles_count(particles, false); ++index)
            trail |= menu_route_equal(
                (AbDataValue){.kind = AB_DATA_STRING,
                              .string =
                                  ab_particles_at(particles, false, index)->definition},
                menu_data("powerPotionTrail"));
        AbDataValue copied = menu_route_field(bird, "particles");
        AbDataValue collision = menu_route_field(copied.table, "collision");
        AbDataValue smoke = menu_route_field(collision.table, "smokeBuff");
        AbDataValue original_collision =
            menu_route_field(original_particles.table, "collision");
        AbDataValue original_smoke =
            menu_route_field(original_collision.table, "smokeBuff");
        if (!trail || copied.kind != AB_DATA_TABLE ||
            copied.table == original_particles.table ||
            !menu_route_equal(menu_route_field(smoke.table, "definition"),
                              menu_data("smokeBuffBirdsPurple")) ||
            !menu_route_equal(menu_route_field(original_smoke.table, "definition"),
                              menu_data("smokeBuffBirdsDefault")))
            return ab_fail(error,
                           "Super Seeds did not isolate its trail/collision particles");
        if (!powerup_test_tick(menu, 180, error))
            return false;
        game_session_free(menu);
        menu->level_request_ready = false;
    }
    menu->infinite_powerups = false;
    menu->keep_powerups_open = false;
    puts("Actual Super Seeds HUD: finite/infinite mouse activation, growing native "
         "circle, "
         "stable bird queue, stronger mouse shot, source trail and isolated purple "
         "collision smoke on Metal.");
    return true;
}

static bool powerup_quake_widgets(PreviewMenu *menu, AbMetal *renderer,
                                  AbError *error) {
    for (unsigned infinite = 0; infinite < 2; ++infinite) {
        menu->infinite_powerups = infinite != 0;
        if (!ability_test_level(menu, "Level1", error))
            return false;
        PreviewGame *game = menu->game;
        PreviewPowerups *powers = game->powerups;
        const AbPowerupMetadata *row = ab_powerup_catalog_at(menu->powerup_catalog, 3);
        if (!ab_settings_inventory_set(menu->settings, row->name,
                                       menu_route_number(infinite ? 0 : 3),
                                       menu_route_number(0), error) ||
            !powerup_test_tick(menu, 245, error))
            return false;
        if (!ab_ui_slider_is_open(&powers->bar.slider) &&
            !game_test_click_widget(menu, &powers->bar.slider.toggle, error))
            return false;
        if (!powerup_test_tick(menu, 45, error) ||
            !game_test_click_widget(menu, &powers->bar.powerups[3].button, error))
            return false;
        float balance;
        if (!powers->quake_active || !powers->effect_count ||
            !menu_route_truthy(game->prevent_ending) ||
            !ab_settings_inventory_amount(menu->settings, row->name, &balance, error) ||
            balance != (infinite ? 0 : 2) || ab_powerup_uses_count(game->uses) != 1 ||
            game->shot.birds_shot != 0)
            return ab_fail(error,
                           "Birdquake HUD lost its activation/inventory boundary");
        for (size_t frame = 0; powers->quake_remaining == 2 && frame < 90; ++frame)
            if (!powerup_test_tick(menu, 1, error))
                return false;
        if (!(powers->quake_remaining < 2) || !powers->quake_sound ||
            game->globals.eagle_darkness.kind != AB_DATA_NUMBER ||
            menu_real(game->globals.eagle_darkness.bits) != 0.5f)
            return ab_fail(error,
                           "Birdquake did not enter its source impulse/darkness phase");
        AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
        bool scared = false;
        for (size_t rank = 0; rank < level.goal_count; ++rank) {
            size_t object = level.goal_order[rank];
            AbDataTable *pig = &level.properties->objects[object].physical;
            scared |= !level.removed_objects[object] &&
                      menu_route_truthy(menu_route_field(pig, "specialExpression")) &&
                      level.character_objects[object].goal.special_expression &&
                      level.character_objects[object].goal.special_timer > 0.9f;
        }
        if (!scared)
            return ab_fail(error,
                           "Birdquake did not publish its scared pig expression");
        if (!powerup_test_tick(menu, 10, error))
            return false;
        bool dust = false;
        AbParticles *particles = ab_classic_particles_engine(game->particles);
        for (size_t index = 0; index < ab_particles_count(particles, true); ++index)
            dust |= menu_route_equal(
                (AbDataValue){.kind = AB_DATA_STRING,
                              .string =
                                  ab_particles_at(particles, true, index)->definition},
                menu_data("quake_smoke"));
        if (!dust)
            return ab_fail(error, "Birdquake did not emit its actual menu dust");
        AbImage image = {0};
        float clear[4];
        menu_background_clear(menu, clear);
        bool okay = ab_metal_capture(renderer, menu->context.draw_list, 1024, 768,
                                     clear, 16 * 1024 * 1024, &image, error);
        const char *capture = infinite ? getenv("AB_QUAKE_CAPTURE") : NULL;
        if (okay && capture)
            okay = save(capture, &image, error);
        ab_image_free(&image);
        if (!okay || !powerup_test_tick(menu, 240, error))
            return false;
        bool available;
        if (powers->quake_active || menu_route_truthy(game->prevent_ending) ||
            game->active.items[0] ||
            !ab_classic_powerup_manager_can_activate(&powers->manager, 3, &available,
                                                     error) ||
            !available ||
            (game->globals.eagle_darkness.kind == AB_DATA_NUMBER &&
             menu_real(game->globals.eagle_darkness.bits) > 0.00001f))
            return ab_fail(
                error, "Birdquake did not reset completion/activation/darkness gates");
        game_session_free(menu);
        menu->level_request_ready = false;
    }
    menu->infinite_powerups = false;
    menu->keep_powerups_open = false;
    puts("Actual Birdquake HUD: finite/infinite activation, source camera/darkness, "
         "scared pigs, "
         "earthquake audio, real world impulses, dust on Metal and completion gates "
         "restored.");
    return true;
}

static bool powerup_scope_widgets(PreviewMenu *menu, AbMetal *renderer,
                                  AbError *error) {
    for (unsigned infinite = 0; infinite < 2; ++infinite) {
        menu->infinite_powerups = infinite != 0;
        if (!ability_test_level(menu, infinite ? "Level15" : "Level1", error))
            return false;
        PreviewGame *game = menu->game;
        PreviewPowerups *powers = game->powerups;
        const AbPowerupMetadata *row = ab_powerup_catalog_at(menu->powerup_catalog, 2);
        if (!ab_settings_inventory_set(menu->settings, row->name,
                                       menu_route_number(infinite ? 0 : 3),
                                       menu_route_number(0), error) ||
            !powerup_test_tick(menu, 245, error))
            return false;
        if (!ab_ui_slider_is_open(&powers->bar.slider) &&
            !game_test_click_widget(menu, &powers->bar.slider.toggle, error))
            return false;
        if (!powerup_test_tick(menu, 45, error) ||
            !game_test_click_widget(menu, &powers->bar.powerups[2].button, error) ||
            !powerup_test_tick(menu, 110, error))
            return false;
        float amount;
        bool available;
        if (!powers->scope_activated || !powers->scope_bound ||
            !ab_classic_sling_view(game->playfield.sling).has_scope ||
            game->active.items[0] || game->passive.items[0] != &powers->scope ||
            !ab_settings_inventory_amount(menu->settings, row->name, &amount, error) ||
            amount != (infinite ? 0 : 2) ||
            !ab_classic_powerup_manager_can_activate(&powers->manager, 2, &available,
                                                     error) ||
            available)
            return ab_fail(error,
                           "Sling Scope lost activation/passive/inventory state");
        if (infinite) {
            for (size_t index = 0; index < 2; ++index)
                if (!game_test_click_widget(menu, &powers->bar.powerups[index].button,
                                            error) ||
                    !powerup_test_tick(menu, 155, error))
                    return false;
            AbClassicSlingView view = ab_classic_sling_view(game->playfield.sling);
            if (!view.king || !view.has_scope || !powers->scope_activated)
                return ab_fail(error, "King Sling discarded the active Sling Scope");
        }
        for (size_t phase = 0; phase < 2; ++phase) {
            AbVec2 target = ab_classic_sling_view(game->playfield.sling).band.rest;
            if (phase) {
                target.x -= 4;
                target.y += 1.5f;
            }
            AbCameraViewport viewport;
            AbVec2 point;
            AbUiPointerReply reply;
            if (!ab_camera_system_query_viewport(
                    ab_classic_camera_system(game->playfield.camera), &viewport) ||
                !ab_coordinate_point(&viewport.rendering, AB_SPACE_PHYSICS,
                                     AB_SPACE_SCREEN, target, &point) ||
                !menu_preview_pointer(menu, menu_span(phase ? "LHOLD" : "LPRESS"),
                                      point, &reply, error) ||
                !powerup_test_tick(menu, phase ? 8 : 1, error))
                return false;
        }
        AbClassicSlingView live = ab_classic_sling_view(game->playfield.sling);
        if (!game->shot.dragged || live.band.distance <= 2 ||
            live.band.scope_sound_eligible || menu->scope_state.velocity.x <= 0)
            return ab_fail(error,
                           "Sling Scope did not reach its real aiming draw/sound");
        AbImage image = {0};
        float clear[4];
        menu_background_clear(menu, clear);
        bool okay = ab_metal_capture(renderer, menu->context.draw_list, 1024, 768,
                                     clear, 16 * 1024 * 1024, &image, error);
        const char *capture = infinite ? getenv("AB_SCOPE_CAPTURE") : NULL;
        if (okay && capture)
            okay = save(capture, &image, error);
        ab_image_free(&image);
        AbUiPointerReply reply;
        if (!okay ||
            !menu_preview_pointer(menu, menu_span("LRELEASE"), (AbVec2){400, 400},
                                  &reply, error) ||
            !powerup_test_tick(menu, 180, error) || game->shot.birds_shot != 1)
            return false;
        float timer = menu->scope_state.timer;
        game_session_free(menu);
        menu->level_request_ready = false;
        if (menu->scope_state.timer != timer)
            return ab_fail(error, "Level teardown reset the retained scope phase");
    }
    menu->infinite_powerups = false;
    menu->keep_powerups_open = false;
    puts("Actual Sling Scope: finite/infinite mouse activation, passive sight, "
         "aim trajectory and sound, combined Super Seeds/King Sling on Metal.");
    return true;
}

static bool powerup_overlapping_widgets(PreviewMenu *menu, AbMetal *renderer,
                                        AbError *error) {
    for (unsigned infinite = 0; infinite < 2; ++infinite) {
        menu->infinite_powerups = infinite != 0;
        if (!ability_test_level(menu, "Level1", error))
            return false;
        PreviewGame *game = menu->game;
        PreviewPowerups *powers = game->powerups;
        for (size_t index = 0; index < 4; ++index) {
            const AbPowerupMetadata *row =
                ab_powerup_catalog_at(menu->powerup_catalog, index);
            if (!ab_settings_inventory_set(menu->settings, row->name,
                                           menu_route_number(infinite ? 0 : 3),
                                           menu_route_number(0), error))
                return false;
        }
        if (!powerup_test_tick(menu, 245, error))
            return false;
        if (!ab_ui_slider_is_open(&powers->bar.slider) &&
            !game_test_click_widget(menu, &powers->bar.slider.toggle, error))
            return false;
        if (!powerup_test_tick(menu, 45, error))
            return false;
        for (size_t index = 0; index < 4; ++index) {
            const AbPowerupMetadata *row =
                ab_powerup_catalog_at(menu->powerup_catalog, index);
            if (!game_test_click_widget(menu, &powers->bar.powerups[index].button,
                                        error))
                return false;
            float balance;
            if (powers->effect_count != index + 1 ||
                game->notifications.frame->child_count != index + 1 ||
                !ab_settings_inventory_amount(menu->settings, row->name, &balance,
                                              error) ||
                balance != (infinite ? 0 : 2))
                return ab_fail(error, "Overlapping powerup %zu lost its notification",
                               index);
        }
        for (size_t index = 1; index < 4; ++index)
            if (!powers->effect_live[index] ||
                powers->effects[index - 1].timer <= powers->effects[index].timer)
                return ab_fail(error,
                               "Powerup notifications shared their animation timer");
        if (ab_powerup_uses_count(game->uses) != 4 ||
            !powerup_test_tick(menu, 18, error))
            return false;
        AbImage image = {0};
        float clear[4];
        menu_background_clear(menu, clear);
        bool okay = ab_metal_capture(renderer, menu->context.draw_list, 1024, 768,
                                     clear, 16 * 1024 * 1024, &image, error);
        const char *capture = infinite ? getenv("AB_POWERUP_OVERLAP_CAPTURE") : NULL;
        if (okay && capture)
            okay = save(capture, &image, error);
        ab_image_free(&image);
        if (!okay)
            return false;
        bool staggered = false;
        for (size_t frame = 0; frame < 100; ++frame) {
            if (!powerup_test_tick(menu, 1, error))
                return false;
            staggered |= powers->effect_count > 0 && powers->effect_count < 4;
            if (game->notifications.frame->child_count != powers->effect_count)
                return ab_fail(error,
                               "Powerup notification removal damaged its siblings");
        }
        if (!staggered || powers->effect_count || !powerup_test_tick(menu, 220, error))
            return ab_fail(error,
                           "Overlapping notifications did not expire independently");
        AbClassicSlingView view = ab_classic_sling_view(game->playfield.sling);
        if (!view.king || !view.has_scope || !powers->scope_activated ||
            powers->seed_stage || powers->quake_active || game->active.items[0] ||
            game->passive.items[0] != &powers->scope)
            return ab_fail(error,
                           "Overlapping powerups lost their independent completion");
        /* Reuse detached notification slots through the actual repeatable quake. */
        if (!game_test_click_widget(menu, &powers->bar.powerups[3].button, error) ||
            powers->effect_count != 1 || !powers->effect_live[0] ||
            !powerup_test_tick(menu, 240, error) || powers->effect_count)
            return ab_fail(error, "Detached powerup notification could not be reused");
        game_session_free(menu);
        menu->level_request_ready = false;
    }
    menu->infinite_powerups = false;
    menu->keep_powerups_open = false;
    puts("Overlapping powerups: four consecutive mouse activations, independent "
         "notification timers/removal, finite/infinite inventory, combined effects "
         "and reused detached slots on Metal.");
    return true;
}

#endif
