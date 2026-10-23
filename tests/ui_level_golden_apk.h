#ifndef AB_UI_LEVEL_GOLDEN_APK_TEST_H
#define AB_UI_LEVEL_GOLDEN_APK_TEST_H

static bool golden_test_flag(PreviewMenu *menu, AbSpan level, const char *name,
                             bool value, AbError *error) {
    AbDataValue row;
    AbSpan path[] = {menu_span("openGoldenEggLevels"), level, menu_span(name)};
    return ab_settings_golden_egg(menu->highscores, level, &row, error) &&
           ab_settings_statistics_write(
               menu->highscores, path, 3,
               (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = value}, error);
}

static bool golden_saved_progress(PreviewMenu *menu, AbUiLevelScroll *scroll,
                                  AbError *error) {
    const char *directory = AB_TEST_STATE_DIRECTORY "/golden-progress";
    CcPathKind kind = cc_path_information(directory, false, NULL);
    if (kind != CC_PATH_DIRECTORY &&
        (kind != CC_PATH_MISSING || !cc_directory_create(directory)))
        return ab_fail(error, "Cannot prepare owned Golden Eggs save fixture");
    PreviewMenu fixture = {.archive = menu->archive, .state_directory = directory};
    bool okay = menu_highscores_open(&fixture, error);
    for (size_t index = 0; okay && index < scroll->count; ++index) {
        AbDataValue name = menu_route_field(scroll->buttons[index].metadata, "name");
        okay = golden_test_flag(&fixture, name.string, "unlocked", true, error) &&
               golden_test_flag(&fixture, name.string, "opened", true, error);
    }
    okay = okay &&
           ab_settings_file_save(fixture.highscores_file, fixture.highscores, error);
    ab_settings_statistics_close(fixture.highscores);
    ab_settings_file_close(fixture.highscores_file);
    fixture.highscores = NULL;
    fixture.highscores_file = NULL;
    if (okay)
        okay = menu_highscores_open(&fixture, error);
    for (size_t index = 0; okay && index < scroll->count; ++index) {
        AbDataValue name = menu_route_field(scroll->buttons[index].metadata, "name");
        AbDataValue unlocked = {0}, played = {0};
        okay = ab_settings_golden_egg_unlocked(fixture.highscores, name.string,
                                               &unlocked, error) &&
               ab_settings_golden_egg_played(fixture.highscores, name.string, &played,
                                             error) &&
               unlocked.kind == AB_DATA_BOOLEAN && unlocked.bits &&
               played.kind == AB_DATA_BOOLEAN && played.bits;
    }
    ab_settings_statistics_close(fixture.highscores);
    ab_settings_file_close(fixture.highscores_file);
    return okay || ab_fail(error, "Golden Eggs save did not restore all 32 entries");
}

