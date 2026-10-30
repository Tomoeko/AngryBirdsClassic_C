#include "ab/audio_bank.h"
#include "ab/classic_pause_actions.h"
#include "ab/hash.h"
#include "ab/target_data.h"
#include "ab/target_resources.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Audio bank check failed at %d: %s\n", __LINE__,           \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    char path[512], pcm_hash[65];
    uint32_t frames, rate, channels;
} NativeAudio;

typedef struct {
    AbAudioBank *bank;
    AbDataValue *restarted;
    AbDataTable *first;
    size_t calls;
} PauseObservation;

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static bool paused(void *context, AbDataTable *notice, AbError *error) {
    PauseObservation *observation = context;
    const char *const names[] = {"wood_rolling", "rock_rolling", "light_rolling"};
    if (observation->restarted->kind != AB_DATA_STRING ||
        observation->restarted->string.size != 10 ||
        memcmp(observation->restarted->string.data, "pause menu", 10))
        return ab_fail(error, "Pause event preceded the actual restart-origin write");
    for (size_t index = 0; index < 3; ++index) {
        bool playing;
        if (!ab_audio_bank_is_playing(observation->bank, span(names[index]), &playing,
                                      error) ||
            playing)
            return ab_fail(error, "Pause event preceded actual rolling-audio stops");
    }
    if (observation->calls && notice == observation->first)
        return ab_fail(error, "Common pause action reused its event table");
    observation->first = observation->calls ? observation->first : notice;
    ++observation->calls;
    return true;
}

static bool pause_actions(const AbZip *archive, AbSpan key, AbAudioBank *bank,
                          AbAudioMixer *mixer, AbError *error) {
    AbTargetData ids = {0};
    AbEventManager *events = NULL;
    bool okay = false;
    AbDataValue restarted = {0}, zoom = number(1), typo = {0};
    AbClassicPauseActions actions = {0};
    AbClassicPauseNotice notices[2];
    PauseObservation observation = {.bank = bank, .restarted = &restarted};
    AbEventListener listener = {.name = span("pause-audio-test"),
                                .context = &observation,
                                .event_triggered = paused};
    if (!ab_target_data_open(archive, "assets/data/scripts_common/events.lua", key,
                             1024 * 1024, NULL, &ids, error) ||
        !ab_event_manager_open((AbEventManagerLimits){4, 4, 4, 4, 4, 16384},
                               (AbEventManagerHost){0}, &events, error))
        goto release;
    const AbDataValue *id =
        ab_data_get(ids.data.globals, text("EID_ABOUT_MENU_OPENED"));
    if (!id || !ab_event_manager_add(events, *id, &listener, error) ||
        !ab_classic_pause_actions_init(
            &actions,
            (AbClassicPauseActionConfig){bank, events, ids.data.globals, &restarted,
                                         &zoom, &typo, notices, 2},
            error))
        goto release;
    int16_t unused[1];
    if (!ab_audio_mixer_all_action(mixer, AB_AUDIO_STOP, error) ||
        !ab_audio_mixer_mix(mixer, 0, unused, 1, error) ||
        !ab_audio_mixer_mix(mixer, 0, unused, 1, error))
        goto release;
    const char *const names[] = {"wood_rolling", "rock_rolling", "light_rolling"};
    size_t present = 0;
    for (size_t index = 0; index < 3; ++index) {
        AbAudioBankClip clip;
        if (!ab_audio_bank_clip(bank, span(names[index]), &clip))
            continue;
        uint32_t handle;
        bool playing;
        if (!ab_audio_bank_play_resource(bank, span(names[index]), 1, true, 0, &handle,
                                         error) ||
            handle == UINT32_MAX ||
            !ab_audio_bank_is_playing(bank, span(names[index]), &playing, error) ||
            !playing)
            goto release;
        ++present;
    }
    if (present != 3 || !ab_classic_pause_show(&actions, error) ||
        !ab_classic_pause_show(&actions, error) || observation.calls != 2 ||
        actions.notice_count != 2 ||
        notices[0].table.entries == notices[1].table.entries)
        goto release;
    printf("Common pause actions stop %zu registered rolling resources before "
           "the two distinct source events.\n",
           present);
    okay = true;
release:
    ab_event_manager_close(events);
    ab_target_data_close(&ids);
    return okay;
}

