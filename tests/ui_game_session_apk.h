#ifndef AB_UI_GAME_SESSION_APK_TEST_H
#define AB_UI_GAME_SESSION_APK_TEST_H

#include "ab/script_math.h"

static bool game_test_center(AbUiWidget *widget, AbVec2 *point, AbError *error) {
    AbVec2 scale, size, pivot;
    AbUiImageResourceBoundaries images = widget->context->images;
    AbSpan image = widget->state.image.image.text;
    if (!ab_ui_widget_screen_position(widget, point, error) ||
        !ab_ui_widget_cumulative_scale(widget, &scale, error) ||
        !images.bounds(images.context, image, &size, error) ||
        !images.pivot(images.context, image, &pivot, error)) {
        if (!error->message[0])
            ab_fail(error, "Cannot resolve test button geometry for %.*s",
                    (int)image.size, image.data);
        return false;
    }
    float sine, cosine;
    if (!ab_script_sine(menu_real(widget->frame->fields.angle.number_bits), &sine) ||
        !ab_script_cosine(menu_real(widget->frame->fields.angle.number_bits), &cosine))
        return ab_fail(error, "Cannot resolve actual button rotation");
    float x = (size.x * 0.5f - pivot.x) * scale.x;
    float y = (size.y * 0.5f - pivot.y) * scale.y;
    point->x += x * cosine - y * sine;
    point->y += x * sine + y * cosine;
    return true;
}

static bool game_test_click_widget(PreviewMenu *menu, AbUiWidget *widget,
                                   AbError *error) {
    AbVec2 point;
    AbUiPointerReply reply;
    if (!game_test_center(widget, &point, error))
        return false;
    menu->context.left_button_held = false;
    menu->context.touch_count = 0;
    if (!menu_preview_pointer(menu, menu_span("HOVER"), point, &reply, error) ||
        !menu_preview_draw(menu, 1.0f / 60, error))
        return false;
    menu->context.left_button_held = true;
    menu->context.touch_count = 1;
    if (!menu_preview_pointer(menu, menu_span("LPRESS"), point, &reply, error) ||
        !menu_preview_draw(menu, 1.0f / 60, error) ||
        !menu_preview_pointer(menu, menu_span("LHOLD"), point, &reply, error) ||
        !menu_preview_draw(menu, 1.0f / 60, error))
        return false;
    menu->context.left_button_held = false;
    menu->context.touch_count = 0;
    if (!menu_preview_pointer(menu, menu_span("LRELEASE"), point, &reply, error))
        return false;
    return menu_preview_draw(menu, 1.0f / 60, error);
}

static bool result_test_click(PreviewMenu *menu, size_t button, AbError *error) {
    if (!menu->game || !menu->game->results || button >= 3)
        return ab_fail(error, "Missing selected result test button");
    return game_test_click_widget(menu, menu->game->results->buttons + button, error);
}

#include "ui_eagle_game_apk.h"

/* Reenter the second catalog level through its real button/Loader path. The
 * direct route must exit the subscribed grid before pause controls can safely
 * restart or return to it. Repeat after allocator/listener reuse. */
static bool game_test_direct_pause_routes(PreviewMenu *menu, AbError *error) {
    for (size_t round = 0; round < 2; ++round) {
        if (!route_click(menu, &menu->levels->levels.buttons[1].widget, "GOTO_LEVEL",
                         error))
            return false;
        for (size_t frame = 0; frame < 120 && !menu->game; ++frame)
            if (!menu_preview_draw(menu, 1.0f / 60, error))
                return false;
        if (!menu->game)
            return ab_fail(error, "Direct level button did not start its game");
        if (menu->entered || menu->level_active || menu->levels->eagle_subscribed ||
            menu->levels->levels.sync_subscribed)
            return ab_fail(error, "Direct game launch retained level-grid listeners");
        for (size_t action = 0; action < 3; ++action) {
            if (!game_session_escape(menu, error))
                return false;
            for (size_t frame = 0; frame < 15; ++frame)
                if (!menu_preview_draw(menu, 1.0f / 60, error))
                    return false;
            AbUiPauseControl control = action == 0   ? AB_UI_PAUSE_RESUME
                                       : action == 1 ? AB_UI_PAUSE_RESTART
                                                     : AB_UI_PAUSE_MENU;
            if (!game_test_click_widget(
                    menu, &menu->game->pause->page.controls[control], error))
                return false;
            if (action < 2 && (!menu->game || menu->game->paused || menu->game->ended))
                return ab_fail(error, "Pause Resume/Restart did not restore gameplay");
        }
        if (menu->game || !menu->entered || !menu->level_active ||
            !menu->levels->eagle_subscribed || !menu->levels->levels.sync_subscribed)
            return ab_fail(error, "Pause Menu failed its single grid reentry");
    }
    return true;
}

