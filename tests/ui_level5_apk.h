#ifndef AB_UI_LEVEL5_APK_H
#define AB_UI_LEVEL5_APK_H

static bool level5_reward_widgets(const AbZip *zip, PreviewMenu *menu,
                                  AbMetal *renderer, AbDrawList *list, AbError *error) {
    const char *directory = AB_TEST_STATE_DIRECTORY "/level5-reward";
    const char *const paths[] = {
        AB_TEST_STATE_DIRECTORY "/level5-reward/settings.lua",
        AB_TEST_STATE_DIRECTORY "/level5-reward/settings_local.lua",
        AB_TEST_STATE_DIRECTORY "/level5-reward/highscores.lua"};
    for (size_t path = 0; path < 3; ++path)
        if (cc_path_information(paths[path], false, NULL) != CC_PATH_MISSING)
            return ab_fail(error, "Level5 requires absent owned reward fixture files");
    const char *stage = "fresh Level5 application session";
    menu_preview_free(menu);
    bool opened = intro_route_open(zip, menu, renderer, list, directory, error) &&
                  ability_test_level(menu, "Level5", error);
    bool okay = false;
    AbImage image = {0};
    if (!opened)
        goto release;
    stage = "actual intro camera sequence";
    for (size_t frame = 0;
         frame < 360 &&
         !ab_classic_camera_view(menu->game->playfield.camera).system.input_enabled;
         ++frame)
        if (!menu_preview_draw(menu, 1.0f / 30, error))
            goto release;
    stage = "actual treasure chest and mouse zoom";
    AbClassicLevelView level = ab_classic_level_view(menu->game->playfield.level);
    AbDataValue chest =
        menu_route_field(&level.properties->world, "ExtraTreasureChest_1");
    if (chest.kind != AB_DATA_TABLE)
        goto release;
    AbVec2 point = {0};
    bool visible = false;
    for (size_t attempt = 0; attempt < 64; ++attempt) {
        AbDataValue x = menu_route_field(chest.table, "x"),
                    y = menu_route_field(chest.table, "y");
        AbCameraViewport viewport;
        if (x.kind != AB_DATA_NUMBER || y.kind != AB_DATA_NUMBER ||
            !ab_camera_system_query_viewport(
                ab_classic_camera_system(menu->game->playfield.camera), &viewport) ||
            !ab_coordinate_point(&viewport.rendering, AB_SPACE_PHYSICS, AB_SPACE_SCREEN,
                                 (AbVec2){menu_real(x.bits), menu_real(y.bits)},
                                 &point))
            goto release;
        visible = point.x > 30 && point.y > 90 && point.x < menu->screen.x - 30 &&
                  point.y < menu->screen.y - 30;
        if (visible)
            break;
        if (!game_session_scroll(menu, -1, (AbVec2){512, 384}, error) ||
            !menu_preview_draw(menu, 1.0f / 30, error))
            goto release;
    }
    if (!visible) {
        ab_fail(error, "Level5 chest stayed outside the viewport at %.2f,%.2f", point.x,
                point.y);
        goto release;
    }
    stage = "four actual mouse edges and shared root double-click";
    for (size_t edge = 0; edge < 4; ++edge) {
        AbUiPointerReply reply;
        if (!menu_preview_pointer(menu, menu_span(edge % 2 ? "LRELEASE" : "LPRESS"),
                                  point, &reply, error) ||
            !menu_preview_draw(menu, 1.0f / 60, error))
            goto release;
    }
    stage = "source unlock, zero-score record, event and reward draw";
    AbDataValue unlocked;
    const AbDataValue *record = ab_data_get(
        ab_settings_statistics_root(menu->highscores), menu_data("LevelGE_15"));
    if (!menu->reward || !record || record->kind != AB_DATA_TABLE ||
        menu_route_field(record->table, "completed").kind != AB_DATA_BOOLEAN ||
        menu_route_truthy(menu_route_field(record->table, "completed")) ||
        !menu_route_equal(menu_route_field(record->table, "score"),
                          menu_route_number(0)) ||
        !ab_settings_golden_egg_unlocked(menu->highscores, menu_span("LevelGE_15"),
                                         &unlocked, error) ||
        !menu_route_truthy(unlocked) || menu->game->egg_event.count != 2 ||
        !menu_preview_draw(menu, 0.5f, error))
        goto release;
    float clear[4];
    menu_background_clear(menu, clear);
    if (!ab_metal_capture(renderer, list, 1024, 768, clear, 16 * 1024 * 1024, &image,
                          error))
        goto release;
    stage = "encrypted reward reload";
    menu_preview_free(menu);
    if (!intro_route_open(zip, menu, renderer, list, directory, error) ||
        !ab_settings_file_loaded(menu->highscores_file) ||
        !ab_settings_golden_egg_unlocked(menu->highscores, menu_span("LevelGE_15"),
                                         &unlocked, error) ||
        !menu_route_truthy(unlocked))
        goto release;
    puts("Actual Level5: intro camera, mouse zoom and double-click treasure reward, "
         "zero-score record, gained event, Metal notification and encrypted reload.");
    okay = true;
release:
    if (!okay && !error->message[0])
        ab_fail(error, "Level5 reward failed at %s", stage);
    ab_image_free(&image);
    menu_preview_free(menu);
    for (size_t path = 0; path < 3; ++path)
        (void)remove(paths[path]);
    return okay && intro_route_open(zip, menu, renderer, list, NULL, error);
}

#endif
