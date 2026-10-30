#ifndef AB_UI_ACCORDION_APK_H
#define AB_UI_ACCORDION_APK_H

static bool accordion_test_enter(PreviewMenu *menu, AccordionScene **output,
                                 AbError *error) {
    if (!minigame_test_request(menu, "ACCORDION", error))
        return false;
    for (size_t frame = 0; frame < 180 && (!menu_minigame_root(menu) || menu->loader ||
                                           menu_route_truthy(menu->skip_input));
         ++frame)
        if (!game_eagle_test_frame(menu, error))
            return false;
    *output = (AccordionScene *)menu->minigame;
    return (*output && (*output)->base.active && !menu->game &&
            (*output)->base.widget.frame->child_count == 13) ||
           ab_fail(error, "Accordion source Loader/GUI did not complete");
}

static bool accordion_test_press(PreviewMenu *menu, AccordionScene *owner,
                                 AbVec2 *point, AbError *error) {
    AbUiPointerReply reply;
    if (!minigame_test_image_center(&owner->handle, point, error))
        return false;
    menu->context.left_button_held = true;
    return menu_preview_pointer(menu, menu_span("LPRESS"), *point, &reply, error) &&
           owner->state == ACCORDION_DRAGGING;
}

static bool accordion_test_move(PreviewMenu *menu, AbVec2 point, AbError *error) {
    AbUiPointerReply reply;
    return menu_preview_pointer(menu, menu_span("LHOLD"), point, &reply, error) &&
           game_eagle_test_frame(menu, error);
}

