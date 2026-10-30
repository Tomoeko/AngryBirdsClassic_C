#include "ab/classic_audio_ramp.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Classic audio ramp failed at %d: %s\n", __LINE__,         \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static AbDataValue profile(unsigned index) {
    static const float volumes[] = {0, 0, 0, 0.5f, 1, -0.25f, 1.25f, 0.9375f};
    if (!index)
        return (AbDataValue){0};
    if (index == 1)
        return (AbDataValue){.kind = AB_DATA_BOOLEAN};
    return number(volumes[index]);
}

static bool words(FILE *reference, const uint32_t *expected, size_t count) {
    if (!reference)
        return true;
    for (size_t index = 0; index < count; ++index) {
        uint32_t actual;
        CHECK(fscanf(reference, "%" SCNx32, &actual) == 1);
        if (actual != expected[index]) {
            fprintf(stderr, "Audio ramp word %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    index, actual, expected[index]);
            return false;
        }
    }
    return true;
}

static bool snapshot(FILE *reference, unsigned case_id, unsigned step,
                     const AbClassicAudioRamp *ramp) {
    const AbClassicAudioRampState *state = ramp->state;
    uint32_t header[] = {case_id, step};
    static const uint32_t kinds[] = {0, 1, 3, 4, 5};
    uint32_t values[] = {kinds[state->music_volume.kind], state->music_volume.bits,
                         kinds[state->effects_volume.kind], state->effects_volume.bits};
    AbAudioMixerView view = ab_audio_mixer_view(ramp->mixer);
    uint32_t gains[8];
    for (unsigned track = 0; track < 8; ++track)
        gains[track] = number(view.track_gains[track]).bits;
    return words(reference, header, 2) && words(reference, values, 4) &&
           words(reference, gains, 8);
}

static bool corpus(FILE *reference) {
    if (reference) {
        char header[64];
        CHECK(fgets(header, sizeof(header), reference) &&
              !strcmp(header, "AB_ANDROID_AUDIO_RAMP_V1 64 5\n"));
    }
    static const float steps[] = {0.125f, 0, 0.25f, -0.0625f};
    for (unsigned case_id = 0; case_id < 64; ++case_id) {
        AbAudioMixer *mixer = NULL;
        CHECK(ab_audio_mixer_open((AbAudioMixerLimits){1, 1, 1, 4096}, &mixer, NULL));
        AbClassicAudioRampState state = {.music_volume = profile(case_id & 7),
                                         .effects_volume = profile(case_id >> 3),
                                         .music_length = number(case_id & 16  ? 0.0f
                                                                : case_id & 8 ? -2.0f
                                                                              : 2.0f),
                                         .effects_length =
                                             number(case_id & 32  ? 0
                                                    : case_id & 4 ? -0.5f
                                                                  : 0.5f)};
        AbClassicAudioRamp ramp = {0};
        CHECK(ab_classic_audio_ramp_initialize(&state, mixer, &ramp, NULL) &&
              snapshot(reference, case_id, 0, &ramp));
        for (unsigned step = 1; step <= 4; ++step)
            CHECK(ab_classic_audio_ramp_update(&ramp, steps[step - 1], NULL) &&
                  snapshot(reference, case_id, step, &ramp));
        ab_audio_mixer_close(mixer);
    }
    if (reference) {
        char marker[64];
        CHECK(fscanf(reference, " %63[^\n]", marker) == 1 &&
              !strcmp(marker, "AB audio ramp reference complete"));
        int byte;
        do {
            byte = fgetc(reference);
        } while (byte == '\n' || byte == '\r');
        CHECK(byte == EOF && !ferror(reference));
    }
    return true;
}

static bool edges(void) {
    AbClassicAudioRampState state = {.music_volume = {.kind = AB_DATA_BOOLEAN}};
    AbClassicAudioRamp ramp = {0};
    AbAudioMixer *mixer = NULL;
    CHECK(!ab_classic_audio_ramp_initialize(&state, NULL, &ramp, NULL) &&
          ab_audio_mixer_open((AbAudioMixerLimits){1, 1, 1, 4096}, &mixer, NULL) &&
          ab_classic_audio_ramp_initialize(&state, mixer, &ramp, NULL) &&
          ab_classic_audio_ramp_update(&ramp, NAN, NULL) && !ramp.failed &&
          state.music_volume.kind == AB_DATA_BOOLEAN &&
          state.effects_volume.kind == AB_DATA_NIL);
    CHECK(ab_classic_set_music_volume(mixer, 2, NULL) &&
          ab_classic_set_effects_volume(mixer, -1, NULL));
    AbAudioMixerView view = ab_audio_mixer_view(mixer);
    CHECK(view.track_gains[7] == 1 && view.track_gains[0] == 0 &&
          view.track_gains[6] == 0 && view.master_gain == 1);
    state.music_volume = number(0.5f);
    state.music_length = number(0.5f);
    state.effects_volume = number(0.5f);
    CHECK(!ab_classic_audio_ramp_update(&ramp, 0.125f, NULL) && ramp.failed &&
          !ramp.updating && state.music_volume.bits == number(0.75f).bits &&
          state.effects_volume.bits == number(0.5f).bits &&
          view.track_gains[7] == 0.5625f && view.track_gains[0] == 0 &&
          !ab_classic_audio_ramp_update(&ramp, 0, NULL));
    state =
        (AbClassicAudioRampState){.music_volume = number(0), .music_length = number(0)};
    CHECK(ab_classic_audio_ramp_initialize(&state, mixer, &ramp, NULL) &&
          ab_classic_audio_ramp_update(&ramp, 1, NULL) &&
          state.music_volume.kind == AB_DATA_NIL && view.track_gains[7] == 1);
    state.music_volume = number(0.5f);
    CHECK(!ab_classic_audio_ramp_update(&ramp, 0, NULL) && ramp.failed &&
          state.music_volume.kind == AB_DATA_NUMBER && view.track_gains[7] == 1);
    float invalid;
    memcpy(&invalid, &state.music_volume.bits, sizeof(invalid));
    CHECK(isnan(invalid));
    ab_audio_mixer_close(mixer);
    return true;
}

int main(int argc, char **argv) {
    FILE *reference = argc == 2 ? fopen(argv[1], "rb") : NULL;
    if (argc > 2 || (argc == 2 && !reference))
        return 2;
    bool okay = edges() && corpus(reference);
    if (reference)
        okay = fclose(reference) == 0 && okay;
    if (!okay)
        return 1;
    puts(reference
             ? "Audio ramps match 320 original state/track checkpoints"
             : "Audio ramp gates, track routing and partial failure checks passed");
    return 0;
}
