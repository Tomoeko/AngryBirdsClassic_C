#ifndef AB_UI_MENU_LEVEL_ROUTE_APK_TEST_H
#define AB_UI_MENU_LEVEL_ROUTE_APK_TEST_H

static bool menu_level_route_widgets(PreviewMenu *menu, AbMetal *renderer,
                                     const char *path, AbError *error) {
    const char *stage = "enter episode selection";
    AbImage captured = {0};
    bool okay = false;
    menu->route_enabled = true;
    menu->episodes->host.context = menu;
    menu->episodes->host.load_theme = menu_load_theme;
    if (!ab_episode_preferences_select(&menu->preferences, level_test_number(1),
                                       error) ||
        !ab_episode_preferences_set_page(&menu->preferences, level_test_number(1),
                                         level_test_number(1), error) ||
        !route_enter(menu, error) ||
        !route_click(menu, &menu->episodes->cards.buttons[0].widget,
                     "GOTO_LEVEL_SELECTION", error) ||
        !menu->loader || !menu->loading_page || menu->level_active)
        goto release;
    stage = "Loader input gate and actual callable";
    AbUiPointerReply ignored;
    if (!menu_preview_pointer(menu, menu_span("LRELEASE"), (AbVec2){0}, &ignored,
                              error) ||
        ignored.values[0].kind != AB_UI_POINTER_NONE)
        goto release;
    for (size_t frame = 0; frame < 8 && !menu->level_active; ++frame)
        if (!menu_preview_draw(menu, 0.01f, error))
            goto release;
    if (!menu->level_active || menu->episode_active || menu->loader ||
        menu->loading_page || !menu->levels || menu->levels->levels.count != 63 ||
        menu->levels->current_selection != 1)
        goto release;
    /* Loader set skip_input in its completion update. The next update consumes
     * that frame gate before another actual pointer operation. */
    if (!menu_preview_draw(menu, 0, error))
        goto release;
    stage = "Metal level destination and intro event";
    float clear[4];
    menu_background_clear(menu, clear);
    if (!ab_metal_capture(renderer, menu->context.draw_list, 1024, 768, clear,
                          16 * 1024 * 1024, &captured, error) ||
        (path && !save(path, &captured, error)) ||
        !route_click(menu, &menu->levels->levels.buttons[0].widget, "GOTO_LEVEL",
                     error) ||
        menu->requested_launch != &menu->levels->launch_event ||
        level_test_child(menu->requested_launch, menu_data("data")) !=
            &menu->levels->levels.buttons[0].meta)
        goto release;
    stage = "native resize retains resources and page selection";
    size_t textures = ab_asset_textures_count(menu->textures);
    AbUiLevelSelection *identity = menu->levels;
    if (!menu_preview_resize(menu, (AbVec2){1920, 1080}, error) ||
        !menu_preview_draw(menu, 0, error) || menu->levels != identity ||
        ab_asset_textures_count(menu->textures) != textures ||
        !route_click(menu, &menu->levels->right, "SCROLL_RIGHT", error))
        goto release;
    for (size_t frame = 0; frame < 64; ++frame)
        if (!menu_preview_draw(menu, 0.1f, error))
            goto release;
    if (menu->levels->current_selection != 2 ||
        !route_click(menu, &menu->levels->back, "EPISODE_SELECTION", error) ||
        !menu->episode_active || menu->level_active ||
        !route_click(menu, &menu->episodes->cards.buttons[0].widget,
                     "GOTO_LEVEL_SELECTION", error))
        goto release;
    for (size_t frame = 0; frame < 8 && !menu->level_active; ++frame)
        if (!menu_preview_draw(menu, 0.01f, error))
            goto release;
    if (!menu->level_active || menu->levels->current_selection != 2 ||
        !menu_preview_draw(menu, 0, error) ||
        !route_click(menu, &menu->levels->back, "EPISODE_SELECTION", error))
        goto release;
    stage = "second episode loads real theme4";
    if (!route_click(menu, &menu->episodes->cards.buttons[1].widget,
                     "GOTO_LEVEL_SELECTION", error))
        goto release;
    for (size_t frame = 0; frame < 8 && !menu->level_active; ++frame)
        if (!menu_preview_draw(menu, 0.01f, error))
            goto release;
    if (!menu->level_active || menu->levels->levels.count != 42 ||
        menu->theme_name.size != 6 || memcmp(menu->theme_name.data, "theme4", 6) ||
        !menu_preview_draw(menu, 0, error) ||
        !route_click(menu, &menu->levels->back, "EPISODE_SELECTION", error) ||
        !route_click(menu, &menu->episodes->back, "GOTO_MAIN_MENU", error) ||
        menu->episode_active || menu->level_active ||
        !menu_preview_resize(menu, (AbVec2){1024, 768}, error))
        goto release;
    puts("Connected Metal menu: Play, episode Loader, input gate, level screen, "
         "native resize, saved page reentry, Back and real theme1/theme4 transitions.");
    okay = true;
release:
    ab_image_free(&captured);
    if (!okay && !error->message[0])
        ab_fail(error, "Connected level route differs at %s", stage);
    return okay;
}

#endif