/* Controlled result packets exercise navigation after the separate real level-1
 * win below. They are fixture input, not a native level-2 gameplay comparison. */
static bool result_test_event(PreviewMenu *menu, AbError *error) {
    PreviewGame *game = menu->game;
    AbDataEntry fields[] = {{menu_data("id"), menu_event(menu, "EID_LEVEL_ENDED")},
                            {menu_data("level"), game->level_name},
                            {menu_data("score"), game->globals.score},
                            {menu_data("levelComplete"), game->enabled}};
    AbDataTable event = {fields, 4, 4, NULL};
    return ab_event_manager_notify(menu->events, &event, error) &&
           menu_preview_draw(menu, 1.0f / 60, error) && menu->game->results;
}

static bool game_test_wheel(PreviewMenu *menu, AbError *error) {
    int16_t pcm[512];
    for (size_t frame = 0; frame < 220; ++frame)
        if (!menu_preview_draw(menu, 1.0f / 60, error) ||
            !ab_audio_mixer_mix(menu->mixer, 267, pcm, 512, error))
            return false;
    PreviewGame *game = menu->game;
    AbCameraSystem *system = ab_classic_camera_system(game->playfield.camera);
    float previous = 0;
    const float amounts[] = {16, -16, 16};
    for (size_t step = 0; step < 3; ++step) {
        if (!game_session_scroll(menu, amounts[step],
                                 (AbVec2){game->screen.x * 0.5f, game->screen.y * 0.5f},
                                 error))
            return false;
        for (size_t frame = 0; frame < 15; ++frame)
            if (!menu_preview_draw(menu, 1.0f / 60, error) ||
                !ab_audio_mixer_mix(menu->mixer, 267, pcm, 512, error))
                return false;
        AbCameraSystemView camera = ab_camera_system_view(system);
        float zoom = camera.mover->current_zoom;
        if (zoom < camera.limits->far_zoom || zoom > camera.limits->close_zoom ||
            (step == 1 && !(zoom < previous)) || (step == 2 && !(zoom > previous)))
            return ab_fail(error,
                           "Desktop wheel zoom failed its direction/level bounds");
        previous = zoom;
    }
    return true;
}

/* Continue the real menu/intro request into the application session, send a
 * screen-space mouse shot and submit its real draw list to Metal. */
