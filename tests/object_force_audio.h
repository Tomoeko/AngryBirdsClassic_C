#ifndef AB_TEST_OBJECT_FORCE_AUDIO_H
#define AB_TEST_OBJECT_FORCE_AUDIO_H

#include "ab/audio_mixer.h"
#include "ab/object_force_sound.h"

static bool force_sound_edge_checks(void) {
    AbObjectForceSoundPlan plan;
    AbError error = {0};
    CHECK(ab_object_force_sound_plan(NULL, false, NULL, &plan, &error));
    CHECK(!plan.play);
    AbObjectForceSound sound = {
        .kind = AB_FORCE_SOUND_NAMED, .name = sprite_span("sound1"), .gain = NAN};
    CHECK(ab_object_force_sound_plan(&sound, true, NULL, &plan, &error));
    CHECK(plan.play && plan.gain == 1);
    sound.gain_present = true;
    sound.gain = -0.0f;
    CHECK(ab_object_force_sound_plan(&sound, true, NULL, &plan, &error));
    CHECK(plan.play && to_bits(plan.gain) == UINT32_C(0x80000000));
    AbObjectForceSoundPlan saved = plan;
    sound.gain = NAN;
    CHECK(!ab_object_force_sound_plan(&sound, true, NULL, &plan, &error));
    CHECK(memcmp(&plan, &saved, sizeof(plan)) == 0);
    AbSpan choice = sprite_span("sound1");
    sound = (AbObjectForceSound){
        .kind = AB_FORCE_SOUND_ARRAY, .choices = &choice, .count = 1};
    AbRandom48 random, expected;
    CHECK(ab_random48_seed(&random, 17));
    expected = random;
    uint32_t draw;
    CHECK(ab_random48_next(&expected, &draw));
    CHECK(ab_object_force_sound_plan(&sound, true, &random, &plan, &error));
    CHECK(plan.play && random.value == expected.value);
    CHECK(plan.name.size == choice.size && plan.name.data == choice.data);
    saved = plan;
    choice = (AbSpan){NULL, 1};
    CHECK(ab_random48_next(&expected, &draw));
    CHECK(!ab_object_force_sound_plan(&sound, true, &random, &plan, &error));
    CHECK(random.value == expected.value && memcmp(&plan, &saved, sizeof(plan)) == 0);
    sound.count = 0;
    CHECK(!ab_object_force_sound_plan(&sound, true, &random, &plan, &error));
    CHECK(random.value == expected.value);
    CHECK(!ab_object_force_sound_plan(NULL, true, &random, &plan, &error));
    CHECK(!ab_object_force_sound_plan(&sound, true, &random, NULL, &error));
    return true;
}

static uint32_t force_sound_id(AbSpan name, const AbSpan choices[3]) {
    for (uint32_t index = 0; index < 3; ++index) {
        if (name.size == choices[index].size &&
            memcmp(name.data, choices[index].data, name.size) == 0) {
            return index + 1;
        }
    }
    return 0;
}

static bool force_sound_voice_check(const AbAudioMixer *mixer,
                                    const uint32_t expected[11],
                                    const uint32_t summary[3]) {
    AbAudioMixerView view = ab_audio_mixer_view(mixer);
    CHECK(view.pending_count == summary[1] && view.active_count == summary[2]);
    if (!view.pending_count) {
        return true;
    }
    const AbAudioVoice *voice = view.pending;
    const uint32_t actual[] = {
        voice->id,
        voice->clip_id,
        voice->flags,
        voice->track,
        to_bits(voice->gain),
        (uint32_t)(voice->cursor_frames * voice->source.channels * 2),
        0,
        0,
        UINT32_C(0x3f800000),
        0,
        0};
    for (size_t field = 0; field < 11; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr,
                    "Force voice field %zu: %08" PRIx32 " instead of %08" PRIx32 "\n",
                    field, actual[field], expected[field]);
        }
        CHECK(actual[field] == expected[field]);
    }
    CHECK(voice->source.frames == 2 && voice->source.channels == 1);
    return true;
}

typedef struct {
    uint32_t summary[3];
    uint32_t voice[11];
} ForceSoundSnapshot;

static bool force_sound_native_check(FILE *file, const uint32_t input[36], bool pulse,
                                     size_t *requests, size_t *draws,
                                     ForceSoundSnapshot *snapshot) {
    uint32_t summary[3], voice_expected[11] = {0};
    CHECK(read_words(file, summary, 3));
    CHECK(summary[1] <= 1 && summary[2] == 0 && input[30] <= 4 && input[31] > 0 &&
          input[31] <= 3 && input[34] <= 1 && input[35] > 0 && input[35] <= 3);
    if (summary[1]) {
        CHECK(read_words(file, voice_expected, 11));
    }
    AbSpan choices[] = {sprite_span("sound1"), sprite_span("sound2"),
                        sprite_span("sound3")};
    AbObjectForceSound sound = {
        .kind = input[30] == 2                     ? AB_FORCE_SOUND_ARRAY
                : input[30] == 1 || input[30] == 4 ? AB_FORCE_SOUND_NAMED
                                                   : AB_FORCE_SOUND_NONE,
        .name = input[30] == 4 ? sprite_span("1") : choices[input[35] - 1],
        .choices = choices,
        .count = input[31],
        .gain = from_bits(input[33]),
        .gain_present = input[34] != 0,
    };
    AbRandom48 random;
    CHECK(ab_random48_seed(&random, input[32]));
    AbObjectForceSoundPlan plan;
    AbError error = {0};
    CHECK(ab_object_force_sound_plan(&sound, pulse, &random, &plan, &error));
    uint32_t after;
    CHECK(ab_random48_next(&random, &after) && after == summary[0]);
    *requests += plan.play;
    *draws += pulse && sound.kind == AB_FORCE_SOUND_ARRAY;
    uint32_t identifier = plan.play ? force_sound_id(plan.name, choices) : 0;
    AbAudioMixer *mixer = NULL;
    CHECK(ab_audio_mixer_open((AbAudioMixerLimits){1, 2, 1, 4096}, &mixer, &error));
    static const int16_t silence[2] = {0, 0};
    uint32_t handle;
    bool okay = ab_audio_mixer_start(mixer, true, &error) &&
                (!identifier ||
                 ab_audio_mixer_play(mixer, identifier, (AbPcm16View){silence, 2, 1},
                                     plan.gain, false, 0, &handle, &error)) &&
                force_sound_voice_check(mixer, voice_expected, summary);
    ab_audio_mixer_close(mixer);
    CHECK(okay);
    memcpy(snapshot->summary, summary, sizeof(summary));
    memcpy(snapshot->voice, voice_expected, sizeof(voice_expected));
    return true;
}

#endif
