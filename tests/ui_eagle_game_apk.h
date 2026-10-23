#ifndef AB_UI_EAGLE_GAME_APK_TEST_H
#define AB_UI_EAGLE_GAME_APK_TEST_H

static bool game_eagle_test_frame(PreviewMenu *menu, AbError *error) {
    int16_t pcm[512];
    return menu_preview_draw(menu, 1.0f / 60, error) &&
           ab_audio_mixer_mix(menu->mixer, 267, pcm, 512, error);
}

static bool game_eagle_test_capture(PreviewMenu *menu, AbMetal *renderer,
                                    const char *variable, AbError *error) {
    const char *path = getenv(variable);
    if (!path)
        return true;
    AbImage image = {0};
    float clear[] = {0, 0, 0, 1};
    bool okay =
        ab_metal_capture(renderer, menu->context.draw_list,
                         (uint32_t)menu->game->screen.x, (uint32_t)menu->game->screen.y,
                         clear, 16 * 1024 * 1024, &image, error) &&
        save(path, &image, error);
    ab_image_free(&image);
    return okay;
}

static size_t game_eagle_test_blocks(PreviewGame *game) {
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    size_t count = 0;
    for (size_t object = 0; object < level.bodies->count; ++object) {
        const AbDataTable *physical = &level.properties->objects[object].physical;
        if (!level.removed_objects[object] && level.bodies->objects[object].placement &&
            game_eagle_value(physical, "mass") > 0 &&
            !menu_route_truthy(menu_route_field(physical, "controllable")) &&
            !menu_route_truthy(menu_route_field(physical, "levelGoal")))
            ++count;
    }
    return count;
}

static bool game_eagle_test_visible(PreviewGame *game, bool *visible, AbError *error) {
    *visible = false;
    AbCameraViewport viewport;
    if (!ab_camera_system_query_viewport(
            ab_classic_camera_system(game->playfield.camera), &viewport))
        return ab_fail(error, "Missing Eagle flight viewport");
    AbClassicSceneView scene =
        ab_classic_scene_view(ab_classic_render_scene_objects(game->playfield.scene));
    AbDrawCommand command;
    AbDrawList expected = {&command, 0, 1};
    const size_t first = 0;
    if (!ab_sprite_scene_draw(scene.items + game->eagle->bird, 1, &first,
                              viewport.rendering, (uint32_t)game->screen.x,
                              (uint32_t)game->screen.y, &expected, error))
        return false;
    if (!expected.count)
        return true;
    const AbDrawList *actual = game->menu->context.draw_list;
    for (size_t index = 0; index < actual->count; ++index) {
        const AbDrawCommand *draw = actual->commands + index;
        if (draw->texture == command.texture &&
            !memcmp(&draw->quad, &command.quad, sizeof(command.quad))) {
            *visible = true;
            break;
        }
    }
    return true;
}

