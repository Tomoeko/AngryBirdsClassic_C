#include "ab/audio_output.h"
#include "console_common/audio/output.h"
#include "console_common/capture/audio_buffer.h"

#include <stdatomic.h>
#include <stdlib.h>

enum { QUEUE_FRAMES = 8192, TARGET_FRAMES = 1024, MIX_FRAMES = 512 };

struct AbAudioOutput {
    AbAudioMixer *mixer;
    CcAudioOutput *device;
    CcAudioBuffer *capture;
    float samples[QUEUE_FRAMES];
    atomic_uint read_position, write_position;
    atomic_uint_fast64_t rendered, nonzero, underrun;
    bool failed, running;
};

static void render(void *context, float *stereo, size_t frames) {
    AbAudioOutput *output = context;
    unsigned read = atomic_load_explicit(&output->read_position, memory_order_relaxed);
    unsigned write =
        atomic_load_explicit(&output->write_position, memory_order_acquire);
    size_t available = write - read;
    size_t count = frames < available ? frames : available;
    uint64_t nonzero = 0;
    for (size_t index = 0; index < count; ++index) {
        float value = output->samples[(read + (unsigned)index) % QUEUE_FRAMES];
        stereo[index * 2] = stereo[index * 2 + 1] = value;
        nonzero += value != 0;
    }
    for (size_t index = count; index < frames; ++index)
        stereo[index * 2] = stereo[index * 2 + 1] = 0;
    atomic_store_explicit(&output->read_position, read + (unsigned)count,
                          memory_order_release);
    atomic_fetch_add_explicit(&output->rendered, frames, memory_order_relaxed);
    atomic_fetch_add_explicit(&output->nonzero, nonzero, memory_order_relaxed);
    atomic_fetch_add_explicit(&output->underrun, frames - count, memory_order_relaxed);
    cc_audio_buffer_write(output->capture, stereo, frames);
}

bool ab_audio_output_pump(AbAudioOutput *output, AbError *error) {
    if (!output || output->failed || cc_audio_output_failed(output->device))
        return ab_fail(error, "Missing or failed native audio output");
    unsigned write =
        atomic_load_explicit(&output->write_position, memory_order_relaxed);
    unsigned read = atomic_load_explicit(&output->read_position, memory_order_acquire);
    unsigned queued = write - read;
    if (queued > QUEUE_FRAMES) {
        output->failed = true;
        return ab_fail(error, "Invalid native audio queue distance");
    }
    int16_t pcm[MIX_FRAMES];
    while (queued < TARGET_FRAMES) {
        unsigned count = TARGET_FRAMES - queued;
        if (count > MIX_FRAMES)
            count = MIX_FRAMES;
        if (!ab_audio_mixer_mix(output->mixer, count, pcm, MIX_FRAMES, error)) {
            output->failed = true;
            return false;
        }
        for (unsigned index = 0; index < count; ++index)
            output->samples[(write + index) % QUEUE_FRAMES] = (float)pcm[index] / 32768;
        write += count;
        queued += count;
        atomic_store_explicit(&output->write_position, write, memory_order_release);
    }
    return true;
}

bool ab_audio_output_open(AbAudioMixer *mixer, AbAudioOutput **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing native audio output result");
    *output = NULL;
    if (!mixer)
        return ab_fail(error, "Missing native audio mixer");
    AbAudioOutput *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate native audio queue");
    owner->mixer = mixer;
    atomic_init(&owner->read_position, 0);
    atomic_init(&owner->write_position, 0);
    atomic_init(&owner->rendered, 0);
    atomic_init(&owner->nonzero, 0);
    atomic_init(&owner->underrun, 0);
    bool okay = atomic_is_lock_free(&owner->read_position) &&
                atomic_is_lock_free(&owner->write_position) &&
                atomic_is_lock_free(&owner->rendered) &&
                atomic_is_lock_free(&owner->nonzero) &&
                atomic_is_lock_free(&owner->underrun);
    if (!okay) {
        free(owner);
        return ab_fail(error, "Native audio needs lock-free queue counters");
    }
    owner->device =
        cc_audio_output_open(&(CcAudioOutputOptions){.sample_rate = 16000,
                                                     .render = render,
                                                     .context = owner,
                                                     .mode = CC_AUDIO_OUTPUT_DIRECT});
    okay = owner->device && ab_audio_mixer_start(mixer, true, error) &&
           ab_audio_output_pump(owner, error) && ab_audio_output_resume(owner, error);
    if (!okay) {
        ab_audio_output_close(owner);
        return error && error->message[0]
                   ? false
                   : ab_fail(error, "Cannot start the native audio device");
    }
    *output = owner;
    return true;
}

bool ab_audio_output_pause(AbAudioOutput *output, AbError *error) {
    if (!output || !output->device)
        return ab_fail(error, "Missing native audio device to pause");
    bool okay = !output->running || cc_audio_output_stop(output->device);
    output->running = false;
    return okay || ab_fail(error, "Cannot pause native audio output");
}

bool ab_audio_output_resume(AbAudioOutput *output, AbError *error) {
    if (!output || !output->device || output->failed)
        return ab_fail(error, "Missing or failed native audio device to resume");
    if (output->running)
        return true;
    output->running = cc_audio_output_start(output->device);
    return output->running || ab_fail(error, "Cannot resume native audio output");
}

bool ab_audio_output_capture_begin(AbAudioOutput *output, size_t capacity_frames) {
    if (!output || output->running || output->capture)
        return false;
    output->capture = cc_audio_buffer_create(capacity_frames);
    return output->capture != NULL;
}

size_t ab_audio_output_capture_read(AbAudioOutput *output, float *stereo,
                                    size_t capacity_frames) {
    return output ? cc_audio_buffer_read(output->capture, stereo, capacity_frames, NULL)
                  : 0;
}

bool ab_audio_output_capture_failed(const AbAudioOutput *output) {
    return output && cc_audio_buffer_failed(output->capture);
}

void ab_audio_output_capture_end(AbAudioOutput *output) {
    if (!output)
        return;
    AbError ignored = {0};
    (void)ab_audio_output_pause(output, &ignored);
    cc_audio_buffer_destroy(output->capture);
    output->capture = NULL;
}

AbAudioOutputView ab_audio_output_view(const AbAudioOutput *output) {
    if (!output)
        return (AbAudioOutputView){0};
    unsigned read = atomic_load_explicit(&output->read_position, memory_order_acquire);
    unsigned write =
        atomic_load_explicit(&output->write_position, memory_order_acquire);
    return (AbAudioOutputView){
        .queued_frames = write - read,
        .rendered_frames =
            atomic_load_explicit(&output->rendered, memory_order_relaxed),
        .nonzero_samples = atomic_load_explicit(&output->nonzero, memory_order_relaxed),
        .underrun_frames =
            atomic_load_explicit(&output->underrun, memory_order_relaxed)};
}

void ab_audio_output_close(AbAudioOutput *output) {
    if (!output)
        return;
    cc_audio_output_close(output->device);
    cc_audio_buffer_destroy(output->capture);
    AbError error = {0};
    (void)ab_audio_mixer_start(output->mixer, false, &error);
    free(output);
}
