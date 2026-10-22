#ifndef AB_TEST_CLASSIC_GAME_START_H
#define AB_TEST_CLASSIC_GAME_START_H

#include "ab/classic_game_start.h"

typedef struct {
    ShotTest *shot;
    AbClassicSling *sling;
    AbClassicGameStart *start;
    size_t registered_calls, level_calls;
    bool *physics, force_timer;
} LifecycleStart;

static bool lifecycle_start_function(void *context, float seconds, float time,
                                     AbError *error) {
    LifecycleStart *test = context;
    bool menu, ambient;
    CHECK(seconds == 0.125f && time == 4 &&
          ab_classic_sling_view(test->sling).current_bird_index == 0 &&
          ab_classic_sling_view(test->sling).fill_in_next_bird &&
          ab_audio_bank_is_playing(test->shot->audio, span("title_theme"), &menu,
                                   error) &&
          ab_audio_bank_is_playing(test->shot->audio, span("ambient_theme1"), &ambient,
                                   error) &&
          !menu && ambient &&
          !ab_classic_game_start_update(test->start, seconds, time, NULL));
    ++test->registered_calls;
    return true;
}

static bool lifecycle_start_level_hook(void *context, float seconds, float time,
                                       AbError *error) {
    LifecycleStart *test = context;
    (void)error;
    CHECK(seconds == 0.125f && time == 4 &&
          test->registered_calls == test->level_calls + 1);
    ++test->level_calls;
    if (test->force_timer) {
        CHECK(!*test->physics);
        test->start->config.state->level_start_timer = 6;
    }
    return true;
}

/* Controlled callbacks/initial values probe startup boundaries using the actual
 * shared bank/mixer and sling. This is not the selected level's bootstrap,
 * original callback registry, complete GameUpdate or an interactive session. */
static bool lifecycle_start(ShotTest *shot, AbClassicSling *sling, AbError *error) {
    AbClassicGameUpdates *updates = NULL;
    CHECK(ab_classic_game_updates_open(8, 8192, &updates, error) &&
          ab_classic_game_updates_clear(updates, error));
    AbClassicGameStartState state = {
        .right_sweep = {.kind = AB_DATA_BOOLEAN, .bits = 1},
        .left_sweep = {.kind = AB_DATA_BOOLEAN, .bits = 1},
        .quad_click_timer = 0.5f};
    AbDataValue release = {.kind = AB_DATA_BOOLEAN, .bits = 1};
    AbDataValue cheats = {0}, editor = {0};
    AbDataValue sound = {.kind = AB_DATA_STRING,
                         .string = span("level_start_military")};
    AbDataValue menu = {.kind = AB_DATA_STRING, .string = span("title_theme")};
    AbDataValue ambient = {.kind = AB_DATA_STRING, .string = span("ambient_theme1")};
    AbDataEntry key = {{.kind = AB_DATA_STRING, .string = span("PAGEUP")}, {0}};
    AbDataTable keys = {&key, 1, 1, NULL};
    bool physics = false;
    AbClassicGameStart start = {0};
    LifecycleStart test = {shot, sling, &start, 0, 0, &physics, false};
    AbClassicGameUpdateFunction function = {&test, lifecycle_start_function};
    CHECK(ab_classic_game_updates_add(updates, span("startupBoundaryProbe"), &function,
                                      error) &&
          ab_classic_game_start_initialize(
              (AbClassicGameStartConfig){.sling = sling,
                                         .audio = shot->audio,
                                         .updates = updates,
                                         .state = &state,
                                         .release_build = &release,
                                         .cheats_enabled = &cheats,
                                         .started_from_editor = &editor,
                                         .level_start_sound = &sound,
                                         .main_menu_song = &menu,
                                         .current_music = &ambient,
                                         .keys = &keys,
                                         .physics_enabled = &physics,
                                         .context = &test,
                                         .level_update = lifecycle_start_level_hook},
              &start, error));
    uint32_t handle;
    CHECK(ab_audio_bank_play_resource(shot->audio, menu.string, 1, true, 7, &handle,
                                      error) &&
          handle != UINT32_MAX &&
          ab_classic_game_start_update(&start, 0.125f, 4, error) &&
          state.right_sweep.kind == AB_DATA_BOOLEAN && !state.right_sweep.bits &&
          state.left_sweep.kind == AB_DATA_BOOLEAN && !state.left_sweep.bits &&
          state.quad_click_timer == 0.375f &&
          state.level_start_timer == 0.01f + 0.125f && !physics &&
          test.registered_calls == 1 && test.level_calls == 1);
    AbAudioBankClip clip;
    CHECK(ab_audio_bank_clip(shot->audio, ambient.string, &clip));
    AbAudioMixerView voices = ab_audio_mixer_view(shot->mixer);
    size_t ambient_count = 0;
    for (size_t index = 0; index < voices.pending_count; ++index) {
        const AbAudioVoice *voice = voices.pending + index;
        if (voice->clip_id == clip.id) {
            CHECK(voice->track == 7 && (voice->flags & AB_AUDIO_VOICE_LOOP));
            ++ambient_count;
        }
    }
    CHECK(ambient_count == 1);
    size_t audio = voices.pending_count;
    test.force_timer = true;
    CHECK(ab_classic_game_start_update(&start, 0.125f, 4, error) && physics &&
          state.level_start_timer == 10 && test.level_calls == 2 &&
          ab_audio_mixer_view(shot->mixer).pending_count == audio);
    test.force_timer = false;
    CHECK(ab_classic_game_start_update(&start, 0.125f, 4, error) &&
          state.level_start_timer == 10.125f && test.level_calls == 3 &&
          ab_audio_mixer_view(shot->mixer).pending_count == audio &&
          ab_classic_sling_disable_for_level(sling, error) &&
          ab_classic_sling_view(sling).disabled_for_current_level);
    state.level_start_timer = 0;
    physics = false;
    CHECK(ab_classic_game_start_update(&start, 0.125f, 4, error) && !physics &&
          state.level_start_timer == 0.125f && test.level_calls == 4 &&
          ab_audio_mixer_view(shot->mixer).pending_count == audio);
    release = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    key.value = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    AbError missing = {0};
    CHECK(!ab_classic_game_start_update(&start, 0.125f, 4, &missing) &&
          strstr(missing.message, "Missing actual game debug/editor service") &&
          state.quad_click_timer == -0.125f && state.level_start_timer == 0.125f &&
          test.level_calls == 4 && start.failed && !start.updating &&
          !ab_classic_game_start_update(&start, 0.125f, 4, NULL) &&
          state.quad_click_timer == -0.125f &&
          ab_audio_mixer_view(shot->mixer).pending_count == audio);
    ab_classic_game_updates_close(updates);
    return true;
}

#endif