static bool game_test_eagle(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    int16_t pcm[512];
    for (size_t frame = 0; frame < 300; ++frame)
        if (!ab_audio_mixer_mix(menu->mixer, 267, pcm, 512, error))
            return false;
    bool previous_infinite = menu->infinite_powerups;
    menu->infinite_powerups = true;
    for (size_t round = 0; round < 2; ++round) {
        if (!route_click(menu, &menu->levels->levels.buttons[1].widget, "GOTO_LEVEL",
                         error))
            return false;
        for (size_t frame = 0;
             frame < 300 && (!menu->game || !menu->game->shot.bird_ready); ++frame)
            if (!game_eagle_test_frame(menu, error))
                return false;
        PreviewGame *game = menu->game;
        if (!game || !game->eagle || !game->shot.bird_ready ||
            !ab_ui_pointer_value_truthy(
                &game->eagle->button.widget.frame->fields.visible) ||
            !ab_ui_pointer_value_truthy(&game->eagle->button.widget.state.enabled))
            return ab_fail(error,
                           "Infinite Mighty Eagle button unavailable in a fresh level");
        const AbDataValue *ordinary_record = ab_data_get(
            ab_settings_statistics_root(menu->highscores), game->level_name);
        AbDataValue ordinary_score =
            ordinary_record && ordinary_record->kind == AB_DATA_TABLE
                ? menu_route_field(ordinary_record->table, "score")
                : menu_route_number(0);
        if (!menu_preview_resize(menu, (AbVec2){1920, 1080}, error) ||
            !game_session_escape(menu, error) || !game_eagle_test_frame(menu, error) ||
            !game_session_escape(menu, error) || !game_eagle_test_frame(menu, error) ||
            (round == 0 && !game_eagle_test_capture(
                               menu, renderer, "AB_EAGLE_BUTTON_CAPTURE", error)) ||
            !game_test_click_widget(menu, &game->eagle->button.widget, error))
            return false;
        if (game->eagle->bait == SIZE_MAX ||
            !menu_route_truthy(game->eagle_bait_launched) ||
            ab_ui_pointer_value_truthy(
                &game->eagle->button.widget.frame->fields.visible))
            return ab_fail(
                error,
                "Mighty Eagle button did not replace the queue with actual bait");
        for (size_t frame = 0;
             frame < 300 && (!game->shot.bird_ready || !game->shot.current_bird_name ||
                             strcmp(game->shot.current_bird_name, "BaitSardine_1"));
             ++frame)
            if (!game_eagle_test_frame(menu, error))
                return false;
        if (!game->shot.bird_ready || !game->shot.current_bird_name ||
            strcmp(game->shot.current_bird_name, "BaitSardine_1"))
            return ab_fail(
                error,
                "Sardine bait did not enter the live slingshot queue (ready=%d "
                "current=%s index=%g fill=%d)",
                game->shot.bird_ready,
                game->shot.current_bird_name ? game->shot.current_bird_name : "nil",
                (double)ab_classic_sling_view(game->playfield.sling).current_bird_index,
                ab_classic_sling_view(game->playfield.sling).fill_in_next_bird);
        for (size_t frame = 0; frame < 3; ++frame) {
            AbVec2 target = ab_classic_sling_view(game->playfield.sling).band.rest;
            if (frame) {
                target.x -= 4;
                target.y += 2;
            }
            AbCameraViewport viewport;
            AbVec2 point;
            AbUiPointerReply reply;
            if (!ab_camera_system_query_viewport(
                    ab_classic_camera_system(game->playfield.camera), &viewport) ||
                !ab_coordinate_point(&viewport.rendering, AB_SPACE_PHYSICS,
                                     AB_SPACE_SCREEN, target, &point) ||
                !menu_preview_pointer(menu,
                                      menu_span(frame == 0   ? "LPRESS"
                                                : frame == 1 ? "LHOLD"
                                                             : "LRELEASE"),
                                      point, &reply, error) ||
                !game_eagle_test_frame(menu, error))
                return false;
        }
        bool eagle_created = false, eagle_captured = false, eagle_drawn = false;
        size_t blocks_before = SIZE_MAX;
        for (size_t frame = 0; frame < 1800 && !game->ended; ++frame) {
            if (!game_eagle_test_frame(menu, error))
                return false;
            if (game->eagle->bird != SIZE_MAX) {
                eagle_created = true;
                if (blocks_before == SIZE_MAX)
                    blocks_before = game_eagle_test_blocks(game);
                AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
                bool visible;
                if (!game_eagle_test_visible(game, &visible, error))
                    return false;
                eagle_drawn |= visible;
                if (level.world->definition.maximum_translation != 5)
                    return ab_fail(
                        error, "Mighty Eagle did not select its real physics limit");
                if (!eagle_captured && visible &&
                    game_eagle_true(game_eagle_physical(game, game->eagle->bird),
                                    "hitGround")) {
                    eagle_captured = true;
                    if (round == 0 &&
                        !game_eagle_test_capture(menu, renderer,
                                                 "AB_EAGLE_FLIGHT_CAPTURE", error))
                        return false;
                    AbImage image = {0};
                    float clear[] = {0, 0, 0, 1};
                    if (!ab_metal_capture(renderer, menu->context.draw_list, 1920, 1080,
                                          clear, 16 * 1024 * 1024, &image, error))
                        return false;
                    ab_image_free(&image);
                }
            }
        }
        size_t blocks_after = game_eagle_test_blocks(game);
        if (!eagle_drawn || !eagle_captured || blocks_before == SIZE_MAX ||
            blocks_after >= blocks_before)
            return ab_fail(error,
                           "Eagle flight omitted visible impact/destruction "
                           "(drawn=%d impact=%d blocks=%zu/%zu)",
                           eagle_drawn, eagle_captured, blocks_after, blocks_before);
        if (!eagle_created || !game->ended || !game->results || !game->result_complete)
            return ab_fail(
                error,
                "Mighty Eagle did not complete (created=%d ended=%d result=%d timer=%g "
                "moving=%d completing=%d ending=%g bird=%zu x=%g y=%g hit=%d "
                "outside=%d)",
                eagle_created, game->ended, game->result_complete,
                (double)menu_real(game->eagle_timer.timer.bits),
                menu_route_truthy(game->eagle_timer.moving),
                game->finish.complete.completing,
                (double)game->finish.complete.ending_timer, game->eagle->bird,
                (double)game_eagle_value(game_eagle_physical(game, game->eagle->bird),
                                         "x"),
                (double)game_eagle_value(game_eagle_physical(game, game->eagle->bird),
                                         "y"),
                game_eagle_true(game_eagle_physical(game, game->eagle->bird),
                                "hitGround"),
                game_eagle_true(game_eagle_physical(game, game->eagle->bird),
                                "outsideBoundaries"));
        for (size_t frame = 0; frame < 240; ++frame)
            if (!game_eagle_test_frame(menu, error))
                return false;
        const AbDataValue *record = ab_data_get(
            ab_settings_statistics_root(menu->highscores), game->level_name);
        if (!game->results->eagle || !record || record->kind != AB_DATA_TABLE ||
            !menu_route_equal(menu_route_field(record->table, "score"),
                              ordinary_score) ||
            game->eagle->percent < 0 || game->eagle->percent > 100 ||
            game->results->feather_value != game->eagle->percent ||
            !game->results->counter.state.fields.text.text.size ||
            game->results->counter.state.fields.text.text
                    .data[game->results->counter.state.fields.text.text.size - 1] !=
                '%')
            return ab_fail(error, "Eagle result corrupted the ordinary score or lost "
                                  "its percentage meter");
        if (round == 0 &&
            !game_eagle_test_capture(menu, renderer, "AB_EAGLE_RESULT_CAPTURE", error))
            return false;
        if (!result_test_click(menu, 0, error) || menu->game || !menu->level_active)
            return ab_fail(error,
                           "Mighty Eagle result did not return to the level grid");
        printf("Mighty Eagle round %zu: visible flight and impact, %zu of %zu "
               "dynamic structure blocks destroyed.\n",
               round + 1, blocks_before - blocks_after, blocks_before);
    }
    menu->infinite_powerups = previous_infinite;
    puts("Connected Mighty Eagle: real HUD clicks, queued bait, flight, cleanup, Metal "
         "draws and repeat level use in infinite mode.");
    return true;
}

#endif
