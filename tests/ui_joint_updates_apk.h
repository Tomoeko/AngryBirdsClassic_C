#ifndef AB_UI_JOINT_UPDATES_APK_H
#define AB_UI_JOINT_UPDATES_APK_H

/* Real repeated Matilda launches/egg drops in the reported catalog scene. This
 * reproduces the missing break-joint binding; it is not a native playthrough. */
static bool joint_impact_game_widgets(PreviewMenu *menu, AbMetal *renderer,
                                      AbError *error) {
    AbVec2 previous = menu->screen;
    if (!ability_test_episode_level(menu, 2, "LevelP2_80", error))
        return false;
    PreviewGame *game = menu->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    size_t initial = level.bodies->count, joints = level.joints->joint_count;
    size_t phase = 0, shots = 0, after = 0, eggs = 0;
    bool okay = true;
    int16_t pcm[512];
    for (size_t frame = 0; okay && frame < 4800 && !game->ended; ++frame) {
        if (shots && after == 300 && game->shot.bird_ready)
            okay = ab_camera_system_goto_snap(
                ab_classic_camera_system(game->playfield.camera), 0, true, true, NULL,
                error);
        bool visible = false;
        if (okay)
            okay = ab_classic_sling_is_visible(game->playfield.sling, &visible, error);
        if (okay && visible && frame > 240 && game->shot.bird_ready &&
            (phase || !shots || after > 360) && shots < 4 &&
            ab_classic_camera_view(game->playfield.camera).system.input_enabled) {
            AbVec2 target = ab_classic_sling_view(game->playfield.sling).band.rest;
            if (phase)
                target.x -= 4;
            AbCameraViewport viewport;
            AbVec2 point;
            AbUiPointerReply reply;
            okay = ab_camera_system_query_viewport(
                       ab_classic_camera_system(game->playfield.camera), &viewport) &&
                   ab_coordinate_point(&viewport.rendering, AB_SPACE_PHYSICS,
                                       AB_SPACE_SCREEN, target, &point) &&
                   menu_preview_pointer(menu,
                                        menu_span(phase == 0   ? "LPRESS"
                                                  : phase == 1 ? "LHOLD"
                                                               : "LRELEASE"),
                                        point, &reply, error);
            if (++phase == 3) {
                phase = 0;
                ++shots;
                after = 0;
            }
        }
        ++after;
        if (okay && after == 70 && game->flying.kind == AB_DATA_TABLE) {
            AbUiPointerReply reply;
            okay = menu_preview_pointer(menu, menu_span("LPRESS"), (AbVec2){1300, 300},
                                        &reply, error) &&
                   menu_preview_pointer(menu, menu_span("LRELEASE"),
                                        (AbVec2){1300, 300}, &reply, error);
        }
        if (okay)
            okay = menu_preview_draw(menu, 1.0f / 60, error) &&
                   ab_audio_mixer_mix(menu->mixer, 267, pcm, 512, error);
        eggs = level.bodies->count - initial;
    }
    size_t notifications = ab_classic_joint_updates_notifications(game->joint_updates);
    if (okay && (!notifications || level.joints->joint_count >= joints ||
                 game->shot.birds_shot < 3 || eggs < 2 || level.world->failed ||
                 ab_classic_joint_updates_tail(game->joint_updates)->destroy->count))
        okay =
            ab_fail(error,
                    "Repeated Matilda impacts: events=%zu joints=%zu/%zu "
                    "shots=%g eggs=%zu pending=%zu failed=%u",
                    notifications, level.joints->joint_count, joints,
                    (double)game->shot.birds_shot, eggs,
                    ab_classic_joint_updates_tail(game->joint_updates)->destroy->count,
                    (unsigned)level.world->failed);
    if (okay) {
        AbImage image = {0};
        float clear[4];
        menu_background_clear(menu, clear);
        okay = ab_metal_capture(renderer, menu->context.draw_list, 1920, 1080, clear,
                                16 * 1024 * 1024, &image, error);
        ab_image_free(&image);
    }
    game_session_free(menu);
    menu->level_request_ready = false;
    if (okay)
        okay = menu_preview_resize(menu, previous, error);
    if (okay)
        printf("Mighty Hoax level 8: %zu Matilda egg drops, %zu destruction events, "
               "deferred native joint removal and Metal output.\n",
               eggs, notifications);
    return okay;
}

static bool joint_impact_widgets(const AbZip *zip, PreviewMenu *menu, AbMetal *renderer,
                                 AbDrawList *list, AbError *error) {
    menu_preview_free(menu);
    menu->highest_resources = true;
    menu->screen = (AbVec2){1920, 1080};
    menu->launch_enabled = true;
    bool okay =
        menu_preview_open(zip, (AbAssetTextureHost){renderer, upload, remove_texture},
                          list, menu, error) &&
        ab_audio_mixer_start(menu->mixer, true, error) &&
        menu_background_open(menu, error) &&
        joint_impact_game_widgets(menu, renderer, error);
    menu_preview_free(menu);
    return okay;
}

#endif