static bool accordion_widgets_test(PreviewMenu *menu, AbMetal *renderer,
                                   AbError *error) {
    menu->route_enabled = true;
    size_t starts = 0, stars = 0;
    AbEventListener started = {menu_span("AccordionStartFixture"), &starts,
                               soundboard_test_event};
    AbEventListener collected = {menu_span("AccordionStarFixture"), &stars,
                                 soundboard_test_event};
    AccordionScene *owner;
    if (!ab_event_manager_add(menu->events, menu_event(menu, "EID_GE_LEVEL_STARTED"),
                              &started, error) ||
        !ab_event_manager_add(menu->events,
                              menu_event(menu, "EID_GOLDEN_EGG_STAR_GAINED"),
                              &collected, error) ||
        !accordion_test_enter(menu, &owner, error) || starts != 1 ||
        !ambient_capture(menu, renderer, "AB_ACCORDION_CAPTURE", error))
        return false;
    const char *const notes[] = {"cminor", "dismajor", "fmajor", "gminor", "bmajor"};
    for (size_t index = 0; index < 5; ++index) {
        size_t left = 0, right = 0;
        if (!minigame_test_image_click(menu, &owner->buttons[index].widget, error) ||
            !menu_route_equal(owner->note, menu_data(notes[index])) ||
            !ab_audio_mixer_track_count(menu->mixer, 5, &left, error) ||
            !ab_audio_mixer_track_count(menu->mixer, 6, &right, error) || left != 1 ||
            right != 1)
            return ab_fail(error, "Accordion note %zu did not retain its stereo loops",
                           index + 1);
    }
    AbVec2 point;
    if (!accordion_test_press(menu, owner, &point, error))
        return ab_fail(error, "Actual accordion handle did not begin dragging");
    float start = point.x;
    point.x += owner->width * .25f;
    if (!accordion_test_move(menu, point, error) || !owner->direction_changed ||
        owner->direction <= 0 || owner->volume <= 0 ||
        ab_audio_mixer_view(menu->mixer).track_gains[5] != 0 ||
        ab_audio_mixer_view(menu->mixer).track_gains[6] <= 0 ||
        !menu_route_equal(owner->pig_images[0].value, menu_data("GE_GRANDPA_04_SMILE")))
        return ab_fail(error, "Accordion extension lost direction, pig or track gain");
    bool reversed = false;
    for (size_t step = 1; step <= 20; ++step) {
        point.x = start + owner->width * (.25f - (float)step * .025f);
        if (!accordion_test_move(menu, point, error))
            return false;
        reversed |= owner->direction_changed && owner->direction < 0;
    }
    if (!reversed || owner->direction >= 0 ||
        ab_audio_mixer_view(menu->mixer).track_gains[5] <= 0 ||
        ab_audio_mixer_view(menu->mixer).track_gains[6] != 0)
        return ab_fail(error, "Accordion compression did not select its left loop");
    AbUiPointerReply reply;
    menu->context.left_button_held = false;
    if (!menu_preview_pointer(menu, menu_span("LRELEASE"), point, &reply, error))
        return false;
    for (size_t frame = 0; frame < 60; ++frame)
        if (!game_eagle_test_frame(menu, error))
            return false;
    if (owner->state != ACCORDION_IDLE ||
        menu_real(owner->handle.frame->fields.x.number_bits) != owner->handle_x ||
        stars || !minigame_test_image_click(menu, &owner->pig.widget, error) ||
        menu_real(ab_ui_toggle_button_get_state(&owner->pig).number_bits) != 2)
        return ab_fail(error,
                       "Accordion release/pig toggle did not retain source state");
    bool enabled, changed;
    if (!menu_audio_enabled(menu, &enabled, error) ||
        !minigame_test_image_click(menu, &owner->base.audio, error) ||
        !menu_audio_enabled(menu, &changed, error) || changed == enabled ||
        ab_audio_mixer_view(menu->mixer).track_gains[5] != 0 ||
        ab_audio_mixer_view(menu->mixer).track_gains[6] != 0 || !owner->sound_muted ||
        !minigame_test_image_click(menu, &owner->base.audio, error) ||
        owner->sound_muted)
        return ab_fail(error,
                       "Accordion audio toggle did not mute both directional tracks");
    AbVec2 screen = menu->screen;
    if (!menu_preview_resize(menu, (AbVec2){1600, 900}, error) ||
        !game_eagle_test_frame(menu, error) ||
        !ambient_capture(menu, renderer, "AB_ACCORDION_WIDE_CAPTURE", error) ||
        !menu_preview_resize(menu, screen, error) ||
        !game_eagle_test_frame(menu, error) ||
        !accordion_test_press(menu, owner, &point, error))
        return false;
    point.x = owner->handle_x + owner->width * 1.6f;
    if (!accordion_test_move(menu, point, error))
        return false;
    /* Keep the physical button held without motion: the source receives a
     * continuous hold each frame, rather than requiring desktop motion. */
    for (size_t frame = 0; frame < 178; ++frame)
        if (!game_eagle_test_frame(menu, error))
            return false;
    if (owner->state != ACCORDION_DRAGGING || owner->rip_timer >= 3 || stars)
        return ab_fail(error, "Accordion ripped before its three-second threshold");
    for (size_t frame = 0; frame < 8 && owner->state != ACCORDION_RIPPING; ++frame)
        if (!game_eagle_test_frame(menu, error))
            return false;
    menu->context.left_button_held = false;
    if (owner->state != ACCORDION_RIPPING || stars ||
        !owner->ripped.frame->fields.visible.boolean ||
        !ambient_capture(menu, renderer, "AB_ACCORDION_RIP_CAPTURE", error))
        return ab_fail(error, "Accordion did not run its real break sequence");
    for (size_t frame = 0; frame < 600 && !stars; ++frame)
        if (!game_eagle_test_frame(menu, error))
            return false;
    AbDataValue highscore =
        menu_route_field(ab_settings_statistics_root(menu->highscores), "ACCORDION");
    if (stars != 1 || !owner->base.star_collected ||
        !menu_route_truthy(menu_route_field(highscore.table, "completed")))
        return ab_fail(error, "Accordion did not persist its post-break star");
    for (size_t frame = 0; frame < 300 && menu->reward; ++frame)
        if (!game_eagle_test_frame(menu, error) ||
            (frame == 60 &&
             !ambient_capture(menu, renderer, "AB_ACCORDION_STAR_CAPTURE", error)))
            return false;
    if (stars != 1 || menu->reward ||
        !minigame_test_image_click(menu, &owner->base.back, error) || menu->minigame ||
        !menu->level_active || !menu->egg_levels ||
        ab_audio_mixer_view(menu->mixer).track_gains[5] !=
            ab_audio_mixer_view(menu->mixer).track_gains[7] ||
        ab_audio_mixer_view(menu->mixer).track_gains[6] !=
            ab_audio_mixer_view(menu->mixer).track_gains[7] ||
        !accordion_test_enter(menu, &owner, error) || starts != 2 ||
        owner->state != ACCORDION_IDLE || owner->ripped.frame->fields.visible.boolean ||
        !minigame_test_image_click(menu, &owner->base.back, error) ||
        !ab_event_manager_remove(menu->events, menu_event(menu, "EID_GE_LEVEL_STARTED"),
                                 &started, error) ||
        !ab_event_manager_remove(menu->events,
                                 menu_event(menu, "EID_GOLDEN_EGG_STAR_GAINED"),
                                 &collected, error))
        return ab_fail(error,
                       "Accordion star/Back/reentry or restored track gains failed");
    puts("Actual Golden Egg accordion: five stereo notes, extension/compression gains, "
         "pig toggle, stationary held input, release spring, mute, resize, timed "
         "ripping art/audio, one persisted star and Back/reentry.");
    return true;
}

#endif
