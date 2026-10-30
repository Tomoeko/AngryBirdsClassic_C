#ifndef AB_UI_SOUNDBOARD_APK_H
#define AB_UI_SOUNDBOARD_APK_H

static bool soundboard_test_event(void *context, AbDataTable *event, AbError *error) {
    AbDataValue id = menu_route_field(event, "id");
    if (!menu_route_equal(id, menu_data("EID_GE_LEVEL_STARTED")) &&
        !menu_route_equal(id, menu_data("EID_GOLDEN_EGG_STAR_GAINED")))
        return ab_fail(error, "Unexpected soundboard fixture event");
    ++*(size_t *)context;
    return true;
}

static bool minigame_test_request(PreviewMenu *menu, const char *requested,
                                  AbError *error) {
    if (!ab_audio_mixer_start(menu->mixer, true, error))
        return false;
    AbDataValue episode =
        menu_route_field(ab_episode_catalog_episodes(menu->catalog), "G");
    AbDataValue pages = menu_route_field(episode.table, "pages");
    for (size_t page = 1; page <= pages.table->count; ++page) {
        AbDataValue record = minigame_at(pages.table, page);
        AbDataValue levels = menu_route_field(record.table, "levels");
        for (size_t index = 1; index <= levels.table->count; ++index) {
            AbDataValue level = minigame_at(levels.table, index);
            AbDataValue name = menu_route_field(level.table, "name");
            if (!menu_route_equal(name, menu_data(requested)))
                continue;
            AbDataEntry fields[] = {
                {menu_data("episode"), menu_data("G")},
                {menu_data("page"), menu_route_number((float)page)},
                {menu_data("level"), menu_route_number((float)index)},
                {menu_data("levelName"), name}};
            AbDataTable metadata = {fields, 4, 4, NULL};
            if (!ab_classic_level_request_resolve(
                    ab_episode_catalog_episodes(menu->catalog), &metadata,
                    &menu->level_request, error))
                return false;
            menu->level_request_ready = true;
            return menu_preview_draw(menu, 0, error);
        }
    }
    return ab_fail(error, "Missing actual soundboard catalog entry");
}

static bool soundboard_test_click(PreviewMenu *menu, AbUiWidget *widget,
                                  AbError *error) {
    AbVec2 point;
    AbUiPointerReply reply;
    return ab_ui_widget_screen_position(widget, &point, error) &&
           menu_preview_pointer(menu, menu_span("LPRESS"), point, &reply, error) &&
           menu_preview_pointer(menu, menu_span("LRELEASE"), point, &reply, error) &&
           game_eagle_test_frame(menu, error);
}

static bool minigame_test_image_center(AbUiWidget *widget, AbVec2 *point,
                                       AbError *error) {
    AbVec2 scale, bounds, pivot;
    AbUiPointerValue image = widget->state.image.image;
    AbUiImageResourceBoundaries *images = &widget->context->images;
    if (image.kind != AB_UI_POINTER_TEXT ||
        !ab_ui_widget_cumulative_scale(widget, &scale, error) ||
        !images->bounds(images->context, image.text, &bounds, error) ||
        !images->pivot(images->context, image.text, &pivot, error))
        return false;
    AbUiFrameGeometry geometry = {
        .position = {menu_real(widget->frame->fields.x.number_bits),
                     menu_real(widget->frame->fields.y.number_bits)},
        .scale = scale};
    AbUiImageHit hit = {.frame = &geometry,
                        .sprite_size = bounds,
                        .sprite_pivot = pivot,
                        .image_present = true,
                        .angle_radians =
                            menu_real(widget->frame->fields.angle.number_bits)};
    AbUiHitRectangle rectangle;
    bool present;
    if (!ab_ui_image_button_rectangle(&hit, 16, &rectangle, &present) || !present)
        return ab_fail(error, "Radio fixture lost its actual image hit rectangle");
    *point = rectangle.center;
    return true;
}

static bool minigame_test_image_click(PreviewMenu *menu, AbUiWidget *widget,
                                      AbError *error) {
    AbVec2 point;
    AbUiPointerReply reply;
    if (!minigame_test_image_center(widget, &point, error))
        return false;
    menu->context.left_button_held = true;
    if (!menu_preview_pointer(menu, menu_span("LPRESS"), point, &reply, error))
        return false;
    menu->context.left_button_held = false;
    return menu_preview_pointer(menu, menu_span("LRELEASE"), point, &reply, error) &&
           game_eagle_test_frame(menu, error);
}

