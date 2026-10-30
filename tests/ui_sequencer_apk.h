#ifndef AB_UI_SEQUENCER_APK_H
#define AB_UI_SEQUENCER_APK_H

static bool sequencer_test_drag(PreviewMenu *menu, SequencerScene *owner,
                                AbError *error) {
    AbVec2 point;
    AbUiPointerReply reply;
    if (!ab_ui_widget_screen_position(&owner->bird, &point, error) ||
        !menu_preview_pointer(menu, menu_span("LPRESS"), point, &reply, error))
        return false;
    float step = menu->tap_radius + 1;
    for (size_t pass = 0; pass < 128 && owner->value < 1; ++pass) {
        point.x = fminf(point.x + step, owner->maximum_x);
        if (!menu_preview_pointer(menu, menu_span("LHOLD"), point, &reply, error) ||
            !game_eagle_test_frame(menu, error) || owner->drag != SEQUENCER_DRAGGING)
            return ab_fail(error, "Sequencer real tempo drag did not move the bird");
    }
    if (!menu_preview_pointer(menu, menu_span("LRELEASE"), point, &reply, error) ||
        owner->drag != SEQUENCER_IDLE || owner->value != 1 || owner->playing)
        return ab_fail(error, "Sequencer drag release toggled play or lost its limit");
    return true;
}

static bool sequencer_widgets_test(PreviewMenu *menu, AbMetal *renderer,
                                   AbError *error) {
    menu->route_enabled = true;
    size_t starts = 0, stars = 0;
    AbEventListener started = {menu_span("SequencerStartFixture"), &starts,
                               soundboard_test_event};
    AbEventListener collected = {menu_span("SequencerStarFixture"), &stars,
                                 soundboard_test_event};
    if (!ab_event_manager_add(menu->events, menu_event(menu, "EID_GE_LEVEL_STARTED"),
                              &started, error) ||
        !ab_event_manager_add(menu->events,
                              menu_event(menu, "EID_GOLDEN_EGG_STAR_GAINED"),
                              &collected, error) ||
        !minigame_test_request(menu, "SEQUENCER", error))
        return false;
    for (size_t frame = 0; frame < 180 && (!menu_minigame_root(menu) || menu->loader ||
                                           menu_route_truthy(menu->skip_input));
         ++frame)
        if (!game_eagle_test_frame(menu, error))
            return false;
    SequencerScene *owner = (SequencerScene *)menu->minigame;
    AbDataValue played;
    if (!owner || !owner->base.active || menu->game || starts != 1 ||
        owner->base.widget.frame->child_count != 33 ||
        !ab_settings_golden_egg_played(menu->highscores, menu_span("SEQUENCER"),
                                       &played, error) ||
        !menu_route_truthy(played) || sequencer_all_pigs(owner))
        return ab_fail(error, "Sequencer actual Loader/GUI did not complete");
    if (!ambient_capture(menu, renderer, "AB_SEQUENCER_CAPTURE", error) ||
        !soundboard_test_click(menu, &owner->pigs[0][0].button.widget, error) ||
        menu_real(owner->pigs[0][0].button.state.number_bits) != 2 ||
        menu_real(owner->pigs[0][0].button.widget.frame->fields.scale_x.number_bits) !=
            owner->pigs[0][0].scales[1])
        return ab_fail(error, "Sequencer actual pig toggle/scale failed");
    AbVec2 point;
    AbUiPointerReply reply;
    if (!ab_ui_widget_screen_position(&owner->bird, &point, error) ||
        !menu_preview_pointer(menu, menu_span("LPRESS"), point, &reply, error))
        return false;
    point.x += menu->tap_radius;
    if (!menu_preview_pointer(menu, menu_span("LHOLD"), point, &reply, error) ||
        owner->drag != SEQUENCER_CLICK ||
        !menu_preview_pointer(menu, menu_span("LRELEASE"), point, &reply, error) ||
        !owner->playing || owner->play_position || owner->play_delay != 1)
        return ab_fail(error, "Sequencer tap-radius equality did not remain a click");
    for (size_t frame = 0; frame < 90; ++frame)
        if (!game_eagle_test_frame(menu, error))
            return false;
    if (!owner->playing || !owner->play_position || stars ||
        !owner->indicator.frame->fields.visible.boolean ||
        !soundboard_test_click(menu, &owner->bird, error) || owner->playing ||
        owner->indicator.frame->fields.visible.boolean)
        return ab_fail(error, "Sequencer sparse rhythm or stop state failed");
    for (size_t row = 0; row < 3; ++row)
        for (size_t column = 0; column < 8; ++column)
            if ((row || column) &&
                !soundboard_test_click(menu, &owner->pigs[row][column].button.widget,
                                       error))
                return false;
    if (!sequencer_all_pigs(owner) || !sequencer_test_drag(menu, owner, error) ||
        !soundboard_test_click(menu, &owner->bird, error) || !owner->playing || stars)
        return ab_fail(error, "Sequencer complete rhythm/tempo setup failed");
    for (size_t frame = 0; frame < 420; ++frame)
        if (!game_eagle_test_frame(menu, error) ||
            (frame == 120 &&
             !ambient_capture(menu, renderer, "AB_SEQUENCER_STAR_CAPTURE", error)))
            return false;
    AbDataValue highscore =
        menu_route_field(ab_settings_statistics_root(menu->highscores), "SEQUENCER");
    if (!owner->base.star_collected || owner->playing || stars != 1 || menu->reward ||
        !menu_route_truthy(menu_route_field(highscore.table, "completed")))
        return ab_fail(error, "Sequencer tempo puzzle did not save exactly one star");
    if (!soundboard_test_click(menu, &owner->bird, error))
        return false;
    for (size_t frame = 0; frame < 120; ++frame)
        if (!game_eagle_test_frame(menu, error))
            return false;
    if (!owner->playing || stars != 1 ||
        !soundboard_test_click(menu, &owner->bird, error) || owner->playing)
        return ab_fail(error, "Sequencer completed puzzle could not resume its rhythm");
    bool enabled, changed;
    AbVec2 screen = menu->screen;
    if (!menu_audio_enabled(menu, &enabled, error) ||
        !soundboard_test_click(menu, &owner->base.audio, error) ||
        !menu_audio_enabled(menu, &changed, error) || enabled == changed ||
        !soundboard_test_click(menu, &owner->base.audio, error) ||
        !menu_preview_resize(menu, (AbVec2){1600, 900}, error) ||
        !game_eagle_test_frame(menu, error) ||
        !ambient_capture(menu, renderer, "AB_SEQUENCER_WIDE_CAPTURE", error) ||
        !menu_preview_resize(menu, screen, error) ||
        !soundboard_test_click(menu, &owner->base.back, error) || menu->minigame ||
        !menu->level_active || !menu->egg_levels ||
        !ab_event_manager_remove(menu->events, menu_event(menu, "EID_GE_LEVEL_STARTED"),
                                 &started, error) ||
        !ab_event_manager_remove(menu->events,
                                 menu_event(menu, "EID_GOLDEN_EGG_STAR_GAINED"),
                                 &collected, error))
        return ab_fail(error, "Sequencer sound/resize/back continuation failed");
    puts("Actual Golden Egg sequencer: real Loader/old-pig assets, tiled artwork, "
         "24 toggles, tempo tap/drag, rhythm, one saved star, sound, resize, Metal "
         "and Back.");
    return true;
}

#endif