static bool golden_level_widgets(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    AbDataValue episode =
        menu_route_field(ab_episode_catalog_episodes(menu->catalog), "G");
    AbDataValue pages = menu_route_field(episode.table, "pages");
    const AbDataValue *second = ab_data_get(pages.table, menu_route_number(2));
    AbDataValue levels = menu_route_field(second->table, "levels");
    for (size_t index = 0; index < levels.table->count; ++index) {
        const AbDataValue *record =
            ab_data_get(levels.table, menu_route_number((float)index + 1));
        AbDataValue name = menu_route_field(record->table, "name");
        if (!golden_test_flag(menu, name.string, "unlocked", false, error))
            return false;
    }
    if (!golden_test_flag(menu, menu_span("LevelGE_4"), "unlocked", false, error))
        return false;
    menu->episodes->selected_episode = menu_data("G");
    if (!menu_route_level(menu, error) || !menu->egg_levels ||
        menu->levels->levels.count != 32 || menu->levels->levels.golden.count != 32 ||
        menu->levels->levels.scroll.anchor_count != 1)
        return ab_fail(error, "Golden Eggs omitted its actual entries/page unlock");
    AbUiLevelSelection *screen = menu->levels;
    AbUiLevelScroll *scroll = &screen->levels;
    if (!golden_saved_progress(menu, scroll, error))
        return false;
    if (ab_ui_pointer_value_truthy(&scroll->buttons[0].widget.state.enabled) ||
        !ab_ui_pointer_value_truthy(&scroll->buttons[0].widget.frame->fields.visible) ||
        ab_ui_pointer_value_truthy(&scroll->buttons[15].widget.frame->fields.visible) ||
        ab_ui_pointer_value_truthy(&screen->right.frame->fields.visible) ||
        ab_ui_pointer_value_truthy(&screen->dots[0].frame->fields.visible) ||
        ab_ui_pointer_value_truthy(&menu->egg_levels->king_pig.frame->fields.visible))
        return ab_fail(error, "Golden Eggs silhouette/hidden-page/source gates differ");
    for (size_t index = 0; index < 32; ++index) {
        AbUiLevelButton *button = scroll->buttons + index;
        AbClassicLevelRequest request;
        if (button->widget.frame->child_count != 1 || button->number.widget.initialized)
            return ab_fail(error, "Golden Egg %zu lost its actual button class", index);
        if (!ab_classic_level_request_resolve(
                ab_episode_catalog_episodes(menu->catalog), &button->meta, &request,
                error))
            return false;
        /* These five original entries construct registered music/toy scenes;
         * they have no physical-level Lua file. Their controllers remain open. */
        const char *const toys[] = {"SOUNDBOARD1", "RADIO", "KEYBOARD", "SEQUENCER",
                                    "ACCORDION"};
        bool toy = false;
        for (size_t item = 0; item < sizeof(toys) / sizeof(*toys); ++item)
            toy = toy || menu_route_equal(request.name, menu_data(toys[item]));
        if (!toy && !ab_zip_find(menu->archive, request.resource_path))
            return ab_fail(error, "Golden Egg %zu lacks its actual physical file",
                           index);
    }
    float clear[4] = {0, 0, 0, 1};
    menu_background_clear(menu, clear);
    AbImage image = {0};
    bool okay = menu_preview_draw(menu, 1.0f / 60, error) &&
                ab_metal_capture(renderer, menu->context.draw_list,
                                 (uint32_t)menu->screen.x, (uint32_t)menu->screen.y,
                                 clear, 16 * 1024 * 1024, &image, error);
    const char *capture = getenv("AB_GOLDEN_CAPTURE");
    if (okay && capture)
        okay = save(capture, &image, error);
    ab_image_free(&image);
    if (!okay ||
        !golden_test_flag(menu, menu_span("LevelGE_11"), "unlocked", true, error) ||
        !golden_test_flag(menu, menu_span("LevelGE_11"), "opened", false, error) ||
        !ab_ui_widget_lifecycle(&screen->widget, AB_UI_FRAME_EXIT, error) ||
        !ab_ui_widget_lifecycle(&screen->widget, AB_UI_FRAME_ENTRY, error))
        return false;
    if (scroll->scroll.anchor_count != 2 ||
        !ab_ui_pointer_value_truthy(&screen->right.frame->fields.visible) ||
        !ab_ui_pointer_value_truthy(&screen->dots[1].frame->fields.visible) ||
        !ab_ui_pointer_value_truthy(&scroll->golden.halos[15].frame->fields.visible) ||
        !route_click(menu, &screen->right, "SCROLL_RIGHT", error))
        return ab_fail(error, "Unlocked Golden Egg did not expose its second page");
    for (size_t frame = 0; frame < 90; ++frame)
        if (!menu_preview_draw(menu, 1.0f / 60, error))
            return false;
    if (screen->current_selection != 2 ||
        !route_click(menu, &scroll->buttons[15].widget, "GOTO_LEVEL", error))
        return ab_fail(error, "Golden Eggs page-two selection route failed");
    if (!ab_metal_capture(renderer, menu->context.draw_list, (uint32_t)menu->screen.x,
                          (uint32_t)menu->screen.y, clear, 16 * 1024 * 1024, &image,
                          error))
        return false;
    capture = getenv("AB_GOLDEN_SECOND_CAPTURE");
    okay = !capture || save(capture, &image, error);
    ab_image_free(&image);
    AbSpan score[] = {menu_span("LevelGE_11"), menu_span("completed")};
    if (!okay ||
        !golden_test_flag(menu, menu_span("LevelGE_11"), "opened", true, error) ||
        !ab_settings_statistics_create_table(menu->highscores, score, 1, 2, error) ||
        !ab_settings_statistics_write(menu->highscores, score, 2,
                                      (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1},
                                      error) ||
        !ab_ui_level_scroll_refresh(scroll, error) ||
        ab_ui_pointer_value_truthy(&scroll->golden.halos[15].frame->fields.visible) ||
        !ab_ui_pointer_value_truthy(&scroll->buttons[15].stars.frame->fields.visible))
        return ab_fail(error, "Golden Eggs played/completed state did not refresh");
    float previous =
        menu_real(scroll->golden.halos[15].frame->fields.angle.number_bits);
    if (!menu_preview_draw(menu, 0.25f, error) ||
        menu_real(scroll->golden.halos[15].frame->fields.angle.number_bits) !=
            previous + 0.25f * 0.6f ||
        !ab_ui_widget_lifecycle(&screen->widget, AB_UI_FRAME_EXIT, error))
        return ab_fail(error, "Golden Eggs halo clock or exit lifecycle failed");
    /* Exercise the source's explicit debug-build bypass without changing any
     * ordinary level scores or granting campaign completion. */
    menu->egg_levels->host.release_build = false;
    if (!ab_ui_widget_lifecycle(&screen->widget, AB_UI_FRAME_ENTRY, error) ||
        !ab_ui_pointer_value_truthy(
            &menu->egg_levels->king_pig.frame->fields.visible) ||
        !(menu_real(menu->egg_levels->king_pig.frame->fields.w.number_bits) > 0) ||
        !ab_ui_widget_lifecycle(&screen->widget, AB_UI_FRAME_EXIT, error))
        return ab_fail(error, "King Pig debug gate or actual sprite is unavailable");
    menu->egg_levels->host.release_build = true;
    if (!ab_ui_widget_lifecycle(&screen->widget, AB_UI_FRAME_ENTRY, error) ||
        ab_ui_pointer_value_truthy(&menu->egg_levels->king_pig.frame->fields.visible) ||
        !menu_episode_change(menu, true, error))
        return ab_fail(error, "King Pig release gate did not return to actual scores");
    const AbDataTable *episodes = ab_episode_catalog_ids(menu->catalog);
    for (size_t index = 1; index <= episodes->count; ++index) {
        const AbDataValue *id = ab_data_get(episodes, menu_route_number((float)index));
        if (!id)
            return ab_fail(error, "Actual selection episode list is sparse");
        menu->episodes->selected_episode = *id;
        if (!menu_route_level(menu, error) ||
            !menu_preview_draw(menu, 1.0f / 60, error) ||
            !menu_episode_change(menu, true, error))
            return false;
    }
    puts("Golden Eggs: 32 canonical entries, source ImageButton/star classes, "
         "silhouettes, hidden/unlocked second page, Metal selection, played/completed "
         "refresh and halo rotation; all 14 episode level-screen factories open.");
    return true;
}

#endif
