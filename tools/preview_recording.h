#ifndef AB_PREVIEW_RECORDING_H
#define AB_PREVIEW_RECORDING_H

#include "ab/audio_output.h"
#include "console_common/capture/recording.h"
#include "console_common/support/host.h"

typedef struct {
    CcRecording *writer;
    AbAudioOutput *audio;
    bool audio_started;
} PreviewRecording;

static bool preview_recording_time(double *seconds, AbError *error) {
    struct timespec current;
    if (!cc_host_time(&current))
        return ab_fail(error, "Cannot read the recording clock");
    *seconds = (double)current.tv_sec + (double)current.tv_nsec * 1e-9;
    return true;
}

static bool preview_recording_audio_begin(void *context, size_t frames) {
    return ab_audio_output_capture_begin(context, frames);
}

static size_t preview_recording_audio_read(void *context, float *stereo,
                                           size_t frames) {
    return ab_audio_output_capture_read(context, stereo, frames);
}

static bool preview_recording_audio_failed(void *context) {
    return ab_audio_output_capture_failed(context);
}

static void preview_recording_audio_end(void *context) {
    ab_audio_output_capture_end(context);
}

static bool preview_recording_open(CcPlatform *platform, AbAudioOutput *audio,
                                   bool half_size, CcCaptureAudioMode mode,
                                   PreviewRecording *output, AbError *error) {
    if (!cc_capture_audio_mode_supported(mode))
        return ab_fail(error, "The requested recording audio mode is unavailable");
    if (audio && !ab_audio_output_pause(audio, error))
        return false;
    CcRecordingOptions options = {
        .platform = platform,
        .sample_rate = audio ? ab_audio_output_view(audio).sample_rate : 48000,
        .video_rate = 60,
        .half_size = half_size,
        .audio_mode = mode,
        .filename_prefix = "AngryBirds"};
    if (audio)
        options.audio =
            (CcRecordingAudioSource){.context = audio,
                                     .begin = preview_recording_audio_begin,
                                     .read = preview_recording_audio_read,
                                     .failed = preview_recording_audio_failed,
                                     .end = preview_recording_audio_end};
    time_t now = time(NULL);
    struct tm date;
    struct timespec clock;
    char stamp[32], path[128];
    if (!cc_host_localtime(&now, &date) || !cc_host_time(&clock) ||
        !strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &date))
        return ab_fail(error, "Cannot name the recording file");
    int count = snprintf(path, sizeof(path), "Files/AngryBirds-%s-%09ld.mp4", stamp,
                         clock.tv_nsec);
    if (count <= 0 || (size_t)count >= sizeof(path))
        return ab_fail(error, "Recording filename exceeds storage");
    output->writer = cc_recording_open_path(&options, path);
    if (!output->writer)
        return ab_fail(error, "Cannot start recording under Files/");
    output->audio = audio;
    fprintf(stderr, "Recording: %s\n", cc_recording_path(output->writer));
    return true;
}

static bool preview_recording_pump(PreviewRecording *recording, AbError *error) {
    if (!recording->writer)
        return true;
    double now = NAN;
    return preview_recording_time(&now, error) &&
           (cc_recording_pump(recording->writer, now) ||
            ab_fail(error, "%s", cc_recording_error(recording->writer)));
}

static bool preview_recording_frame(PreviewRecording *recording, AbError *error) {
    if (!recording->writer)
        return true;
    double now = NAN;
    if (!preview_recording_time(&now, error) ||
        !cc_recording_frame(recording->writer, now))
        return ab_fail(error, "Recording frame failed: %s",
                       cc_recording_error(recording->writer));
    if (!recording->audio || recording->audio_started)
        return true;
    if (!ab_audio_output_resume(recording->audio, error) ||
        !preview_recording_time(&now, error) ||
        !cc_recording_audio_start(recording->writer, now))
        return ab_fail(error, "Cannot start the recording audio timeline");
    recording->audio_started = true;
    return true;
}

static bool preview_recording_close(PreviewRecording *recording, AbError *error) {
    if (!recording->writer)
        return true;
    bool stopped = !recording->audio || ab_audio_output_pause(recording->audio, error);
    double now = NAN;
    bool clock_valid = preview_recording_time(&now, error);
    bool closed = cc_recording_close(recording->writer, now);
    *recording = (PreviewRecording){0};
    return (stopped && clock_valid && closed) ||
           ab_fail(error, "Cannot finalize the recording");
}

#endif
