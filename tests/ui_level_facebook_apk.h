#ifndef AB_UI_LEVEL_FACEBOOK_APK_TEST_H
#define AB_UI_LEVEL_FACEBOOK_APK_TEST_H

static bool facebook_level_widgets(PreviewMenu *menu, AbMetal *renderer,
                                   AbError *error) {
    menu->episodes->selected_episode = menu_route_number(5);
    if (!menu_route_level(menu, error) || menu->levels->levels.count != 48 ||
        menu->levels->levels.page_count != 4)
        return ab_fail(error, "Ham 'Em High omitted its original fourth level page");
    AbUiLevelSelection *levels = menu->levels;
    AbUiLevelFacebook *facebook = levels->levels.facebook + 3;
    if (!facebook->present ||
        !(menu_real(facebook->like.frame->fields.w.number_bits) > 0))
        return ab_fail(error, "Facebook page omitted its actual Like artwork");
    for (size_t page = 1; page < 4; ++page) {
        if (!route_click(menu, &levels->right, "SCROLL_RIGHT", error))
            return false;
        for (size_t frame = 0; frame < 90; ++frame)
            if (!menu_preview_draw(menu, 1.0f / 60, error))
                return false;
    }
    if (levels->current_selection != 4)
        return ab_fail(error, "Source page arrows did not reach the restored levels");
    for (size_t index = 45; index < 48; ++index) {
        AbUiLevelButton *button = levels->levels.buttons + index;
        if (!ab_ui_pointer_value_truthy(&button->widget.state.enabled) ||
            button->page_metadata != facebook->page ||
            !route_click(menu, &button->widget, "GOTO_LEVEL", error))
            return ab_fail(error, "Facebook level %zu is not locally selectable",
                           index);
        AbClassicLevelRequest request;
        if (!ab_classic_level_request_resolve(
                ab_episode_catalog_episodes(menu->catalog), &button->meta, &request,
                error) ||
            !ab_zip_find(menu->archive, request.resource_path))
            return ab_fail(error, "Restored Facebook level has no actual APK file");
    }
    AbImage image = {0};
    float clear[4] = {0, 0, 0, 1};
    menu_background_clear(menu, clear);
    bool okay = menu_preview_draw(menu, 0, error) &&
                ab_metal_capture(renderer, menu->context.draw_list,
                                 (uint32_t)menu->screen.x, (uint32_t)menu->screen.y,
                                 clear, 16 * 1024 * 1024, &image, error);
    const char *capture = getenv("AB_FACEBOOK_CAPTURE");
    if (okay && capture)
        okay = save(capture, &image, error);
    ab_image_free(&image);
    AbSpan info[] = {menu_span("ageGenderInfo"), menu_span("isUnderThirteen")};
    if (!okay ||
        !ab_settings_statistics_create_table(menu->settings, info, 1, 1, error) ||
        !ab_settings_statistics_write(menu->settings, info, 2,
                                      (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1},
                                      error) ||
        !ab_ui_widget_lifecycle(&levels->widget, AB_UI_FRAME_EXIT, error) ||
        !ab_ui_widget_lifecycle(&levels->widget, AB_UI_FRAME_ENTRY, error) ||
        !menu_preview_draw(menu, 0, error))
        return false;
    if (ab_ui_pointer_value_truthy(&facebook->like.frame->fields.visible))
        return ab_fail(error, "Live age filter did not hide the social control");
    for (size_t index = 45; index < 48; ++index)
        if (!ab_ui_pointer_value_truthy(
                &levels->levels.buttons[index].widget.state.enabled))
            return ab_fail(error, "Age filter incorrectly locked a local level");
    /* Creation and re-entry are distinct source branches: a filtered fresh
     * page never creates the Like child, so its level row stays at mid-height. */
    if (!menu_route_level(menu, error))
        return false;
    levels = menu->levels;
    facebook = levels->levels.facebook + 3;
    if (facebook->present || levels->levels.count != 48)
        return ab_fail(error, "Filtered page construction changed its local levels");
    for (size_t index = 45; index < 48; ++index) {
        AbUiLevelButton *button = levels->levels.buttons + index;
        float y = menu_real(button->widget.frame->fields.y.number_bits);
        if (!ab_ui_pointer_value_truthy(&button->widget.state.enabled) ||
            fabsf(y - menu->screen.y * 0.5f) > 0.001f)
            return ab_fail(error, "Filtered local level did not use its source row");
    }
    if (!ab_settings_statistics_write(menu->settings, info, 1, (AbDataValue){0}, error))
        return false;
    puts("Ham 'Em High: restored fourth page, all 48 catalog levels, three local "
         "Facebook-page selections, real resource paths and live/fresh age-filter "
         "behavior.");
    return true;
}

#endif
