#ifndef AB_UI_RADIO_APK_H
#define AB_UI_RADIO_APK_H

static bool radio_test_enter(PreviewMenu *menu, RadioScene **output, AbError *error) {
    if (!minigame_test_request(menu, "RADIO", error))
        return false;
    PreviewMinigame *pending = menu->minigame;
    if (!pending || pending->active || !menu_minigame_leave(menu, error) ||
        menu->minigame != pending)
        return ab_fail(error, "Escape during radio loading lost its pending owner");
    for (size_t frame = 0; frame < 180 && (!menu_minigame_root(menu) || menu->loader ||
                                           menu_route_truthy(menu->skip_input));
         ++frame)
        if (!game_eagle_test_frame(menu, error))
            return false;
    *output = (RadioScene *)menu->minigame;
    return *output && (*output)->base.active && !menu->game &&
           (*output)->base.widget.frame->child_count == 12;
}

static bool radio_test_select(PreviewMenu *menu, RadioScene *owner, size_t button,
                              AbError *error) {
    if (!button || button > 4 ||
        !minigame_test_image_click(menu, &owner->buttons[button - 1].widget, error))
        return false;
    for (size_t frame = 0; frame < 90; ++frame)
        if (!game_eagle_test_frame(menu, error))
            return false;
    size_t voices = 0;
    AbDataValue song;
    bool playing;
    if (!owner->current_present || owner->current_channel != (float)button ||
        owner->position != (float)button || owner->state != RADIO_IDLE)
        return ab_fail(error,
                       "Radio button %zu selected channel %g at position %g/state %d",
                       button, (double)owner->current_channel, (double)owner->position,
                       (int)owner->state);
    if (!radio_song(owner, (float)button, &song, error) ||
        !ab_audio_bank_is_playing(menu->audio, song.string, &playing, error) ||
        !ab_audio_mixer_track_count(menu->mixer, 7, &voices, error))
        return false;
    if (!playing || voices != 1)
        return ab_fail(error, "Radio button %zu music playing=%d, track voices=%zu",
                       button, (int)playing, voices);
    return true;
}

static bool radio_test_wheel(PreviewMenu *menu, RadioScene *owner, AbError *error) {
    AbVec2 center;
    float width = 0;
    if (!radio_wheel_geometry(owner, &center, &width, error))
        return false;
    AbVec2 offset;
    if (!ab_script_polar(-radio_pi * .5f, width * 1.5f, &offset))
        return ab_fail(error, "Radio wheel fixture exceeded its angle domain");
    AbVec2 point = {center.x + offset.x, center.y + offset.y};
    AbUiPointerReply reply;
    menu->context.left_button_held = true;
    if (!menu_preview_pointer(menu, menu_span("LPRESS"), point, &reply, error) ||
        owner->state != RADIO_DRAGGING)
        return ab_fail(error, "Real radio wheel press did not select its annulus");
    for (size_t step = 1; step <= 18; ++step) {
        float angle = -radio_pi * .5f + (float)step * (radio_pi / 18);
        if (!ab_script_polar(angle, width * 1.5f, &offset))
            return false;
        point = (AbVec2){center.x + offset.x, center.y + offset.y};
        if (!menu_preview_pointer(menu, menu_span("LHOLD"), point, &reply, error) ||
            !game_eagle_test_frame(menu, error) || owner->state != RADIO_DRAGGING)
            return ab_fail(error, "Real radio wheel drag lost its selected annulus");
    }
    menu->context.left_button_held = false;
    if (!menu_preview_pointer(menu, menu_span("LRELEASE"), point, &reply, error) ||
        owner->target != 3.5f)
        return ab_fail(error, "Real radio wheel release selected the wrong station");
    return true;
}

