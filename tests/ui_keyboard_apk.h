#ifndef AB_UI_KEYBOARD_APK_H
#define AB_UI_KEYBOARD_APK_H

static bool keyboard_test_note(PreviewMenu *menu, KeyboardScene *owner,
                               const char *note, AbError *error) {
    KeyboardKey *key = keyboard_find(owner, menu_data(note));
    return key ? soundboard_test_click(menu, &key->widget, error)
               : ab_fail(error, "Missing authored keyboard fixture note");
}

static bool keyboard_widgets_test(PreviewMenu *menu, AbMetal *renderer,
                                  AbError *error) {
    menu->route_enabled = true;
    size_t starts = 0, stars = 0;
    AbEventListener started = {menu_span("KeyboardStartFixture"), &starts,
                               soundboard_test_event};
    AbEventListener collected = {menu_span("KeyboardStarFixture"), &stars,
                                 soundboard_test_event};
    if (!ab_event_manager_add(menu->events, menu_event(menu, "EID_GE_LEVEL_STARTED"),
                              &started, error) ||
        !ab_event_manager_add(menu->events,
                              menu_event(menu, "EID_GOLDEN_EGG_STAR_GAINED"),
                              &collected, error) ||
        !minigame_test_request(menu, "KEYBOARD", error))
        return false;
    for (size_t frame = 0; frame < 180 && (!menu_minigame_root(menu) || menu->loader ||
                                           menu_route_truthy(menu->skip_input));
         ++frame)
        if (!game_eagle_test_frame(menu, error))
            return false;
    KeyboardScene *owner = (KeyboardScene *)menu->minigame;
    AbDataValue played;
    if (!owner || !owner->base.active || menu->game || starts != 1 ||
        owner->base.widget.frame->child_count != 19 ||
        !ab_settings_golden_egg_played(menu->highscores, menu_span("KEYBOARD"), &played,
                                       error) ||
        !menu_route_truthy(played))
        return ab_fail(error, "Keyboard actual Loader/GUI did not complete");
    if (!ambient_capture(menu, renderer, "AB_KEYBOARD_CAPTURE", error) ||
        !keyboard_test_note(menu, owner, "E", error) || owner->note_count != 1 ||
        owner->autoplay || stars)
        return ab_fail(error, "Keyboard wrong-note sequence awarded a star");
    KeyboardKey *key = keyboard_find(owner, menu_data("C"));
    AbUiPointerReply reply;
    if (!key ||
        !menu_preview_pointer(menu, menu_span("LPRESS"), key->hit_box.center, &reply,
                              error) ||
        !menu_equal(reply.values[0], "PLAY_NOTE") ||
        !menu_preview_pointer(menu, menu_span("LHOLD"), key->hit_box.center, &reply,
                              error) ||
        reply.values[0].kind != AB_UI_POINTER_NONE ||
        !menu_preview_pointer(menu, menu_span("LHOLD"), (AbVec2){-100, -100}, &reply,
                              error) ||
        !menu_preview_pointer(menu, menu_span("LHOLD"), key->hit_box.center, &reply,
                              error) ||
        !menu_equal(reply.values[0], "PLAY_NOTE") ||
        !menu_preview_pointer(menu, menu_span("LRELEASE"), key->hit_box.center, &reply,
                              error))
        return ab_fail(error, "Keyboard swipe reentry repeated or lost a note");
    const char *const melody[] = {"C", "D", "Dis", "C", "G"};
    for (size_t index = 0; index < 5; ++index)
        if (!keyboard_test_note(menu, owner, melody[index], error))
            return false;
    if (!owner->autoplay || owner->play_position != 1 || stars ||
        keyboard_find(owner, menu_data("G"))->click_scale != .8f)
        return ab_fail(error, "Keyboard melody did not start its actual autoplay");
    /* Source emits at most one note per update, even with excess elapsed time. */
    float remaining = (owner->play_timer + 10) - .5f;
    if (!keyboard_update(owner, &owner->base.widget, 10, menu->time, error) ||
        owner->play_position != 2 || owner->play_timer != remaining)
        return ab_fail(error, "Keyboard autoplay consumed multiple notes in one call");
    for (size_t frame = 0; frame < 360; ++frame)
        if (!game_eagle_test_frame(menu, error) ||
            (frame == 60 &&
             !ambient_capture(menu, renderer, "AB_KEYBOARD_STAR_CAPTURE", error)))
            return false;
    AbDataValue highscore =
        menu_route_field(ab_settings_statistics_root(menu->highscores), "KEYBOARD");
    if (!owner->base.star_collected || owner->autoplay || stars != 1 || menu->reward ||
        !menu_route_truthy(menu_route_field(highscore.table, "completed")))
        return ab_fail(error, "Keyboard autoplay did not persist exactly one star");
    for (size_t index = 0; index < 5; ++index)
        if (!keyboard_test_note(menu, owner, melody[index], error))
            return false;
    if (owner->autoplay || stars != 1)
        return ab_fail(error, "Keyboard completed puzzle repeated its star sequence");
    bool enabled, changed;
    AbVec2 screen = menu->screen;
    if (!menu_audio_enabled(menu, &enabled, error) ||
        !soundboard_test_click(menu, &owner->base.audio, error) ||
        !menu_audio_enabled(menu, &changed, error) || enabled == changed ||
        !soundboard_test_click(menu, &owner->base.audio, error) ||
        !menu_preview_resize(menu, (AbVec2){1600, 900}, error) ||
        !game_eagle_test_frame(menu, error) ||
        !ambient_capture(menu, renderer, "AB_KEYBOARD_WIDE_CAPTURE", error) ||
        !menu_preview_resize(menu, screen, error) ||
        !soundboard_test_click(menu, &owner->base.back, error) || menu->minigame ||
        !menu->level_active || !menu->egg_levels ||
        !ab_event_manager_remove(menu->events, menu_event(menu, "EID_GE_LEVEL_STARTED"),
                                 &started, error) ||
        !ab_event_manager_remove(menu->events,
                                 menu_event(menu, "EID_GOLDEN_EGG_STAR_GAINED"),
                                 &collected, error))
        return ab_fail(error, "Keyboard sound/resize/back continuation failed");
    puts("Actual Golden Egg keyboard: pinned notes/song, hash-ordered shadows/keys, "
         "press/swipe, pulse, single-note autoplay, persisted star, audio, resize, "
         "Metal and Back.");
    return true;
}

#endif