static bool reference_read(FILE *file, NativeAudio reference[347]) {
    char marker[80], source_hash[65];
    CHECK(fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB_MP3_FORMAT_REFERENCE_V1 347\n"));
    for (size_t index = 0; index < 347; ++index) {
        NativeAudio *row = reference + index;
        CHECK(fgets(row->path, sizeof(row->path), file));
        size_t length = strlen(row->path);
        CHECK(length && row->path[length - 1] == '\n');
        row->path[length - 1] = 0;
        CHECK(fscanf(file, "%64s", source_hash) == 1);
        uint32_t words[8];
        for (size_t word = 0; word < 8; ++word)
            CHECK(fscanf(file, "%8" SCNx32, words + word) == 1);
        row->frames = words[4];
        row->rate = words[1];
        row->channels = words[2];
        CHECK(fscanf(file, "%64s", row->pcm_hash) == 1 && fgetc(file) == '\n');
    }
    CHECK(fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB MP3 format reference complete\n") && fgetc(file) == EOF &&
          !ferror(file));
    return true;
}

static bool pcm_matches(AbAudioBankClip clip, const NativeAudio *reference) {
    CHECK(clip.pcm.frames == reference->frames && clip.sample_rate == reference->rate &&
          clip.pcm.channels == reference->channels);
    size_t samples = clip.pcm.frames * clip.pcm.channels;
    uint8_t *bytes = malloc(samples * 2);
    CHECK(bytes);
    for (size_t index = 0; index < samples; ++index) {
        uint16_t sample = (uint16_t)clip.pcm.samples[index];
        bytes[2 * index] = (uint8_t)sample;
        bytes[2 * index + 1] = (uint8_t)(sample >> 8);
    }
    uint8_t digest[32];
    char hash[65];
    bool okay = ab_sha256((AbSpan){bytes, samples * 2}, digest, NULL);
    free(bytes);
    CHECK(okay);
    ab_digest_hex(digest, hash);
    CHECK(!strcmp(hash, reference->pcm_hash));
    return true;
}

static bool bank_check(AbAudioBank *bank, AbAudioMixer *mixer, AbRandom48 *random,
                       const NativeAudio reference[347]) {
    CHECK(ab_audio_bank_count(bank) == 340 && ab_audio_bank_bytes(bank) > 1024 * 1024);
    size_t compared = 0, wav = 0, streams = 0, nonstandard_rates = 0;
    for (size_t index = 0; index < ab_audio_bank_count(bank); ++index) {
        AbAudioBankClip clip;
        CHECK(ab_audio_bank_at(bank, index, &clip));
        streams += clip.stream;
        nonstandard_rates += clip.sample_rate != 16000;
        const NativeAudio *native = NULL;
        for (size_t candidate = 0; candidate < 347; ++candidate) {
            AbSpan path = span(reference[candidate].path);
            if (path.size == clip.path.size &&
                !memcmp(path.data, clip.path.data, path.size)) {
                native = reference + candidate;
                break;
            }
        }
        if (native) {
            CHECK(pcm_matches(clip, native));
            ++compared;
        } else {
            CHECK(clip.name.size == strlen("score_count") &&
                  !memcmp(clip.name.data, "score_count", clip.name.size) &&
                  clip.pcm.frames == 5730 && clip.pcm.channels == 1 &&
                  clip.sample_rate == 16000);
            ++wav;
        }
    }
    if (compared != 339 || wav != 1 || streams != 35 || nonstandard_rates != 1)
        fprintf(stderr,
                "Audio bank totals: MP3 %zu WAV %zu stream %zu other-rate %zu\n",
                compared, wav, streams, nonstandard_rates);
    CHECK(compared == 339 && wav == 1 && streams == 35 && nonstandard_rates == 1);
    AbRandom48 before = *random;
    const char *resolved = NULL;
    CHECK(ab_audio_bank_name(bank, "bird_01_flying", &resolved, NULL) && resolved &&
          !strcmp(resolved, "bird_01_flying") && random->value == before.value);
    const uint8_t unterminated[] = {'x', 'y', 'z'};
    AbDataValue typed = {.kind = AB_DATA_STRING,
                         .string = {unterminated, sizeof(unterminated)}};
    AbDataValue typed_result;
    CHECK(ab_audio_bank_name_value(bank, typed, &typed_result, NULL) &&
          typed_result.kind == AB_DATA_STRING &&
          typed_result.string.data == unterminated && typed_result.string.size == 3 &&
          random->value == before.value &&
          ab_audio_bank_name_value(bank, (AbDataValue){0}, &typed_result, NULL) &&
          typed_result.kind == AB_DATA_NIL && random->value == before.value);
    AbRandom48 expected = *random;
    uint32_t ignored;
    const char *sling = NULL;
    CHECK(ab_random48_next(&expected, &ignored) &&
          ab_audio_bank_name(bank, "bird_shot", &sling, NULL) && sling &&
          strcmp(sling, "bird_shot") && random->value == expected.value);
    AbAudioBankClip selected;
    CHECK(ab_audio_bank_clip(bank, span(sling), &selected));
    AbAudioBankClip bird;
    CHECK(ab_audio_bank_clip(bank, span(resolved), &bird));
    uint32_t handle = 99;
    AbAudioPlay request = {.name = text(resolved), .volume = number(1)};
    CHECK(ab_audio_mixer_start(mixer, true, NULL) &&
          ab_audio_bank_play(bank, request, &handle, NULL) && handle != UINT32_MAX);
    AbAudioMixerView view = ab_audio_mixer_view(mixer);
    CHECK(view.pending_count == 1 && view.pending[0].clip_id == bird.id &&
          view.pending[0].source.samples == bird.pcm.samples &&
          view.pending[0].gain == 1 && view.pending[0].track == 0);
    bool playing;
    CHECK(ab_audio_bank_is_playing(bank, span(resolved), &playing, NULL) && playing);
    int16_t samples[512];
    CHECK(ab_audio_mixer_mix(mixer, 512, samples, 512, NULL));
    view = ab_audio_mixer_view(mixer);
    CHECK(view.active_count == 1 && view.active[0].cursor_frames == 512 &&
          ab_audio_bank_stop(bank, span(resolved), NULL) &&
          ab_audio_mixer_mix(mixer, 0, samples, 512, NULL));
    request.name = text("powerup_awarded");
    handle = 99;
    CHECK(!ab_audio_bank_play(bank, request, &handle, NULL) && handle == 99 &&
          !ab_audio_mixer_view(mixer).pending_count);
    request.name = text("unregistered-fixture");
    uint32_t next = ab_audio_mixer_view(mixer).next_id;
    CHECK(ab_audio_bank_play(bank, request, &handle, NULL) && handle == UINT32_MAX &&
          ab_audio_mixer_view(mixer).next_id == next);
    request.name = text(resolved);
    request.channel = number(7);
    for (size_t index = 0; index < 4; ++index) {
        CHECK(ab_audio_bank_play(bank, request, &handle, NULL));
        CHECK((handle == UINT32_MAX) == (index == 3));
    }
    CHECK(ab_audio_mixer_view(mixer).pending_count == 3);
    puts("Audio bank owns all 340 registrations; 339 MP3 bindings match complete "
         "native PCM hashes. Real metadata/resource/mixer admission passes; the "
         "24-kHz clip rejects pending rate recovery. No device output was opened.");
    return true;
}

static bool desktop_quality(AbAudioBank *bank, AbAudioMixer *mixer, AbError *error) {
    CHECK(ab_audio_mixer_all_action(mixer, AB_AUDIO_STOP, error));
    CHECK(ab_audio_mixer_mix(mixer, 0, NULL, 0, error));
    ab_audio_bank_coalesce_frame_effects(bank, true);
    uint32_t first, duplicate, later;
    AbAudioBankClip wood = {0};
    AbSpan wood_path = span("assets/data/audio/sfx/wood destroyed a1.mp3");
    for (size_t index = 0; index < ab_audio_bank_count(bank); ++index) {
        AbAudioBankClip clip;
        CHECK(ab_audio_bank_at(bank, index, &clip));
        if (clip.path.size == wood_path.size &&
            !memcmp(clip.path.data, wood_path.data, wood_path.size))
            wood = clip;
    }
    CHECK(wood.pcm.samples);
    CHECK(ab_audio_bank_play_resource(bank, wood.name, .7f, false, 3, &first, error));
    CHECK(ab_audio_bank_play_resource(bank, wood.name, .5f, false, 3, &later, error));
    CHECK(first != later);
    CHECK(
        ab_audio_bank_play_resource(bank, wood.name, .7f, false, 3, &duplicate, error));
    CHECK(duplicate == first && ab_audio_mixer_view(mixer).pending_count == 2);
    CHECK(ab_audio_bank_play_resource(bank, wood.name, .7f, false, 2, &later, error));
    CHECK(later != first && ab_audio_mixer_view(mixer).pending_count == 3);
    CHECK(ab_audio_mixer_all_action(mixer, AB_AUDIO_STOP, error));
    CHECK(ab_audio_mixer_mix(mixer, 0, NULL, 0, error));
    ab_audio_bank_begin_frame(bank);
    AbSpan thump = span("mighty_eagle_thump");
    CHECK(ab_audio_bank_play_resource(bank, thump, 1, false, 0, &first, error));
    CHECK(ab_audio_bank_play_resource(bank, thump, 1, false, 0, &duplicate, error));
    CHECK(first == duplicate && ab_audio_mixer_view(mixer).pending_count == 1);
    CHECK(ab_audio_mixer_start(mixer, false, error));
    CHECK(ab_audio_bank_play_resource(bank, thump, 1, false, 0, &duplicate, error));
    CHECK(duplicate == UINT32_MAX);
    CHECK(ab_audio_mixer_start(mixer, true, error));
    ab_audio_bank_begin_frame(bank);
    CHECK(ab_audio_bank_play_resource(bank, thump, 1, false, 0, &later, error));
    CHECK(later != first && ab_audio_mixer_view(mixer).pending_count == 2);
    CHECK(ab_audio_bank_stop(bank, thump, error));
    CHECK(ab_audio_bank_play_resource(bank, thump, 1, false, 0, &duplicate, error));
    CHECK(duplicate != later);
    CHECK(ab_audio_mixer_all_action(mixer, AB_AUDIO_STOP, error));
    CHECK(ab_audio_mixer_mix(mixer, 0, NULL, 0, error));
    CHECK(ab_audio_mixer_start(mixer, false, error));
    CHECK(ab_audio_mixer_configure_output(mixer, 48000, 2, error));
    CHECK(ab_audio_mixer_start(mixer, true, error));
    AbAudioBankClip award = {0}, ambience = {0};
    for (size_t index = 0; index < ab_audio_bank_count(bank); ++index) {
        AbAudioBankClip clip;
        CHECK(ab_audio_bank_at(bank, index, &clip));
        if (clip.sample_rate == 24000)
            award = clip;
        if (clip.pcm.channels == 2)
            ambience = clip;
    }
    CHECK(award.pcm.samples && ambience.pcm.samples);
    CHECK(ab_audio_bank_play_resource(bank, award.name, .5f, false, 0, &first, error));
    CHECK(
        ab_audio_bank_play_resource(bank, ambience.name, .5f, true, 7, &later, error));
    int16_t output[1024];
    CHECK(ab_audio_mixer_mix(mixer, 512, output, 1024, error));
    AbAudioMixerView view = ab_audio_mixer_view(mixer);
    CHECK(view.active_count == 2 && view.active[0].sample_rate == 24000 &&
          view.active[0].cursor_frames == 256 && view.active[1].sample_rate == 16000 &&
          view.active[1].cursor_frames == 170 && view.active[1].rate_phase == 2);
    puts("Desktop audio bank: duplicate Eagle effects share one voice per frame; "
         "later/stopped effects retrigger, and original 24-kHz/stereo PCM plays at 48 "
         "kHz.");
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fputs("Usage: ab_audio_bank_tests PINNED_APK NATIVE_MP3_FORMAT_CORPUS\n",
              stderr);
        return 2;
    }
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbAudioBank *bank = NULL;
    AbAudioMixer *mixer = NULL;
    AbRandom48 random;
    NativeAudio *reference = calloc(347, sizeof(*reference));
    AbError error = {0};
    uint8_t key[32] = {0};
    FILE *file = fopen(argv[2], "rb");
    bool okay =
        reference && file && reference_read(file, reference) &&
        ab_file_read(argv[1], 64 * 1024 * 1024, &bytes, &error) &&
        ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
        ab_random48_seed(&random, 7) &&
        ab_audio_mixer_open((AbAudioMixerLimits){32, 512, 1, 256 * 1024}, &mixer,
                            &error) &&
        ab_target_resource_key(&archive, 16 * 1024 * 1024, key, &error) &&
        ab_audio_bank_open(&archive, (AbSpan){key, sizeof(key)},
                           (AbAudioBankConfig){mixer, &random, 64 * 1024 * 1024,
                                               4 * 1024 * 1024, 4 * 1024 * 1024},
                           &bank, &error);
    okay = okay && bank_check(bank, mixer, &random, reference);
    okay = okay &&
           pause_actions(&archive, (AbSpan){key, sizeof(key)}, bank, mixer, &error);
    okay = okay && desktop_quality(bank, mixer, &error);
    memset(key, 0, sizeof(key));
    if (!okay && error.message[0])
        fprintf(stderr, "%s\n", error.message);
    ab_audio_mixer_close(mixer);
    ab_audio_bank_close(bank);
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    free(reference);
    if (file && fclose(file))
        okay = false;
    return okay ? 0 : 1;
}
