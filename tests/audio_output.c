#include "ab/audio_output.h"
#include "console_common/audio/output.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

struct CcAudioOutput {
    CcAudioOutputOptions options;
};

static CcAudioOutput *device;
static bool fail_start;

CcAudioOutput *cc_audio_output_open(const CcAudioOutputOptions *options) {
    if (!options || options->sample_rate != 16000 ||
        options->mode != CC_AUDIO_OUTPUT_DIRECT || device)
        return NULL;
    device = calloc(1, sizeof(*device));
    if (device)
        device->options = *options;
    return device;
}

bool cc_audio_output_start(CcAudioOutput *output) {
    return output && !fail_start;
}

bool cc_audio_output_stop(CcAudioOutput *output) {
    return output != NULL;
}

bool cc_audio_output_failed(const CcAudioOutput *output) {
    (void)output;
    return false;
}

void cc_audio_output_close(CcAudioOutput *output) {
    free(output);
    device = NULL;
}

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Audio output failed at %d: %s\n", __LINE__,               \
                    error.message);                                                    \
            return 1;                                                                  \
        }                                                                              \
    } while (0)

int main(void) {
    AbError error = {0};
    AbAudioMixer *mixer = NULL;
    AbAudioOutput *output = NULL;
    CHECK(ab_audio_mixer_open((AbAudioMixerLimits){4, 512, 1, 65536}, &mixer, &error));
    fail_start = true;
    CHECK(!ab_audio_output_open(mixer, &output, &error) && !output && !device);
    error = (AbError){0};
    fail_start = false;
    CHECK(ab_audio_output_open(mixer, &output, &error) && output && device);
    float stereo[8192];
    device->options.render(device->options.context, stereo, 1024);
    for (size_t index = 0; index < 2048; ++index)
        CHECK(stereo[index] == 0);
    int16_t samples[32];
    for (size_t index = 0; index < 32; ++index)
        samples[index] = index % 2 ? -16384 : 16384;
    uint32_t handle = 0;
    CHECK(ab_audio_mixer_play(mixer, 1, (AbPcm16View){samples, 32, 1}, 1, true, 0,
                              &handle, &error));
    uint64_t expected_frames = 1024, expected_nonzero = 0;
    for (unsigned pass = 0; pass < 2048; ++pass) {
        size_t frames = pass % 2 ? 701 : 129;
        CHECK(ab_audio_output_pump(output, &error));
        device->options.render(device->options.context, stereo, frames);
        for (size_t index = 0; index < frames; ++index) {
            CHECK(stereo[index * 2] == stereo[index * 2 + 1]);
            CHECK(fabsf(fabsf(stereo[index * 2]) - 0.5f) < 0.0001f);
        }
        expected_frames += frames;
        expected_nonzero += frames;
        AbAudioOutputView view = ab_audio_output_view(output);
        CHECK(view.rendered_frames == expected_frames &&
              view.nonzero_samples == expected_nonzero && !view.underrun_frames &&
              view.queued_frames <= 1024);
    }
    /* Exhaust the queue in one variable native callback and verify the
     * silent tail, accounting and recovery without replaying old samples. */
    AbAudioOutputView before = ab_audio_output_view(output);
    device->options.render(device->options.context, stereo, 4096);
    for (size_t index = before.queued_frames * 2; index < 8192; ++index)
        CHECK(stereo[index] == 0);
    CHECK(ab_audio_output_view(output).underrun_frames == 4096 - before.queued_frames &&
          ab_audio_output_pump(output, &error));
    CHECK(!ab_audio_output_capture_begin(output, 1024) &&
          ab_audio_output_pause(output, &error) &&
          !ab_audio_output_capture_begin(output, 0) &&
          ab_audio_output_capture_begin(output, 1024) &&
          !ab_audio_output_capture_begin(output, 1024) &&
          ab_audio_output_resume(output, &error));
    float captured[2048];
    device->options.render(device->options.context, stereo, 257);
    CHECK(ab_audio_output_capture_read(output, captured, 1024) == 257);
    for (size_t index = 0; index < 257 * 2; ++index)
        CHECK(captured[index] == stereo[index]);
    CHECK(!ab_audio_output_capture_failed(output) &&
          !ab_audio_output_capture_read(output, captured, 1024));
    device->options.render(device->options.context, stereo, 4096);
    CHECK(ab_audio_output_capture_failed(output));
    ab_audio_output_capture_end(output);
    CHECK(ab_audio_output_resume(output, &error));
    ab_audio_output_close(output);
    CHECK(!device && !ab_audio_mixer_view(mixer).started);
    ab_audio_mixer_close(mixer);
    puts(
        "Native audio adapter: 16-kHz format, mono/stereo mapping, variable callbacks, "
        "queue wrap, underrun recovery and startup failure cleanup.");
    return 0;
}
