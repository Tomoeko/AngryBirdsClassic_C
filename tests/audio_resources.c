#include "ab/classic_audio.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Audio resources failed at %d: %s\n", __LINE__,            \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbSpan string(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static bool resource_checks(AbAudioMixer *mixer) {
    const int16_t pcm[] = {-2000, 1000, 3000, 2000};
    AbAudioClipBinding clips[] = {
        {string("alpha"), 7, {pcm, 4, 1}},
        {{(const uint8_t *)"alpha\0x", 7}, 8, {pcm, 4, 1}},
        {string("zeta"), 7, {pcm, 4, 1}},
    };
    AbAudioResources resources = {0};
    CHECK(!ab_audio_resources_bind(mixer, NULL, 1, &resources, NULL) &&
          !ab_audio_resources_bind(mixer, clips, SIZE_MAX, &resources, NULL) &&
          !ab_audio_resources_bind(mixer, clips, 3, NULL, NULL));
    clips[1].name = clips[0].name;
    CHECK(!ab_audio_resources_bind(mixer, clips, 3, &resources, NULL) &&
          !resources.mixer);
    clips[1].name = (AbSpan){(const uint8_t *)"alpha\0x", 7};
    CHECK(ab_audio_resources_bind(NULL, clips, 3, &resources, NULL));
    uint32_t handle = 99;
    bool playing = true;
    CHECK(ab_audio_resources_is_playing(&resources, string("alpha"), &playing, NULL) &&
          !playing &&
          !ab_audio_resources_play(&resources, string("missing"), 1, false, 0, &handle,
                                   NULL) &&
          handle == 99 &&
          !ab_audio_resources_stop(&resources, string("missing"), NULL) &&
          !ab_audio_resources_gain(&resources, string("missing"), 1, NULL));
    CHECK(ab_audio_resources_bind(mixer, clips, 3, &resources, NULL) &&
          ab_audio_resources_play(&resources, string("missing"), NAN, false, 99,
                                  &handle, NULL) &&
          handle == UINT32_MAX && ab_audio_mixer_view(mixer).next_id == 0 &&
          ab_audio_mixer_start(mixer, true, NULL));
    AbRollingAudioCall call = {AB_ROLLING_AUDIO_QUERY, string("alpha"), 0, true};
    CHECK(ab_classic_audio_rolling(&resources, &call, NULL) && !call.playing);
    call.operation = AB_ROLLING_AUDIO_PLAY;
    call.gain = 0.5f;
    CHECK(ab_classic_audio_rolling(&resources, &call, NULL));
    AbAudioMixerView view = ab_audio_mixer_view(mixer);
    CHECK(view.pending_count == 1 && view.pending[0].clip_id == 7 &&
          view.pending[0].flags == AB_AUDIO_VOICE_LOOP && view.pending[0].track == 2 &&
          view.pending[0].gain == 0.5f);
    int16_t output[4];
    CHECK(ab_audio_mixer_mix(mixer, 4, output, 4, NULL) && output[0] == -1000 &&
          output[1] == 500 && output[2] == 1500 && output[3] == 1000 &&
          ab_audio_resources_play(&resources, string("zeta"), 1, false, 0, &handle,
                                  NULL) &&
          handle == 1 &&
          ab_audio_resources_play(&resources, clips[1].name, 1, false, 0, &handle,
                                  NULL) &&
          handle == 2);
    call.operation = AB_ROLLING_AUDIO_QUERY;
    CHECK(ab_classic_audio_rolling(&resources, &call, NULL) && call.playing);
    call.operation = AB_ROLLING_AUDIO_GAIN;
    call.gain = 0.25f;
    CHECK(ab_classic_audio_rolling(&resources, &call, NULL));
    view = ab_audio_mixer_view(mixer);
    CHECK(view.active_count == 1 && view.pending_count == 2 &&
          view.active[0].gain == 0.25f && view.pending[0].gain == 0.25f &&
          view.pending[1].gain == 1);
    call.operation = AB_ROLLING_AUDIO_STOP;
    call.name = string("zeta");
    CHECK(ab_classic_audio_rolling(&resources, &call, NULL));
    call.operation = AB_ROLLING_AUDIO_QUERY;
    CHECK(ab_classic_audio_rolling(&resources, &call, NULL) && !call.playing &&
          ab_audio_resources_is_playing(&resources, clips[1].name, &playing, NULL) &&
          playing && ab_audio_mixer_mix(mixer, 0, NULL, 0, NULL));
    view = ab_audio_mixer_view(mixer);
    CHECK(view.active_count == 1 && view.active[0].clip_id == 8 && !view.pending_count);
    call.operation = (AbRollingAudioOperation)99;
    CHECK(
        !ab_classic_audio_rolling(&resources, &call, NULL) &&
        !ab_classic_audio_rolling(&resources, NULL, NULL) &&
        !ab_classic_audio_rolling(NULL, &call, NULL) &&
        !ab_audio_resources_is_playing(NULL, string("alpha"), &playing, NULL) &&
        !ab_audio_resources_is_playing(&resources, (AbSpan){NULL, 1}, &playing, NULL) &&
        !ab_audio_resources_is_playing(&resources, string("alpha"), NULL, NULL));
    CHECK(ab_audio_mixer_all_action(mixer, AB_AUDIO_STOP, NULL) &&
          ab_audio_mixer_mix(mixer, 0, NULL, 0, NULL));
    return true;
}

int main(void) {
    AbAudioMixer *mixer = NULL;
    bool okay =
        ab_audio_mixer_open((AbAudioMixerLimits){8, 4, 1, 65536}, &mixer, NULL) &&
        resource_checks(mixer);
    ab_audio_mixer_close(mixer);
    return okay ? 0 : 1;
}