static bool soundboard_widgets_test(PreviewMenu *menu, AbMetal *renderer,
                                    AbError *error) {
    menu->route_enabled = true;
    size_t starts = 0, stars = 0;
    AbEventListener started = {menu_span("SoundboardStartFixture"), &starts,
                               soundboard_test_event};
    AbEventListener collected = {menu_span("SoundboardStarFixture"), &stars,
                                 soundboard_test_event};
    if (!ab_event_manager_add(menu->events, menu_event(menu, "EID_GE_LEVEL_STARTED"),
                              &started, error) ||
        !ab_event_manager_add(menu->events,
                              menu_event(menu, "EID_GOLDEN_EGG_STAR_GAINED"),
                              &collected, error) ||
        !minigame_test_request(menu, "SOUNDBOARD1", error))
        return false;
    for (size_t frame = 0; frame < 180 && (!menu_minigame_root(menu) || menu->loader ||
                                           menu_route_truthy(menu->skip_input));
         ++frame)
        if (!game_eagle_test_frame(menu, error))
            return false;
    SoundboardScene *owner = (SoundboardScene *)menu->minigame;
    AbDataValue played;
    if (!owner || !owner->base.active || menu->game || starts != 1 ||
        owner->base.widget.frame->child_count != 19)
        return ab_fail(error, "Soundboard source Loader/scene did not complete");
    if (!ab_settings_golden_egg_played(menu->highscores, menu_span("SOUNDBOARD1"),
                                       &played, error) ||
        !menu_route_truthy(played) ||
        !ambient_capture(menu, renderer, "AB_SOUNDBOARD_CAPTURE", error))
        return false;
    const size_t sequence[] = {1, 8, 4, 4, 4, 10, 13};
    for (size_t index = 0; index < sizeof(sequence) / sizeof(sequence[0]); ++index)
        if (!soundboard_test_click(menu, &owner->buttons[sequence[index] - 1].widget,
                                   error))
            return false;
    AbDataValue highscore =
        menu_route_field(ab_settings_statistics_root(menu->highscores), "SOUNDBOARD1");
    if (!owner->base.star_collected || stars != 1 || !menu->reward ||
        !menu_route_truthy(menu_route_field(highscore.table, "completed")) ||
        owner->state != SOUNDBOARD_LAUNCHED || owner->specialty)
        return ab_fail(error, "Soundboard puzzle sequence lost its actual star/state");
    for (size_t frame = 0; frame < 240; ++frame)
        if (!game_eagle_test_frame(menu, error) ||
            (frame == 60 &&
             !ambient_capture(menu, renderer, "AB_SOUNDBOARD_STAR_CAPTURE", error)))
            return false;
    if (menu->reward ||
        !soundboard_test_click(menu, &owner->buttons[12].widget, error) || stars != 1)
        return ab_fail(error, "Soundboard repeated pig click duplicated the star");
    bool enabled, changed;
    if (!menu_audio_enabled(menu, &enabled, error) ||
        !soundboard_test_click(menu, &owner->base.audio, error) ||
        !menu_audio_enabled(menu, &changed, error) || enabled == changed ||
        !soundboard_test_click(menu, &owner->base.audio, error) ||
        !menu_audio_enabled(menu, &changed, error) || enabled != changed)
        return ab_fail(error, "Soundboard sound toggle did not follow single clicks");
    AbVec2 screen = menu->screen;
    if (!menu_preview_resize(menu, (AbVec2){1600, 900}, error) ||
        !game_eagle_test_frame(menu, error) ||
        !ambient_capture(menu, renderer, "AB_SOUNDBOARD_WIDE_CAPTURE", error) ||
        !menu_preview_resize(menu, screen, error) ||
        !soundboard_test_click(menu, &owner->base.back, error) || menu->minigame ||
        !menu->level_active || !menu->egg_levels ||
        !ab_event_manager_remove(menu->events, menu_event(menu, "EID_GE_LEVEL_STARTED"),
                                 &started, error) ||
        !ab_event_manager_remove(menu->events,
                                 menu_event(menu, "EID_GOLDEN_EGG_STAR_GAINED"),
                                 &collected, error))
        return ab_fail(error, "Soundboard resize/back continuation failed");
    puts("Actual Golden Egg soundboard: pinned descriptor table, real Loader/GUI, "
         "audio sequence, one star/settings/popup, sound toggle, resize, Metal and "
         "Back.");
    return true;
}

#endif
