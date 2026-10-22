#include "ab/audio_pcm.h"
#include "ab/audio_resources.h"
#include "ab/hash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Audio PCM failed at %d: %s\n", __LINE__, #condition);     \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool edges(void) {
    const uint8_t wave[] = {
        'R', 'I', 'F', 'F', 40,  0,   0,   0,   'W', 'A', 'V', 'E', 'f', 'm', 't', ' ',
        16,  0,   0,   0,   1,   0,   1,   0,   128, 62,  0,   0,   0,   125, 0,   0,
        2,   0,   16,  0,   'd', 'a', 't', 'a', 4,   0,   0,   0,   0,   128, 255, 127,
    };
    AbSpan input = {wave, sizeof(wave)};
    AbPcm16 pcm = {.frames = 99};
    CHECK(!ab_audio_pcm_decode((AbSpan){NULL, 0}, 2, &pcm, NULL) &&
          !ab_audio_pcm_decode(input, 2, NULL, NULL) &&
          !ab_audio_pcm_decode(input, 1, &pcm, NULL) && pcm.frames == 99 &&
          !pcm.samples && !ab_audio_pcm_load(NULL, "", 1, 1, &pcm, NULL));
    CHECK(ab_audio_pcm_decode(input, 2, &pcm, NULL) && pcm.frames == 2 &&
          pcm.sample_rate == 16000 && pcm.channels == 1 &&
          pcm.samples[0] == INT16_MIN && pcm.samples[1] == INT16_MAX);
    ab_pcm16_free(&pcm);
    uint8_t mp3[36] = {0xff, 0xf3, 0x18, 0xc0};
    CHECK(ab_audio_pcm_decode((AbSpan){mp3, sizeof(mp3)}, 576, &pcm, NULL) &&
          pcm.frames == 576 && pcm.sample_rate == 16000 && pcm.channels == 1);
    for (size_t index = 0; index < pcm.frames; ++index) {
        CHECK(pcm.samples[index] == 0);
    }
    ab_pcm16_free(&pcm);
    CHECK(!ab_audio_pcm_decode((AbSpan){wave, 10}, 2, &pcm, NULL) &&
          !ab_audio_pcm_decode((AbSpan){mp3, 10}, 576, &pcm, NULL) && !pcm.samples);
    return true;
}

static bool hash_matches(const AbPcm16 *pcm, const char *expected) {
    size_t count = pcm->frames * pcm->channels;
    uint8_t *encoded = malloc(count * 2);
    CHECK(encoded);
    for (size_t index = 0; index < count; ++index) {
        uint16_t value = (uint16_t)pcm->samples[index];
        encoded[index * 2] = (uint8_t)value;
        encoded[index * 2 + 1] = (uint8_t)(value >> 8);
    }
    uint8_t digest[32] = {0};
    char actual[65];
    bool okay = ab_sha256((AbSpan){encoded, count * 2}, digest, NULL);
    ab_digest_hex(digest, actual);
    free(encoded);
    CHECK(okay && strcmp(actual, expected) == 0);
    return true;
}

static bool borrow_for_mixer(const AbPcm16 *pcm) {
    AbAudioMixer *mixer = NULL;
    CHECK(ab_audio_mixer_open((AbAudioMixerLimits){2, 32, 1, 65536}, &mixer, NULL));
    AbAudioClipBinding clip = {{(const uint8_t *)"collision", 9},
                               7,
                               {pcm->samples, pcm->frames, pcm->channels}};
    AbAudioResources resources;
    uint32_t handle = UINT32_MAX;
    bool okay =
        ab_audio_resources_bind(mixer, &clip, 1, &resources, NULL) &&
        ab_audio_mixer_start(mixer, true, NULL) &&
        ab_audio_resources_play(&resources, clip.name, 1.0f, false, 0, &handle, NULL);
    AbAudioMixerView view = ab_audio_mixer_view(mixer);
    okay = okay && handle == 0 && view.pending_count == 1 &&
           view.pending[0].source.samples == pcm->samples &&
           view.pending[0].source.frames == pcm->frames &&
           view.pending[0].source.channels == pcm->channels;
    ab_audio_mixer_close(mixer);
    CHECK(okay);
    return true;
}

static bool apk_checks(const AbZip *archive) {
    static const struct {
        const char *path;
        size_t frames;
        uint32_t rate;
        uint16_t channels;
        const char *hash;
    } cases[] = {
        {"assets/data/audio/sfx/bird 01 collision a1.mp3", 8787, 16000, 1,
         "63d8517cc0f132cc5b41e97197b47edde89e2c8f0a2732e712589a88a0333b0c"},
        {"assets/data/audio/music/BeachAmbience.mp3", 1111510, 16000, 2,
         "0a2581558089d80acae8a8a74467359ac5b800f0fa7e745073284e6d4e891e05"},
        {"assets/data/audio/sfx/powerup_awarded.mp3", 54784, 24000, 1,
         "15ad93d62a4778581fb7136f70bfdf0ae71e7da91453de95ec6c3134eeb775eb"},
    };
    AbPcm16 pcm = {.frames = 99};
    CHECK(!ab_audio_pcm_load(archive, NULL, 512 * 1024, 2000000, &pcm, NULL) &&
          !ab_audio_pcm_load(archive, "absent", 512 * 1024, 2000000, &pcm, NULL) &&
          !ab_audio_pcm_load(archive, cases[0].path, 1, 2000000, &pcm, NULL) &&
          !ab_audio_pcm_load(archive, cases[0].path, 512 * 1024, 1, &pcm, NULL) &&
          pcm.frames == 99 && !pcm.samples);
    for (size_t index = 0; index < sizeof(cases) / sizeof(*cases); ++index) {
        AbError error = {0};
        if (!ab_audio_pcm_load(archive, cases[index].path, 512 * 1024, 2000000, &pcm,
                               &error)) {
            fprintf(stderr, "Loading %s: %s\n", cases[index].path, error.message);
            return false;
        }
        bool okay = pcm.frames == cases[index].frames &&
                    pcm.sample_rate == cases[index].rate &&
                    pcm.channels == cases[index].channels &&
                    hash_matches(&pcm, cases[index].hash) &&
                    (index != 0 || borrow_for_mixer(&pcm));
        ab_pcm16_free(&pcm);
        CHECK(okay);
    }
    CHECK(ab_audio_pcm_load(
              archive, "assets/data/audio/sfx/gamescorescreen_score_count_loop.wav",
              16384, 5730, &pcm, NULL) &&
          pcm.frames == 5730 && pcm.sample_rate == 16000 && pcm.channels == 1);
    ab_pcm16_free(&pcm);
    puts("APK PCM loader matches three native MP3 hashes, loads score WAVE and "
         "borrows decoded storage into named mixer bindings");
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edges() ? 0 : 1;
    }
    if (argc != 2) {
        fprintf(stderr, "Usage: ab_audio_pcm_tests [APK]\n");
        return 2;
    }
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbError error = {0};
    bool okay = ab_file_read(argv[1], 64 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
                apk_checks(&archive);
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay ? 0 : 1;
}
