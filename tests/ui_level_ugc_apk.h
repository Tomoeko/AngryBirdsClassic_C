#ifndef AB_UI_LEVEL_UGC_APK_H
#define AB_UI_LEVEL_UGC_APK_H

static bool ugc_return_to_levels(PreviewMenu *menu, AbError *error) {
    if (!game_session_escape(menu, error) ||
        !game_test_click_widget(
            menu, &menu->game->pause->page.controls[AB_UI_PAUSE_MENU], error))
        return false;
    return (!menu->game && menu->level_active && menu->entered) ||
           ab_fail(error, "Birdday 5 pause Menu did not restore its entered grid");
}

static bool ugc_level_widgets(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    bool previous_launch = menu->launch_enabled;
    menu->launch_enabled = true;
    AbSpan setting = menu_span("UGCEpisodeCardGiftBoxOpened");
    if (!ab_settings_statistics_write(menu->settings, &setting, 1,
                                      (AbDataValue){.kind = AB_DATA_BOOLEAN}, error) ||
        !ab_episode_preferences_select(&menu->preferences, menu_data("birdday5UGC"),
                                       error) ||
        !menu_episode_change(menu, true, error))
        return false;
    size_t card = 0;
    while (card < menu->episodes->cards.count &&
           menu->episodes->cards.buttons[card].class_kind != AB_UI_EPISODE_UGC)
        ++card;
    size_t anchor = card + 1;
    if (anchor > menu->episodes->cards.scroll.anchor_count)
        anchor = menu->episodes->cards.scroll.anchor_count;
    if (card == menu->episodes->cards.count ||
        !ab_ui_scroll_frame_set_current(&menu->episodes->cards.scroll, anchor, error) ||
        !menu_preview_draw(menu, 0, error) ||
        !game_test_click_widget(menu, &menu->episodes->cards.buttons[card].widget,
                                error))
        return false;
    const AbDataValue *opened = ab_data_get(ab_settings_statistics_root(menu->settings),
                                            menu_data("UGCEpisodeCardGiftBoxOpened"));
    if (!opened || !menu_route_truthy(*opened) || !menu->ugc.normal ||
        !menu->episode_active || menu->loader ||
        ab_particles_count(menu->gift_particles, true) != 100)
        return ab_fail(error, "Birdday 5 gift did not unwrap before level navigation");
    for (size_t index = 0; index < 100; ++index)
        if (!ab_particles_at(menu->gift_particles, true, index)->sprite_handle)
            return ab_fail(error, "Birdday 5 unwrap omitted an actual paper texture");
    AbImage image = {0};
    float clear[] = {0, 0, 0, 1};
    bool okay = ab_metal_capture(renderer, menu->context.draw_list,
                                 (uint32_t)menu->screen.x, (uint32_t)menu->screen.y,
                                 clear, 16 * 1024 * 1024, &image, error);
    const char *capture = getenv("AB_UGC_GIFT_CAPTURE");
    if (okay && capture)
        okay = save(capture, &image, error);
    ab_image_free(&image);
    if (!okay || !game_test_click_widget(
                     menu, &menu->episodes->cards.buttons[card].widget, error))
        return false;
    for (size_t frame = 0; frame < 120 && !menu->level_active; ++frame)
        if (!menu_preview_draw(menu, 1.0f / 60, error))
            return false;
    if (!menu->level_active ||
        menu->episodes->selected_episode.kind != AB_DATA_STRING ||
        !menu_route_equal(menu->levels->episode, menu_data("birdday5UGC")) ||
        menu->levels->levels.count != 30 || menu->levels->levels.page_count != 2)
        return ab_fail(error, "Birdday 5 did not open both real catalog pages");
    if (!menu_preview_draw(menu, 0, error))
        return false;
    if (!route_click(menu, &menu->levels->levels.buttons[0].widget, "GOTO_LEVEL",
                     error))
        return false;
    for (size_t frame = 0; frame < 120 && !menu->game; ++frame)
        if (!menu_preview_draw(menu, 1.0f / 60, error))
            return false;
    if (!menu->game ||
        !menu_route_equal(menu->game->level_name, menu_data("UGC1Level01")))
        return ab_fail(error, "Birdday 5 first actual level did not start");
    for (size_t frame = 0; frame < 120; ++frame)
        if (!game_eagle_test_frame(menu, error))
            return false;
    if (!ugc_return_to_levels(menu, error) ||
        !route_click(menu, &menu->levels->right, "SCROLL_RIGHT", error))
        return false;
    for (size_t frame = 0; frame < 120; ++frame)
        if (!menu_preview_draw(menu, 1.0f / 60, error))
            return false;
    if (menu->levels->current_selection != 2)
        return ab_fail(error, "Birdday 5 second catalog page did not scroll");
    /* Direct catalog resolution covers a later page independently of the
     * fixture's campaign progression; it does not unlock that level for users. */
    const AbDataTable *episodes = ab_episode_catalog_episodes(menu->catalog);
    AbDataEntry fields[] = {{menu_data("episode"), menu_data("birdday5UGC")},
                            {menu_data("page"), menu_route_number(2)},
                            {menu_data("level"), menu_route_number(1)},
                            {menu_data("levelName"), menu_data("UGC1Level16")}};
    AbDataTable metadata = {fields, 4, 4, NULL};
    if (!ab_classic_level_request_resolve(episodes, &metadata, &menu->level_request,
                                          error))
        return false;
    menu->level_request_ready = true;
    for (size_t frame = 0; frame < 120; ++frame)
        if (!game_eagle_test_frame(menu, error))
            return false;
    if (!menu->game ||
        !menu_route_equal(menu->game->level_name, menu_data("UGC1Level16")))
        return ab_fail(error, "Birdday 5 later-page level did not start");
    if (!ugc_return_to_levels(menu, error))
        return false;
    menu->launch_enabled = previous_launch;
    puts("Birdday 5: actual two-click gift route, saved flag, 100 textured paper "
         "particles on Metal, both level pages and gameplay on each page.");
    return true;
}

#endif
