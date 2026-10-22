#ifndef AB_UI_INTRO_ROUTE_APK_TEST_H
#define AB_UI_INTRO_ROUTE_APK_TEST_H

static bool intro_route_open(const AbZip *zip, PreviewMenu *menu, AbMetal *renderer,
                             AbDrawList *list, const char *directory, AbError *error) {
    menu->highest_resources = true;
    menu->screen = (AbVec2){1024, 768};
    menu->state_directory = directory;
    menu->navigation_enabled = menu->route_enabled = menu->launch_enabled = true;
    return menu_preview_open(zip,
                             (AbAssetTextureHost){renderer, upload, remove_texture},
                             list, menu, error) &&
           menu_background_open(menu, error);
}

static bool intro_route_widgets(const AbZip *zip, PreviewMenu *menu, AbMetal *renderer,
                                AbDrawList *list, const char *capture, AbError *error) {
    const char *directory = AB_TEST_STATE_DIRECTORY;
    const char *paths[] = {AB_TEST_STATE_DIRECTORY "/settings.lua",
                           AB_TEST_STATE_DIRECTORY "/settings_local.lua",
                           AB_TEST_STATE_DIRECTORY "/highscores.lua"};
    const char *stage = "fresh isolated persistent profile";
    AbImage image = {0};
    bool okay = false, owned_paths = false;
    for (size_t index = 0; index < 3; ++index)
        if (cc_path_information(paths[index], false, NULL) != CC_PATH_MISSING)
            return ab_fail(error,
                           "Intro persistence test requires absent fixture files");
    owned_paths = true;
    menu_preview_free(menu);
    if (!intro_route_open(zip, menu, renderer, list, directory, error) ||
        ab_settings_file_loaded(menu->settings_file) || !route_enter(menu, error) ||
        !route_click(menu, &menu->episodes->cards.buttons[0].widget,
                     "GOTO_LEVEL_SELECTION", error))
        goto release;
    for (size_t frame = 0; frame < 8 && !menu->level_active; ++frame)
        if (!menu_preview_draw(menu, 0.01f, error))
            goto release;
    if (!menu->level_active || !menu_preview_draw(menu, 0, error))
        goto release;
    stage = "retained intro request and separate Loader completion";
    AbDataTable *meta = &menu->levels->levels.buttons[0].meta;
    if (!route_click(menu, &menu->levels->levels.buttons[0].widget, "GOTO_LEVEL",
                     error) ||
        !menu->loader || !menu->loading_page || menu->cutscene_active ||
        level_test_child(&menu->intro_play, menu_data("data")) != meta ||
        menu_route_field(&menu->intro_play, "from").kind != AB_DATA_NIL)
        goto release;
    for (size_t frame = 0; frame < 8 && !menu->cutscene_active; ++frame)
        if (!menu_preview_draw(menu, 0.01f, error))
            goto release;
    if (!menu->cutscene_active || menu->level_active || menu->episode_active ||
        menu->loader || menu->loading_page || !menu->cutscene->sprite_present ||
        menu->cutscene->command_index != 5 ||
        level_test_child(&menu->intro_end, menu_data("data")) != meta ||
        menu_route_field(&menu->intro_end, "from").kind != AB_DATA_NIL)
        goto release;
    stage = "connected intro Metal draw and native resize";
    float clear[4];
    menu_background_clear(menu, clear);
    size_t textures = ab_asset_textures_count(menu->textures);
    AbUiCutscene *identity = menu->cutscene;
    if (clear[0] != 0 || clear[1] != 0 || clear[2] != 0 || clear[3] != 1 ||
        !ab_metal_capture(renderer, list, 1024, 768, clear, 16 * 1024 * 1024, &image,
                          error) ||
        (capture && !save(capture, &image, error)) ||
        !menu_preview_resize(menu, (AbVec2){1920, 1080}, error) ||
        !menu_preview_draw(menu, 0, error) || menu->cutscene != identity ||
        textures != ab_asset_textures_count(menu->textures) ||
        !menu_preview_resize(menu, (AbVec2){1024, 768}, error))
        goto release;
    stage = "natural completion writes encrypted watched state";
    if (!menu_preview_draw(menu, 16, error) ||
        menu->requested_launch != &menu->intro_end || !menu->level_request_ready ||
        strcmp(menu->level_request.resource_path,
               "assets/data/levels/pack1/Level1.lua") ||
        menu->level_request.current_level_number != 1 ||
        !ab_settings_file_loaded(menu->settings_file) ||
        !menu_preview_flush(menu, error))
        goto release;
    stage = "fresh host restores watched state and menu preferences";
    if (!game_session_widgets(menu, renderer, error))
        goto release;
    menu_preview_free(menu);
    if (!intro_route_open(zip, menu, renderer, list, directory, error) ||
        !ab_settings_file_loaded(menu->settings_file) ||
        !ab_settings_file_loaded(menu->local_file) ||
        !ab_settings_file_loaded(menu->highscores_file))
        goto release;
    const AbDataTable *watched = level_test_child(
        ab_settings_statistics_root(menu->settings), menu_data("cutscenesWatched"));
    AbDataValue done = menu_route_field(watched, "pack1_intro");
    AbDataValue selected = {0};
    if (done.kind != AB_DATA_BOOLEAN || !done.bits ||
        !ab_episode_preferences_selected(&menu->preferences, &selected, error) ||
        !menu_route_equal(selected, level_test_number(1)) ||
        !route_enter(menu, error) || menu->episodes->current_selection != 1)
        goto release;
    const AbDataTable *saved = level_test_child(
        ab_settings_statistics_root(menu->highscores), menu_data("Level1"));
    if (!saved || !menu_route_truthy(menu_route_field(saved, "completed")) ||
        menu_route_field(saved, "score").kind != AB_DATA_NUMBER ||
        !(menu_real(menu_route_field(saved, "score").bits) > 0))
        goto release;
    puts("Connected intro: main/episode/level route, real Loader events, story Metal "
         "draw, native resize, encrypted natural-end save and fresh-host restore. "
         "CHANGE_LEVEL activated the real game session through its winning result.");
    okay = true;
release:
    if (!okay && !error->message[0])
        ab_fail(error, "Connected intro differs at %s", stage);
    menu_preview_free(menu);
    ab_image_free(&image);
    if (owned_paths)
        for (size_t index = 0; index < 3; ++index)
            if (cc_path_information(paths[index], false, NULL) == CC_PATH_FILE &&
                remove(paths[index]))
                okay = false;
    return okay;
}

#endif
