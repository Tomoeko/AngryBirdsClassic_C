#ifndef AB_GAME_PAUSE_H
#define AB_GAME_PAUSE_H

#include "ab/ui_pause_page.h"
#include "ab/classic_pause_actions.h"

struct PreviewPause {
    PreviewGame *game;
    AbUiPausePage page;
    AbUiHudPauseButton button;
    AbUiPausePageEnvironment source;
    AbClassicPauseActions actions;
    AbUiPauseNotice notices[512];
    AbClassicPauseNotice action_notices[128];
    AbEventTimerEntry timers[8];
    AbClassicAudioRampState ramp;
    AbDataValue restarted_from, stored_zoom;
    AbVec2 margin;
    ResultTextClip label;
    AbFontAsset font;
    AbSpan font_name;
    char level_label[64];
    bool font_loaded, entered;
};

static bool game_pause_audio(void *context, bool *output, AbError *error) {
    PreviewPause *pause = context;
    return menu_audio_enabled(pause->game->menu, output, error);
}

static bool game_pause_label(void *context, AbUiPointerValue *output, AbError *error) {
    PreviewPause *pause = context;
    AbClassicLevelRequest *request = &pause->game->menu->level_request;
    int count = snprintf(pause->level_label, sizeof(pause->level_label), "%g-%g",
                         (double)menu_real(request->world_number.bits),
                         (double)menu_real(request->level_index.bits));
    if (count < 1 || (size_t)count >= sizeof(pause->level_label))
        return ab_fail(error, "Pause level label exceeds its storage");
    *output = menu_text(pause->level_label);
    return true;
}

static bool game_pause_show_service(void *context, AbError *error) {
    return ab_classic_pause_show(&((PreviewPause *)context)->actions, error);
}

static bool game_pause_scale_service(void *context, bool enabled, AbError *error) {
    return ab_classic_pause_scaling(&((PreviewPause *)context)->actions, enabled,
                                    error);
}

static bool game_pause_font(PreviewPause *pause, AbError *error) {
    PreviewMenu *menu = pause->game->menu;
    if (!ab_text_resources_get(&menu->text_resources, menu_span("TEXTS_BASIC"),
                               menu_span("FONT_MENU"), &pause->font_name, error))
        return false;
    for (size_t index = 0; index < menu->fonts.count; ++index)
        if (menu->font_bindings[index].name.size == pause->font_name.size &&
            !memcmp(menu->font_bindings[index].name.data, pause->font_name.data,
                    pause->font_name.size))
            return true;
    if (!pause->font_name.size || pause->font_name.size > 64 || menu->fonts.count == 6)
        return ab_fail(error, "Pause font exceeds its binding storage");
    for (size_t index = 0; index < pause->font_name.size; ++index) {
        uint8_t byte = pause->font_name.data[index];
        if (!((byte >= 'A' && byte <= 'Z') || (byte >= '0' && byte <= '9') ||
              byte == '_'))
            return ab_fail(error, "Unsupported pause font identifier");
    }
    char path[128];
    int count =
        snprintf(path, sizeof(path), "assets/data/fonts/%s/%.*s.dat",
                 menu->highest_resources ? "1024x768" : "480x320",
                 (int)pause->font_name.size, (const char *)pause->font_name.data);
    if (count < 1 || (size_t)count >= sizeof(path) ||
        !ab_font_asset_open(menu->archive, path, 8 * 1024 * 1024, 2 * 1024 * 1024,
                            &pause->font, error))
        return false;
    if (!menu->texture_host.upload(
            menu->texture_host.context, 66,
            (AbSpriteAtlasImage){.image = &pause->font.image,
                                 .alpha = pause->font.has_alpha
                                              ? AB_SPRITE_ALPHA_PRESENT
                                              : AB_SPRITE_ALPHA_OPAQUE},
            error)) {
        ab_font_asset_close(&pause->font);
        return false;
    }
    pause->font_loaded = true;
    menu->font_bindings[menu->fonts.count++] =
        (AbUiFontBinding){pause->font_name, &pause->font.font, 66,
                          pause->font.image.width, pause->font.image.height};
    return true;
}

