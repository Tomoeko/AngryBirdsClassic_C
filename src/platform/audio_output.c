#include "ab/audio_output.h"
#include "console_common/audio/output.h"

#include <stdatomic.h>
#include <stdlib.h>

enum { QUEUE_FRAMES = 8192, TARGET_FRAMES = 1024, MIX_FRAMES = 512 };

struct AbAudioOutput {
    AbAudioMixer *mixer;
    CcAudioOutput *device;
    float samples[QUEUE_FRAMES];
    atomic_uint read_position, write_position;
    atomic_uint_fast64_t rendered, nonzero, underrun;
    bool failed;
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
           ab_audio_output_pump(owner, error) && cc_audio_output_start(owner->device);
    if (!okay) {
        ab_audio_output_close(owner);
        return error && error->message[0]
                   ? false
                   : ab_fail(error, "Cannot start the native audio device");
    }
    *output = owner;
    return true;
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
    AbError error = {0};
    (void)ab_audio_mixer_start(output->mixer, false, &error);
    free(output);
}
