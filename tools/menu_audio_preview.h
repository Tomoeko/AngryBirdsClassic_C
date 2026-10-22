#ifndef AB_MENU_AUDIO_PREVIEW_H
#define AB_MENU_AUDIO_PREVIEW_H

#include "ab/classic_audio_ramp.h"

/* Desktop presentation policy requested for the unusually loud pack click.
 * ScrollFrame6059 traverses twice and EpisodeSelection3398 confirms again.
 * Keep those recovered handlers and independent gameplay mixer voices intact. */
static bool menu_audio_ui(void *context, AbUiPointerValue sound, float volume,
                          bool loop, AbError *error) {
    PreviewMenu *menu = context;
    bool confirmation = menu->pointer_audio_active && menu->episode_active && !loop &&
                        menu_equal(sound, "menu_confirm");
    if (confirmation && menu->pointer_confirm_played)
        return true;
    if (!menu->ui_audio ||
        !menu->ui_audio(menu->ui_audio_context, sound, volume, loop, error))
        return false;
    if (confirmation)
        menu->pointer_confirm_played = true;
    return true;
}

static void menu_audio_bind(PreviewMenu *menu) {
    menu->ui_audio = menu->context.play_audio;
    menu->ui_audio_context = menu->context.audio_context;
    menu->context.play_audio = menu_audio_ui;
    menu->context.audio_context = menu;
}

static bool menu_audio_enabled(PreviewMenu *menu, bool *output, AbError *error) {
    if ((!menu->local_settings && !menu_settings_open(menu, error)) ||
        !menu->local_settings)
        return false;
    const AbDataTable *root = ab_settings_statistics_root(menu->local_settings);
    AbDataValue enabled = menu_route_field(root, "audioEnabled");
    if (enabled.kind == AB_DATA_NIL) {
        AbSpan key = menu_span("audioEnabled");
        if (!ab_settings_statistics_write(
                menu->local_settings, &key, 1,
                (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1}, error))
            return false;
        enabled = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    }
    *output = menu_route_truthy(enabled);
    return true;
}

static bool menu_audio_indicator(PreviewMenu *menu, bool enabled, AbError *error) {
    for (size_t index = 0; index < 4; ++index) {
        AbUiToggleButton *button = menu->options.toggles + index;
        if (&button->widget == menu->options.controls[AB_UI_OPTION_SOUND])
            return ab_ui_toggle_button_set_state(button, enabled ? 1 : 2, error);
    }
    return ab_fail(error, "Missing actual main-menu sound toggle");
}

static bool menu_audio_initialize(PreviewMenu *menu, AbError *error) {
    bool enabled = false;
    return menu_audio_enabled(menu, &enabled, error) &&
           ab_classic_set_effects_volume(menu->mixer, enabled ? 1 : 0, error) &&
           ab_classic_set_music_volume(menu->mixer, enabled ? 1 : 0, error) &&
           menu_audio_indicator(menu, enabled, error);
}

/* changeAudio92979's Android branch, from MainMenu's actual pointer action.
 * Native output continues to drain while track gains are muted. */
static bool menu_audio_toggle(PreviewMenu *menu, AbError *error) {
    bool enabled = false;
    if (!menu_audio_enabled(menu, &enabled, error))
        return false;
    enabled = !enabled;
    AbSpan key = menu_span("audioEnabled");
    if (!ab_settings_statistics_write(
            menu->local_settings, &key, 1,
            (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = enabled}, error) ||
        !ab_classic_set_effects_volume(menu->mixer, enabled ? 1 : 0, error))
        return false;
    /* Synchronous notification; its selected application listeners do not
     * retain or queue this source payload. */
    AbDataEntry fields[] = {
        {menu_data("id"), menu_event(menu, "EID_AUDIO_STATE_CHANGED")},
        {menu_data("state"), menu_route_number(enabled ? 1 : 0)},
        {menu_data("from"), menu_data("mainMenu")}};
    AbDataTable notice = {fields, 3, 3, NULL};
    return ab_event_manager_notify(menu->events, &notice, error) &&
           ab_classic_set_music_volume(menu->mixer, enabled ? 1 : 0, error) &&
           menu_audio_indicator(menu, enabled, error) &&
           menu_preview_flush(menu, error);
}

/* MainMenuRoot.update7373 words44..61. Direct resource playback avoids an
 * additional script audio-name selection/RNG draw. */
static bool menu_audio_music(PreviewMenu *menu, AbError *error) {
    if (menu->main_song.kind == AB_DATA_NIL)
        return true;
    if (menu->main_song.kind != AB_DATA_STRING)
        return ab_fail(error, "Unsupported selected main-menu song");
    bool playing = false;
    if (!ab_audio_bank_is_playing(menu->audio, menu->main_song.string, &playing, error))
        return false;
    uint32_t handle;
    return playing || ab_audio_bank_play_resource(menu->audio, menu->main_song.string,
                                                  0.8f, true, 7, &handle, error);
}

#endif