static void game_pause_free(PreviewGame *game) {
    PreviewPause *pause = game->pause;
    if (!pause)
        return;
    PreviewMenu *menu = game->menu;
    AbError error = {0};
    if (pause->entered && !menu->context.failed)
        (void)ab_ui_widget_lifecycle(&pause->page.widget, AB_UI_FRAME_EXIT, &error);
    (void)ab_ui_pause_page_detach(&pause->page, &error);
    if (pause->font_loaded) {
        menu->fonts.current = NULL;
        for (size_t index = 0; index < menu->fonts.count; ++index)
            if (menu->font_bindings[index].font == &pause->font.font) {
                memmove(menu->font_bindings + index, menu->font_bindings + index + 1,
                        (menu->fonts.count - index - 1) * sizeof(*menu->font_bindings));
                --menu->fonts.count;
                break;
            }
        (void)menu->texture_host.remove(menu->texture_host.context, 66, &error);
        ab_font_asset_close(&pause->font);
    }
    free(pause);
    game->pause = NULL;
}

static bool game_pause_layout(PreviewGame *game, AbError *error) {
    if (!game->pause)
        return true;
    PreviewPause *pause = game->pause;
    AbUiWidget *button = &pause->button.widget;
    float scale;
    if (!ab_ui_image_state_refresh(&button->state.image, &game->menu->context.images,
                                   error) ||
        !ab_ui_scaling_get(&game->menu->scaling, &scale, error))
        return false;
    button->frame->fields.x = menu_number(
        menu_real(button->state.image.resource_pivot_x.number_bits) * 1.1f * scale);
    button->frame->fields.y = menu_number(
        menu_real(button->state.image.resource_pivot_y.number_bits) * 1.1f * scale);
    /* Retire old clipped label generations only at this quiescent app layout. */
    pause->page.number_clipping = (AbUiTextClipping){0};
    return ab_ui_text_clipping_init(&pause->page.number_clipping,
                                    pause->page.host.label_storage, error) &&
           ab_ui_scaling_set_to(&game->menu->scaling, &button->frame->fields, error) &&
           ab_ui_widget_lifecycle(&pause->page.widget, AB_UI_FRAME_LAYOUT, error);
}

static bool game_pause_open(PreviewGame *game, AbError *error) {
    PreviewMenu *menu = game->menu;
    PreviewPause *pause = calloc(1, sizeof(*pause));
    if (!pause)
        return ab_fail(error, "Cannot allocate application pause controls");
    game->pause = pause;
    pause->game = game;
    pause->source = (AbUiPausePageEnvironment){.powerups_enabled = true,
                                               .use_scaling = true,
                                               .width_ratio = 0.2f,
                                               .device_model = menu_span("android")};
    if (!game_pause_font(pause, error))
        return false;
    pause->source.menu_font =
        (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = pause->font_name};
    if (!ab_classic_pause_actions_init(
            &pause->actions,
            (AbClassicPauseActionConfig){.audio = menu->audio,
                                         .events = menu->events,
                                         .event_ids = menu->event_data.data.globals,
                                         .restarted_from = &pause->restarted_from,
                                         .zoom_level = &game->draw.zoom_level,
                                         .sotred_zoom_level = &pause->stored_zoom,
                                         .notices = pause->action_notices,
                                         .notice_capacity = 128},
            error))
        return false;
    AbUiPausePageHost host = {
        .scaling = &menu->scaling,
        .fonts = &menu->fonts,
        .events = menu->events,
        .event_ids = menu->event_data.data.globals,
        .screen = &game->screen,
        .margin = &pause->margin,
        .environment = &pause->source,
        .time = &game->globals.time,
        .tutorial_active = &game->absent,
        .current_music = &game->music,
        .skip_input = &game->input.skip_input,
        .mixer = menu->mixer,
        .audio = menu->audio,
        .ramp = &pause->ramp,
        .solid_texture = 63,
        .notices = pause->notices,
        .notice_capacity = 512,
        .timers = pause->timers,
        .timer_capacity = 8,
        .label_storage = {pause->label.lines, pause->label.names, 8,
                          pause->label.wrapped, pause->label.text, 8,
                          pause->label.codes, 256, pause->label.bytes, 2048},
        .context = pause,
        .audio_enabled = game_pause_audio,
        .level_display = game_pause_label,
        .show_pause_menu = game_pause_show_service,
        .scaling_enabled = game_pause_scale_service};
    AbUiImageButtonArguments button = {.image.frame.name = menu_text("pauseButton"),
                                       .return_value = menu_text("PAUSE"),
                                       .activate_on_release = result_boolean(true)};
    if (!ab_ui_pause_page_init(&pause->page, &menu->context, host, error) ||
        !ab_ui_hud_pause_button_init(&pause->button, &menu->context,
                                     (AbUiWidgetChildren){0}, &button, false, NULL,
                                     error) ||
        !ab_ui_image_button_state_set_image(
            &pause->button.widget.state, menu_text("INGAME_BUTTON_PAUSE"),
            (AbUiPointerValue){0}, &menu->context.images, error) ||
        !ab_ui_widget_lifecycle(&pause->page.widget, AB_UI_FRAME_ENTRY, error))
        return false;
    pause->entered = true;
    return game_pause_layout(game, error);
}