static bool game_session_widgets(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    if (!menu->level_request_ready || !ab_audio_mixer_start(menu->mixer, true, error) ||
        !menu_preview_draw(menu, 1.0f / 60, error) || !menu->game)
        return error && error->message[0]
                   ? false
                   : ab_fail(error, "Intro did not activate the application game");
    PreviewGame *game = menu->game;
    const AbLuaData *catalog =
        ab_classic_level_view(game->playfield.level).catalog_data;
    AbClassicLevelSource *source = menu->level_source;
    if (!source || !catalog || ab_classic_level_source_bytes(source) > 16 * 1024 * 1024)
        return ab_fail(error, "Game omitted its bounded shared level-source owner");
    size_t pressed = SIZE_MAX, pan_start = SIZE_MAX;
    bool captured = false, panned = false, intro_moved = false;
    bool skip_shown = false, skip_clicked = false;
    ScoreHudCheck score_check = {0};
    bool fast_forward = getenv("AB_TEST_SKIP_END") != NULL;
    AbVec2 pan_position = {0}, intro_middle = {0};
    int16_t pcm[512];
    AbImage image = {0};
    size_t textures = ab_asset_textures_count(menu->textures);
    for (size_t frame = 0; frame < 1800 && !game->ended; ++frame) {
        if ((frame == 60 && !menu_preview_resize(menu, (AbVec2){1920, 1080}, error)) ||
            (frame == 450 && !menu_preview_resize(menu, (AbVec2){1280, 720}, error)))
            return false;
        bool visible = false;
        if (!ab_classic_sling_is_visible(game->playfield.sling, &visible, error))
            return false;
        AbCameraSystemView camera =
            ab_classic_camera_view(game->playfield.camera).system;
        if (frame == 150)
            intro_middle = camera.mover->current_position;
        if (frame == 210)
            intro_moved = !camera.sequence->current &&
                          (intro_middle.x != camera.mover->current_position.x ||
                           intro_middle.y != camera.mover->current_position.y);
        if (pan_start == SIZE_MAX && frame > 240 && game->shot.bird_ready &&
            camera.input_enabled) {
            pan_start = frame;
            pan_position = camera.mover->current_position;
        }
        if (pan_start != SIZE_MAX && frame <= pan_start + 3) {
            AbUiPointerReply reply;
            AbVec2 point = {game->screen.x * 0.8f, game->screen.y * 0.2f};
            if (frame > pan_start)
                point.x -= 180;
            if (!menu_preview_pointer(menu,
                                      menu_span(frame == pan_start       ? "LPRESS"
                                                : frame == pan_start + 3 ? "LRELEASE"
                                                                         : "LHOLD"),
                                      point, &reply, error))
                return false;
        }
        if (pressed == SIZE_MAX && pan_start != SIZE_MAX && frame > pan_start + 30 &&
            game->shot.bird_ready && visible && camera.input_enabled)
            pressed = frame;
        if (pressed != SIZE_MAX && frame <= pressed + 2) {
            AbVec2 target = ab_classic_sling_view(game->playfield.sling).band.rest;
            if (frame > pressed) {
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
                                      menu_span(frame == pressed       ? "LPRESS"
                                                : frame == pressed + 1 ? "LHOLD"
                                                                       : "LRELEASE"),
                                      point, &reply, error))
                return false;
        }
        if (!menu_preview_draw(menu, 1.0f / 60, error) ||
            !ab_audio_mixer_mix(menu->mixer, 267, pcm, 512, error))
            return false;
        if (!game->ended && !score_test_frame(game, renderer, &score_check, error))
            return false;
        if (!game->ended && game->skip.state == AB_UI_SKIP_ONSCREEN &&
            ab_ui_pointer_value_truthy(&game->skip.widget.frame->fields.visible)) {
            if (!menu_route_truthy(game->goal_sent))
                return ab_fail(error,
                               "Fast-forward appeared before goals were cleared");
            skip_shown = true;
            if (fast_forward && !skip_clicked) {
                AbVec2 point;
                AbUiPointerReply reply;
                menu->context.left_button_held = true;
                menu->context.touch_count = 1;
                if (!game_test_center(&game->skip.widget, &point, error) ||
                    !menu_preview_pointer(menu, menu_span("LPRESS"), point, &reply,
                                          error))
                    return false;
                if (!ab_ui_pointer_value_truthy(&game->skip.widget.state.click_started))
                    return ab_fail(
                        error,
                        "Fast-forward press missed at %g,%g (button %g,%g size %g,%g "
                        "scale %g,%g)",
                        point.x, point.y,
                        menu_real(game->skip.widget.frame->fields.x.number_bits),
                        menu_real(game->skip.widget.frame->fields.y.number_bits),
                        menu_real(game->skip.widget.frame->fields.w.number_bits),
                        menu_real(game->skip.widget.frame->fields.h.number_bits),
                        menu_real(game->skip.widget.frame->fields.scale_x.number_bits),
                        menu_real(game->skip.widget.frame->fields.scale_y.number_bits));
                menu->context.left_button_held = false;
                menu->context.touch_count = 0;
                if (!menu_preview_pointer(menu, menu_span("LRELEASE"), point, &reply,
                                          error))
                    return false;
                if (game->skip.state != AB_UI_SKIP_CLICKED ||
                    !menu_route_truthy(game->ending.skip_ending))
                    return ab_fail(error, "Fast-forward state=%d enabled=%d hit=%d",
                                   (int)game->skip.state,
                                   (int)game->skip.widget.state.enabled.kind,
                                   (int)game->skip.widget.state.click_started.kind);
                skip_clicked = true;
            }
        }
        if (pan_start != SIZE_MAX && frame == pan_start + 2) {
            AbVec2 current = ab_classic_camera_view(game->playfield.camera)
                                 .system.mover->current_position;
            panned = current.x != pan_position.x || current.y != pan_position.y;
        }
        if (frame == 400) {
            float clear[4] = {0, 0, 0, 1};
            bool okay = ab_metal_capture(
                renderer, menu->context.draw_list, (uint32_t)game->screen.x,
                (uint32_t)game->screen.y, clear, 16 * 1024 * 1024, &image, error);
            captured = okay && image.rgba.size && menu->context.draw_list->count;
            ab_image_free(&image);
            if (!captured)
                return false;
        }
        if (!game->ended && textures != ab_asset_textures_count(menu->textures))
            return ab_fail(error, "Game resize reloaded its texture resources");
    }
    if (!game->ended || !game->finish.complete.completed ||
        game->shot.birds_shot <= 0 || !captured ||
        !(menu_real(game->globals.score.bits) > 0) || !panned || !intro_moved ||
        !skip_shown || (fast_forward && !skip_clicked))
        return ab_fail(error,
                       "Application game did not reach its actual winning result");
    if (!score_check.zero || !score_check.total || !score_check.hit ||
        !score_check.pig || !score_check.expired)
        return ab_fail(error, "Gameplay score checks incomplete: %d/%d/%d/%d/%d",
                       score_check.zero, score_check.total, score_check.hit,
                       score_check.pig, score_check.expired);
    const AbDataValue *saved =
        ab_data_get(ab_settings_statistics_root(menu->highscores), game->level_name);
    if (!saved || saved->kind != AB_DATA_TABLE ||
        !menu_route_truthy(menu_route_field(saved->table, "completed")) ||
        !menu_route_equal(menu_route_field(saved->table, "score"),
                          game->globals.score) ||
        !menu_route_equal(menu_route_field(saved->table, "legitScore"),
                          game->globals.score) ||
        !menu_route_equal(menu_route_field(saved->table, "birds"),
                          menu_route_number(game->shot.birds_shot)))
        return ab_fail(error,
                       "Game result did not publish its actual highscore fields");
    if (!menu_preview_resize(menu, (AbVec2){1024, 768}, error) ||
        !menu_preview_draw(menu, 1.0f / 60, error))
        return false;
    if (!game->results)
        return ab_fail(error, "Winning level did not construct its score card");
    AbUiPointerReply ignored;
    if (!menu_preview_pointer(menu, menu_span("HOVER"), (AbVec2){0, 0}, &ignored,
                              error) ||
        !menu_preview_pointer(menu, menu_span("LPRESS"), (AbVec2){0, 0}, &ignored,
                              error) ||
        !menu_preview_pointer(menu, menu_span("LRELEASE"), (AbVec2){0, 0}, &ignored,
                              error) ||
        !menu_preview_draw(menu, 1.0f / 60, error) ||
        game->results->pending_action.kind != AB_UI_POINTER_NONE)
        return ab_fail(error, "Unhandled score-card input became a navigation action");
    for (size_t frame = 0; frame < 300; ++frame)
        if (!menu_preview_draw(menu, 1.0f / 60, error) ||
            !ab_audio_mixer_mix(menu->mixer, 267, pcm, 512, error))
            return false;
    for (size_t index = 0; (float)index < game->results->stars_earned; ++index)
        if (!game->results->star_achieved[index])
            return ab_fail(error, "Score card did not reveal its earned stars");
    if (!game->results->badge_shown || game->results->shade != 0.5f ||
        !menu_equal(game->results->best.state.fields.text, "MI_NEW_HIGHSCORE"))
        return ab_fail(error, "Score card did not reach its actual highscore state");
    float clear[4] = {0, 0, 0, 1};
    if (!ab_metal_capture(renderer, menu->context.draw_list, 1024, 768, clear,
                          16 * 1024 * 1024, &image, error))
        return false;
    const char *capture = getenv("AB_RESULT_CAPTURE");
    if (capture && !save(capture, &image, error))
        return false;
    ab_image_free(&image);
    AbDataValue next_levels = menu_route_field(menu->level_request.page, "levels");
    AbDataValue next_record = result_at(next_levels.table, 2);
    AbDataValue next_name = menu_route_field(next_record.table, "name");
    if (!result_test_click(menu, 2, error))
        return false;
    if (!menu->game || menu->game->ended ||
        !menu_route_equal(menu->level_request.name, next_name))
        return ab_fail(error, "Next selected %s (active=%d, ended=%d)",
                       menu->level_request.resource_path, menu->game != NULL,
                       menu->game ? menu->game->ended : false);
    if (!game_test_wheel(menu, error))
        return false;
    if (menu->level_source != source ||
        ab_classic_level_view(menu->game->playfield.level).catalog_data != catalog)
        return ab_fail(error, "Next decoded another immutable block catalog");
    if (!result_test_event(menu, error) || !result_test_click(menu, 1, error) ||
        !menu->game || menu->game->ended || menu->game->shot.birds_shot != 0 ||
        !menu_route_equal(menu->level_request.name, next_name))
        return ab_fail(error, "Restart did not reset the selected level");
    if (!result_test_event(menu, error) || !result_test_click(menu, 0, error) ||
        menu->game || !menu->level_active || menu->episode_active ||
        menu->cutscene_active)
        return ab_fail(error, "Result Menu did not return to the level grid");
    puts("Connected game: intro launch, actual mouse shot, live physics/scoring, "
         "Metal frame, two native size changes without asset reload, and winning "
         "result.");
    return game_test_direct_pause_routes(menu, error) &&
           game_test_eagle(menu, renderer, error);
}

#endif
