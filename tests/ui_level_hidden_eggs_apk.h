#ifndef AB_UI_LEVEL_HIDDEN_EGGS_APK_TEST_H
#define AB_UI_LEVEL_HIDDEN_EGGS_APK_TEST_H

static bool hidden_egg_reward_frames(PreviewMenu *menu, AbMetal *renderer,
                                     AbError *error) {
    bool captured = false;
    int16_t pcm[512];
    for (size_t frame = 0; frame < 240; ++frame) {
        if (!menu_preview_draw(menu, 1.0f / 60, error) ||
            !ab_audio_mixer_mix(menu->mixer, 267, pcm, 512, error))
            return false;
        if (frame == 90) {
            if (!menu->reward || menu->reward->rate != 1 ||
                menu_real(menu->reward->glow.frame->fields.y.number_bits) !=
                    menu->screen.y * 0.5f)
                return ab_fail(error, "Golden-egg reward missed its middle phase");
            AbImage image = {0};
            float clear[4] = {0, 0, 0, 1};
            captured = ab_metal_capture(
                renderer, menu->context.draw_list, (uint32_t)menu->screen.x,
                (uint32_t)menu->screen.y, clear, 16 * 1024 * 1024, &image, error);
            ab_image_free(&image);
            if (!captured)
                return false;
        }
    }
    return captured && !menu->reward
               ? true
               : ab_fail(error, "Golden-egg notification did not finish its process");
}

/* Select the real source subclass at the application factory boundary. The
 * extreme scroll is controlled fixture input, not a native gesture trace. */
static bool hidden_egg_widgets(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    const unsigned episodes[] = {3, 6};
    const char *const levels[] = {"LevelGE_7", "LevelGE_1"};
    for (size_t index = 0; index < 2; ++index) {
        menu->episodes->selected_episode = menu_route_number((float)episodes[index]);
        if (!menu_route_level(menu, error) || !menu->hidden_eggs ||
            !menu_preview_draw(menu, 0, error))
            return false;
        AbUiLevelHiddenEggs *hidden = menu->hidden_eggs;
        AbUiWidget *egg = &hidden->egg;
        if (!hidden->present ||
            !ab_ui_pointer_value_truthy(&egg->frame->fields.visible) ||
            !(menu_real(egg->frame->fields.w.number_bits) > 0))
            return ab_fail(error, "Selected subclass omitted its actual hidden egg");
        if (index == 0) {
            AbUiScrollFrame *scroll = &menu->levels->levels.scroll;
            if (!ab_ui_scroll_frame_set_current(scroll, menu->levels->levels.page_count,
                                                error) ||
                !ab_ui_scroll_frame_begin_drag(scroll, (AbVec2){500, 300}, error))
                return false;
            for (size_t step = 1; step <= 8; ++step)
                if (!ab_ui_scroll_frame_update_drag(
                        scroll, (AbVec2){500 - 200 * (float)step, 300}, error))
                    return false;
            if (!menu_preview_draw(menu, 0, error))
                return false;
            if (hidden->present)
                return ab_fail(error, "Danger Above did not discover its scrolled egg");
        } else {
            AbVec2 point;
            AbUiPointerReply reply;
            menu->context.left_button_held = true;
            menu->context.touch_count = 1;
            if (!game_test_center(egg, &point, error) ||
                !menu_preview_pointer(menu, menu_span("LPRESS"), point, &reply, error))
                return false;
            menu->context.left_button_held = false;
            menu->context.touch_count = 0;
            if (!menu_preview_pointer(menu, menu_span("LRELEASE"), point, &reply,
                                      error))
                return false;
            if (!hidden->fading)
                return ab_fail(error,
                               "Mine and Dine's hidden egg missed at %g,%g (%.*s)",
                               point.x, point.y,
                               reply.values[0].kind == AB_UI_POINTER_TEXT
                                   ? (int)reply.values[0].text.size
                                   : 0,
                               reply.values[0].kind == AB_UI_POINTER_TEXT
                                   ? (const char *)reply.values[0].text.data
                                   : "");
        }
        AbDataValue unlocked;
        if (!menu->reward)
            return ab_fail(error, "Episode %u did not construct its discovered reward",
                           episodes[index]);
        if (!ab_settings_golden_egg_unlocked(menu->highscores, menu_span(levels[index]),
                                             &unlocked, error))
            return false;
        if (!menu_route_truthy(unlocked))
            return ab_fail(error, "Episode %u did not unlock its discovered egg",
                           episodes[index]);
        if (!hidden_egg_reward_frames(menu, renderer, error))
            return false;
        if (index == 1 && menu_real(egg->frame->fields.scale_x.number_bits) != 0)
            return ab_fail(error, "Mine and Dine's found egg did not shrink away");
        if (!ab_ui_widget_lifecycle(&menu->levels->widget, AB_UI_FRAME_EXIT, error) ||
            !ab_ui_widget_lifecycle(&menu->levels->widget, AB_UI_FRAME_ENTRY, error) ||
            !ab_ui_widget_lifecycle(&menu->levels->widget, AB_UI_FRAME_LAYOUT, error))
            return false;
        if (hidden->present &&
            ab_ui_pointer_value_truthy(&hidden->egg.frame->fields.visible))
            return ab_fail(error, "Found golden egg became visible on reentry");
    }
    puts("Danger Above and Mine and Dine: actual screen subclasses, hidden eggs, "
         "unlock storage, reward Metal frames and reentry visibility.");
    return true;
}

#endif