static bool game_pause_toggle(PreviewGame *game, AbError *error) {
    if (!game || game->ended || !game->pause)
        return true;
    PreviewPause *pause = game->pause;
    if (!ab_classic_sling_cancel_drag(game->playfield.sling, error))
        return false;
    game->held_key.value.bits = game->pressed_key.value.bits =
        game->released_key.value.bits = 0;
    game->input.touch_count = menu_route_number(0);
    if (game->paused) {
        if (!ab_ui_pause_page_hide(&pause->page, error))
            return false;
        game->paused = false;
        game->physics = true;
    } else {
        game->physics = false;
        game->paused = true;
        if (!ab_ui_pause_page_show(&pause->page, error))
            return false;
    }
    return true;
}

static bool game_session_escape(PreviewMenu *menu, AbError *error) {
    if (!menu->game)
        return true;
    if (menu->game->results) {
        menu->game->results->pending_action = menu_text("RETURN_TO_LEVEL_SELECTION");
        return true;
    }
    return game_pause_toggle(menu->game, error);
}

static bool game_pause_pointer(PreviewGame *game, AbSpan action, AbVec2 point,
                               bool *consumed, AbError *error) {
    *consumed = false;
    if (!game->pause || game->ended)
        return true;
    PreviewPause *pause = game->pause;
    AbUiPointerReply reply;
    bool visible =
        ab_ui_pointer_value_truthy(&pause->page.widget.frame->fields.visible);
    if (!ab_ui_widget_pointer(visible ? &pause->page.widget : &pause->button.widget,
                              action, point, &reply, error))
        return false;
    *consumed = visible || menu_equal(reply.values[0], "PAUSE") ||
                menu_equal(reply.values[0], "CLICK") ||
                menu_equal(reply.values[0], "HOVER");
    if (menu_equal(reply.values[0], "PAUSE") ||
        menu_equal(reply.values[0], "BACK_TO_GAME"))
        return game_pause_toggle(game, error);
    if (menu_equal(reply.values[0], "RESTART_LEVEL"))
        game->navigation_action = menu_text("RESTART_LEVEL");
    else if (menu_equal(reply.values[0], "BACK_TO_LEVEL_SELECTION"))
        game->navigation_action = menu_text("RETURN_TO_LEVEL_SELECTION");
    else if (menu_equal(reply.values[0], "TOGGLE_SOUNDS"))
        return menu_audio_toggle(game->menu, error) &&
               ab_ui_pause_page_audio_state(&pause->page, error);
    else if (menu_equal(reply.values[0], "SHOW_TUTORIALS") ||
             menu_equal(reply.values[0], "GOTO_SHOP_FROM_PAUSE"))
        return ab_fail(error, "Selected pause action requires its enclosing scene");
    return true;
}

static bool game_pause_draw(PreviewGame *game, float seconds, AbError *error) {
    if (!game->pause || game->ended)
        return true;
    PreviewPause *pause = game->pause;
    AbUiDrawInput input = {0};
    if (!ab_ui_widget_update(&pause->page.widget, seconds, game->menu->time, error))
        return false;
    return (game->paused || ab_ui_widget_draw(&pause->button.widget, &input, error)) &&
           ab_ui_widget_draw(&pause->page.widget, &input, error);
}

#endif
