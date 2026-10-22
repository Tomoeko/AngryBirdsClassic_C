#ifndef AB_UI_MENU_AUDIO_APK_TEST_H
#define AB_UI_MENU_AUDIO_APK_TEST_H

static bool menu_test_music(PreviewMenu *menu, uint32_t expected_voice, uint32_t *voice,
                            AbError *error) {
    AbAudioBankClip clip;
    if (menu->main_song.kind != AB_DATA_STRING ||
        !ab_audio_bank_clip(menu->audio, menu->main_song.string, &clip))
        return ab_fail(error, "Menu music has no actual decoded registration");
    AbAudioMixerView mixer = ab_audio_mixer_view(menu->mixer);
    size_t loops = 0;
    for (size_t index = 0; index < mixer.active_count; ++index) {
        const AbAudioVoice *current = mixer.active + index;
        if (current->track != 7 || !(current->flags & AB_AUDIO_VOICE_LOOP) ||
            (current->flags & AB_AUDIO_VOICE_ENDED))
            continue;
        ++loops;
        if (current->clip_id != clip.id || current->gain != 0.8f ||
            (expected_voice != UINT32_MAX && current->id != expected_voice))
            return ab_fail(error, "Menu navigation changed its music voice/gain");
        *voice = current->id;
    }
    return loops == 1 && mixer.master_gain == 1 && mixer.track_gains[7] == 1
               ? true
               : ab_fail(error, "Menu music overlap or unexpected output gain");
}

/* Isolate the live mixer route from the gameplay replay's RNG/frame schedule.
 * No output device or persistent user profile is opened by this fixture. */
static bool menu_audio_widgets(const AbZip *zip, PreviewMenu *menu, AbMetal *renderer,
                               AbDrawList *list, AbError *error) {
    menu_preview_free(menu);
    menu->highest_resources = true;
    menu->screen = (AbVec2){1024, 768};
    menu->navigation_enabled = menu->route_enabled = true;
    int16_t pcm[512];
    uint32_t voice = UINT32_MAX, observed = UINT32_MAX;
    if (!menu_preview_open(zip, (AbAssetTextureHost){renderer, upload, remove_texture},
                           list, menu, error) ||
        !menu_background_open(menu, error) ||
        !ab_audio_mixer_start(menu->mixer, true, error) ||
        !menu_preview_draw(menu, 0, error) ||
        !ab_audio_mixer_mix(menu->mixer, 512, pcm, 512, error) ||
        !menu_test_music(menu, UINT32_MAX, &voice, error) ||
        !route_enter(menu, error) || !menu_preview_draw(menu, 0.01f, error) ||
        !ab_audio_mixer_mix(menu->mixer, 0, pcm, 512, error) ||
        !menu_test_music(menu, voice, &observed, error))
        return false;
    AbAudioBankClip confirmation;
    if (!ab_audio_bank_clip(menu->audio, menu_span("menu_confirm"), &confirmation))
        return ab_fail(error, "Missing actual confirmation clip");
    uint32_t before_click = ab_audio_mixer_view(menu->mixer).next_id;
    if (!route_click(menu, &menu->episodes->cards.buttons[0].widget,
                     "GOTO_LEVEL_SELECTION", error))
        return false;
    AbAudioMixerView clicked = ab_audio_mixer_view(menu->mixer);
    size_t confirmations = 0;
    for (size_t index = 0; index < clicked.pending_count; ++index)
        if (clicked.pending[index].id >= before_click &&
            clicked.pending[index].clip_id == confirmation.id)
            ++confirmations;
    if (confirmations != 1)
        return ab_fail(error, "Pack selection queued %zu confirmation voices",
                       confirmations);
    for (size_t frame = 0; frame < 8 && !menu->level_active; ++frame)
        if (!menu_preview_draw(menu, 0.01f, error))
            return false;
    if (!menu->level_active || !menu_preview_draw(menu, 0, error) ||
        !ab_audio_mixer_mix(menu->mixer, 0, pcm, 512, error) ||
        !menu_test_music(menu, voice, &observed, error))
        return false;
    puts("Live menu audio: one unchanged 0.8-gain music voice across Play and "
         "Poached Eggs selection; one confirmation per pack click.");
    return true;
}

#endif