static bool radio_widgets_test(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    menu->route_enabled = true;
    size_t starts = 0, stars = 0;
    AbEventListener started = {menu_span("RadioStartFixture"), &starts,
                               soundboard_test_event};
    AbEventListener collected = {menu_span("RadioStarFixture"), &stars,
                                 soundboard_test_event};
    RadioScene *owner = NULL;
    if (!ab_event_manager_add(menu->events, menu_event(menu, "EID_GE_LEVEL_STARTED"),
                              &started, error) ||
        !ab_event_manager_add(menu->events,
                              menu_event(menu, "EID_GOLDEN_EGG_STAR_GAINED"),
                              &collected, error) ||
        !radio_test_enter(menu, &owner, error))
        return false;
    if (starts != 1 || owner->current_present || owner->channel_present ||
        !ambient_capture(menu, renderer, "AB_RADIO_CAPTURE", error))
        return ab_fail(error, "Radio did not enter its original untuned state");
    const float ties[][2] = {{1.5f, 2}, {2.5f, 2}, {3.25f, 3}, {3.75f, 4}};
    for (size_t index = 0; index < 4; ++index) {
        RadioScene controlled = {.position = ties[index][0]};
        radio_release(&controlled);
        if (controlled.target != ties[index][1])
            return ab_fail(error,
                           "Radio nearest-station tie differs from native traversal");
        if (!radio_test_select(menu, owner, index + 1, error) || stars)
            return false;
    }
    if (!radio_test_select(menu, owner, 3, error) ||
        !radio_test_wheel(menu, owner, error))
        return false;
    for (size_t frame = 0; frame < 360; ++frame)
        if (!game_eagle_test_frame(menu, error) ||
            (frame == 60 &&
             !ambient_capture(menu, renderer, "AB_RADIO_STAR_CAPTURE", error)))
            return false;
    AbDataValue highscore =
        menu_route_field(ab_settings_statistics_root(menu->highscores), "RADIO");
    if (!owner->base.star_collected || owner->current_channel != 3.5f ||
        !owner->bird_active || stars != 1 || menu->reward ||
        !menu_route_truthy(menu_route_field(highscore.table, "completed")))
        return ab_fail(error, "Radio secret station did not persist exactly one star");
    if (!radio_test_select(menu, owner, 1, error))
        return false;
    AbUiPointerReply reply;
    for (size_t button = 2; button < 4; ++button) {
        AbVec2 point;
        if (!minigame_test_image_center(&owner->buttons[button].widget, &point,
                                        error) ||
            !menu_preview_pointer(menu, menu_span("LRELEASE"), point, &reply, error))
            return false;
    }
    for (size_t frame = 0; frame < 90; ++frame)
        if (!game_eagle_test_frame(menu, error))
            return false;
    if (owner->current_channel != 3.5f || stars != 1 || menu->reward ||
        !owner->buttons[2].down || !owner->buttons[3].down ||
        !radio_test_select(menu, owner, 1, error))
        return ab_fail(error,
                       "Radio simultaneous buttons or repeated star state failed");
    bool enabled, changed;
    AbVec2 screen = menu->screen;
    AbDataValue title = owner->menu_song;
    if (!menu_audio_enabled(menu, &enabled, error) ||
        !minigame_test_image_click(menu, &owner->base.audio, error) ||
        !menu_audio_enabled(menu, &changed, error) || enabled == changed ||
        !minigame_test_image_click(menu, &owner->base.audio, error) ||
        !menu_preview_resize(menu, (AbVec2){1600, 900}, error) ||
        !game_eagle_test_frame(menu, error) ||
        !ambient_capture(menu, renderer, "AB_RADIO_WIDE_CAPTURE", error) ||
        !menu_preview_resize(menu, screen, error) ||
        !minigame_test_image_click(menu, &owner->base.back, error) || menu->minigame ||
        !menu->level_active || !menu->egg_levels)
        return ab_fail(error, "Radio sound/resize/back continuation failed");
    bool playing;
    if (!ab_audio_bank_is_playing(menu->audio, title.string, &playing, error) ||
        !playing || !radio_test_enter(menu, &owner, error) || starts != 2 ||
        !radio_test_select(menu, owner, 2, error) ||
        !minigame_test_image_click(menu, &owner->base.back, error) || menu->minigame ||
        !ab_audio_bank_is_playing(menu->audio, menu_span("ambient_theme1"), &playing,
                                  error) ||
        playing ||
        !ab_event_manager_remove(menu->events, menu_event(menu, "EID_GE_LEVEL_STARTED"),
                                 &started, error) ||
        !ab_event_manager_remove(menu->events,
                                 menu_event(menu, "EID_GOLDEN_EGG_STAR_GAINED"),
                                 &collected, error))
        return ab_fail(error,
                       "Radio exit lost its actual station-specific audio lifetime");
    puts("Actual Golden Egg radio: pinned layout, native numeric-key ties, annulus "
         "drag, four music stations, simultaneous buttons, secret star/bird motion, "
         "sound, resize, Metal, loading Escape and station-specific exit/reentry.");
    return true;
}

#endif
