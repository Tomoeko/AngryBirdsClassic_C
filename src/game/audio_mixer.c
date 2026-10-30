#include "ab/audio_mixer.h"
#include "ab/audio_resample.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct AbAudioMixer {
    AbAudioMixerLimits limits;
    AbAudioVoice *active;
    AbAudioVoice *pending;
    size_t active_count;
    size_t pending_count;
    float *accumulation;
    int16_t *scratch;
    float track_gains[8];
    int32_t track_limits[8];
    float master_gain;
    uint32_t next_id;
    bool started;
    uint32_t sample_rate;
    AbAudioResampler *resampler;
};

void ab_audio_mixer_close(AbAudioMixer *mixer) {
    if (!mixer) {
        return;
    }
    free(mixer->scratch);
    free(mixer->resampler);
    free(mixer->accumulation);
    free(mixer->pending);
    free(mixer->active);
    free(mixer);
}

bool ab_audio_mixer_open(AbAudioMixerLimits limits, AbAudioMixer **output,
                         AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing audio mixer output");
    }
    *output = NULL;
    if (!limits.voices || !limits.frames || limits.channels < 1 ||
        limits.channels > 2 || limits.voices > UINT32_MAX ||
        limits.frames > INT32_MAX / 4 ||
        limits.voices >
            (SIZE_MAX - sizeof(AbAudioMixer)) / (2 * sizeof(AbAudioVoice))) {
        return ab_fail(error, "Invalid audio mixer limits");
    }
    size_t bytes = sizeof(AbAudioMixer) + limits.voices * 2 * sizeof(AbAudioVoice);
    size_t frame_bytes = 2 * sizeof(int16_t) + limits.channels * sizeof(float);
    if (limits.frames > (SIZE_MAX - bytes) / frame_bytes ||
        bytes + limits.frames * frame_bytes > limits.storage_bytes) {
        return ab_fail(error, "Audio mixer exceeds retained storage limit");
    }
    AbAudioMixer *mixer = calloc(1, sizeof(*mixer));
    if (!mixer) {
        return ab_fail(error, "Cannot allocate audio mixer");
    }
    mixer->limits = limits;
    mixer->sample_rate = 16000;
    mixer->active = calloc(limits.voices, sizeof(*mixer->active));
    mixer->pending = calloc(limits.voices, sizeof(*mixer->pending));
    mixer->scratch = calloc(limits.frames * 2, sizeof(*mixer->scratch));
    mixer->accumulation =
        calloc(limits.frames * limits.channels, sizeof(*mixer->accumulation));
    if (!mixer->active || !mixer->pending || !mixer->scratch || !mixer->accumulation) {
        ab_audio_mixer_close(mixer);
        return ab_fail(error, "Cannot allocate audio mixer storage");
    }
    mixer->master_gain = 1;
    for (size_t track = 0; track < 8; ++track) {
        mixer->track_gains[track] = 1;
        mixer->track_limits[track] = -1;
    }
    *output = mixer;
    return true;
}

AbAudioMixerView ab_audio_mixer_view(const AbAudioMixer *mixer) {
    if (!mixer) {
        return (AbAudioMixerView){0};
    }
    return (AbAudioMixerView){
        mixer->active,        mixer->active_count,   mixer->pending,
        mixer->pending_count, mixer->track_gains,    mixer->track_limits,
        mixer->master_gain,   mixer->next_id,        mixer->started,
        mixer->sample_rate,   mixer->limits.channels};
}

bool ab_audio_mixer_configure_output(AbAudioMixer *mixer, uint32_t sample_rate,
                                     uint16_t channels, AbError *error) {
    if (!mixer || mixer->started || mixer->active_count || mixer->pending_count ||
        (sample_rate != 16000 && sample_rate != 48000) || channels < 1 || channels > 2)
        return ab_fail(error, "Invalid stopped audio output configuration");
    size_t bytes = sizeof(*mixer) + mixer->limits.voices * 2 * sizeof(AbAudioVoice);
    size_t frame_bytes = 2 * sizeof(int16_t) + channels * sizeof(float);
    size_t filter_bytes = sample_rate == 48000 ? sizeof(AbAudioResampler) : 0;
    if (bytes > mixer->limits.storage_bytes ||
        filter_bytes > mixer->limits.storage_bytes - bytes ||
        mixer->limits.frames >
            (mixer->limits.storage_bytes - bytes - filter_bytes) / frame_bytes)
        return ab_fail(error, "Audio output format exceeds retained storage limit");
    float *accumulation = calloc(mixer->limits.frames * channels, sizeof(float));
    AbAudioResampler *resampler = filter_bytes ? malloc(filter_bytes) : NULL;
    if (!accumulation || (filter_bytes && !resampler) ||
        (resampler && !ab_audio_resampler_init(resampler, error))) {
        free(accumulation);
        free(resampler);
        return ab_fail(error, "Cannot prepare audio output format");
    }
    free(mixer->accumulation);
    free(mixer->resampler);
    mixer->accumulation = accumulation;
    mixer->resampler = resampler;
    mixer->limits.channels = channels;
    mixer->sample_rate = sample_rate;
    return true;
}

bool ab_audio_mixer_start(AbAudioMixer *mixer, bool started, AbError *error) {
    if (!mixer) {
        return ab_fail(error, "Missing audio mixer");
    }
    mixer->started = started;
    return true;
}

bool ab_audio_mixer_track_count(const AbAudioMixer *mixer, uint32_t track,
                                size_t *output, AbError *error) {
    if (!mixer || !output || track >= 8) {
        return ab_fail(error, "Invalid audio track query");
    }
    size_t count = 0;
    for (size_t index = 0; index < mixer->active_count; ++index) {
        const AbAudioVoice *voice = mixer->active + index;
        count += voice->track == track && !(voice->flags & AB_AUDIO_VOICE_ENDED);
    }
    for (size_t index = 0; index < mixer->pending_count; ++index) {
        const AbAudioVoice *voice = mixer->pending + index;
        count += voice->track == track && !(voice->flags & AB_AUDIO_VOICE_ENDED);
    }
    *output = count;
    return true;
}

bool ab_audio_mixer_is_playing(const AbAudioMixer *mixer, uint32_t handle) {
    if (!mixer) {
        return false;
    }
    for (size_t index = 0; index < mixer->active_count; ++index) {
        const AbAudioVoice *voice = mixer->active + index;
        if (voice->id == handle && !(voice->flags & AB_AUDIO_VOICE_ENDED)) {
            return true;
        }
    }
    for (size_t index = 0; index < mixer->pending_count; ++index) {
        const AbAudioVoice *voice = mixer->pending + index;
        if (voice->id == handle && !(voice->flags & AB_AUDIO_VOICE_ENDED)) {
            return true;
        }
    }
    return false;
}

bool ab_audio_mixer_play(AbAudioMixer *mixer, uint32_t id, AbPcm16View source,
                         float gain, bool loop, uint32_t track, uint32_t *handle,
                         AbError *error) {
    return ab_audio_mixer_play_rate(mixer, id, source, mixer ? mixer->sample_rate : 0,
                                    gain, loop, track, handle, error);
}

bool ab_audio_mixer_play_rate(AbAudioMixer *mixer, uint32_t id, AbPcm16View source,
                              uint32_t sample_rate, float gain, bool loop,
                              uint32_t track, uint32_t *handle, AbError *error) {
    if (!mixer || !handle) {
        return ab_fail(error, "Missing audio mixer or playback output");
    }
    if (!mixer->started) {
        *handle = UINT32_MAX;
        return true;
    }
    size_t count;
    if (!ab_audio_mixer_track_count(mixer, track, &count, error)) {
        return false;
    }
    int32_t limit = mixer->track_limits[track];
    if (count >= (uint32_t)limit) {
        *handle = UINT32_MAX;
        return true;
    }
    if (!isfinite(gain) || !source.samples || !source.frames || source.channels < 1 ||
        source.channels > 2 ||
        source.frames > (size_t)INT32_MAX / (source.channels * 2u)) {
        return ab_fail(error, "Invalid equal-rate audio clip");
    }
    if (sample_rate != mixer->sample_rate &&
        !(mixer->sample_rate == 48000 &&
          (sample_rate == 16000 || sample_rate == 24000)))
        return ab_fail(error, "Unsupported source/output audio rate");
    if (mixer->active_count + mixer->pending_count == mixer->limits.voices) {
        return ab_fail(error, "Audio voice storage is full");
    }
    uint32_t next = mixer->next_id++;
    mixer->pending[mixer->pending_count++] =
        (AbAudioVoice){.id = next,
                       .clip_id = id,
                       .source = source,
                       .gain = gain,
                       .track = track,
                       .flags = loop ? AB_AUDIO_VOICE_LOOP : 0,
                       .sample_rate = sample_rate};
    *handle = next;
    return true;
}

static bool valid_action(AbAudioVoiceAction action) {
    return action == AB_AUDIO_PAUSE || action == AB_AUDIO_RESUME ||
           action == AB_AUDIO_STOP;
}

static void apply_action(AbAudioVoice *voice, AbAudioVoiceAction action) {
    if (action == AB_AUDIO_PAUSE) {
        voice->flags |= AB_AUDIO_VOICE_PAUSED;
    } else if (action == AB_AUDIO_RESUME) {
        voice->flags &= ~(uint32_t)AB_AUDIO_VOICE_PAUSED;
    } else {
        voice->flags |= AB_AUDIO_VOICE_ENDED;
    }
}

bool ab_audio_mixer_voice_action(AbAudioMixer *mixer, uint32_t handle,
                                 AbAudioVoiceAction action, AbError *error) {
    if (!mixer || !valid_action(action)) {
        return ab_fail(error, "Invalid audio voice action");
    }
    for (size_t index = 0; index < mixer->active_count; ++index) {
        if (mixer->active[index].id == handle) {
            apply_action(mixer->active + index, action);
        }
    }
    for (size_t index = 0; index < mixer->pending_count; ++index) {
        if (mixer->pending[index].id == handle) {
            apply_action(mixer->pending + index, action);
        }
    }
    return true;
}

bool ab_audio_mixer_all_action(AbAudioMixer *mixer, AbAudioVoiceAction action,
                               AbError *error) {
    if (!mixer || !valid_action(action)) {
        return ab_fail(error, "Invalid audio mixer action");
    }
    for (size_t index = 0; index < mixer->active_count; ++index) {
        apply_action(mixer->active + index, action);
    }
    for (size_t index = 0; index < mixer->pending_count; ++index) {
        apply_action(mixer->pending + index, action);
    }
    return true;
}

bool ab_audio_mixer_clip_action(AbAudioMixer *mixer, uint32_t clip_id,
                                AbAudioVoiceAction action, AbError *error) {
    if (!mixer || (action != AB_AUDIO_PAUSE && action != AB_AUDIO_STOP)) {
        return ab_fail(error, "Invalid audio clip action");
    }
    for (size_t index = 0; index < mixer->active_count; ++index) {
        if (mixer->active[index].clip_id == clip_id) {
            apply_action(mixer->active + index, action);
        }
    }
    for (size_t index = 0; index < mixer->pending_count; ++index) {
        if (mixer->pending[index].clip_id == clip_id) {
            apply_action(mixer->pending + index, action);
        }
    }
    return true;
}

bool ab_audio_mixer_clip_gain(AbAudioMixer *mixer, uint32_t clip_id, float gain,
                              AbError *error) {
    if (!mixer || !isfinite(gain)) {
        return ab_fail(error, "Invalid audio clip gain");
    }
    for (size_t index = 0; index < mixer->active_count; ++index) {
        if (mixer->active[index].clip_id == clip_id) {
            mixer->active[index].gain = gain;
        }
    }
    for (size_t index = 0; index < mixer->pending_count; ++index) {
        if (mixer->pending[index].clip_id == clip_id) {
            mixer->pending[index].gain = gain;
        }
    }
    return true;
}

bool ab_audio_mixer_clip_is_playing(const AbAudioMixer *mixer, uint32_t clip_id) {
    if (!mixer) {
        return false;
    }
    for (size_t index = 0; index < mixer->active_count; ++index) {
        if (mixer->active[index].clip_id == clip_id &&
            !(mixer->active[index].flags & AB_AUDIO_VOICE_ENDED)) {
            return true;
        }
    }
    for (size_t index = 0; index < mixer->pending_count; ++index) {
        if (mixer->pending[index].clip_id == clip_id &&
            !(mixer->pending[index].flags & AB_AUDIO_VOICE_ENDED)) {
            return true;
        }
    }
    return false;
}

bool ab_audio_mixer_voice_gain(AbAudioMixer *mixer, uint32_t handle, float gain,
                               AbError *error) {
    if (!mixer || !isfinite(gain)) {
        return ab_fail(error, "Invalid audio voice gain");
    }
    for (size_t index = 0; index < mixer->active_count; ++index) {
        if (mixer->active[index].id == handle) {
            mixer->active[index].gain = gain;
        }
    }
    for (size_t index = 0; index < mixer->pending_count; ++index) {
        if (mixer->pending[index].id == handle) {
            mixer->pending[index].gain = gain;
        }
    }
    return true;
}

bool ab_audio_mixer_track_gain(AbAudioMixer *mixer, uint32_t track, float gain,
                               AbError *error) {
    if (!mixer || track >= 8 || !isfinite(gain)) {
        return ab_fail(error, "Invalid audio track gain");
    }
    mixer->track_gains[track] = gain < 0 ? 0 : gain > 1 ? 1 : gain;
    return true;
}

bool ab_audio_mixer_master_gain(AbAudioMixer *mixer, float gain, AbError *error) {
    if (!mixer || !isfinite(gain)) {
        return ab_fail(error, "Invalid audio master gain");
    }
    mixer->master_gain = gain;
    return true;
}

bool ab_audio_mixer_track_limit(AbAudioMixer *mixer, uint32_t track, int32_t limit,
                                AbError *error) {
    if (!mixer || track >= 8) {
        return ab_fail(error, "Invalid audio track limit");
    }
    mixer->track_limits[track] = limit;
    return true;
}

static void publish_pending(AbAudioMixer *mixer) {
    size_t count = 0;
    for (size_t index = 0; index < mixer->active_count; ++index) {
        if (!(mixer->active[index].flags & AB_AUDIO_VOICE_ENDED)) {
            mixer->active[count++] = mixer->active[index];
        }
    }
    for (size_t index = 0; index < mixer->pending_count; ++index) {
        if (!(mixer->pending[index].flags & AB_AUDIO_VOICE_ENDED)) {
            mixer->active[count++] = mixer->pending[index];
        }
    }
    mixer->active_count = count;
    mixer->pending_count = 0;
}

static bool fetch_source(AbAudioVoice *voice, size_t frames, int16_t *scratch,
                         AbError *error) {
    if (voice->cursor_frames > voice->source.frames) {
        return ab_fail(error, "Invalid audio source cursor");
    }
    size_t channels = voice->source.channels;
    memset(scratch, 0, frames * channels * sizeof(*scratch));
    size_t copied = 0;
    do {
        size_t count = voice->source.frames - voice->cursor_frames;
        if (count > frames - copied) {
            count = frames - copied;
        }
        if (count) {
            memcpy(scratch + copied * channels,
                   voice->source.samples + voice->cursor_frames * channels,
                   count * channels * sizeof(*scratch));
            voice->cursor_frames += count;
            copied += count;
        } else if (voice->flags & AB_AUDIO_VOICE_LOOP) {
            voice->cursor_frames = 0;
        } else {
            voice->flags |= AB_AUDIO_VOICE_ENDED;
        }
        if (!(voice->flags & AB_AUDIO_VOICE_LOOP)) {
            break;
        }
    } while (copied < frames);
    return true;
}

static bool fetch_resampled(AbAudioMixer *mixer, AbAudioVoice *voice, size_t frames,
                            AbError *error) {
    unsigned ratio = mixer->sample_rate / voice->sample_rate;
    size_t channels = voice->source.channels;
    bool loop = (voice->flags & AB_AUDIO_VOICE_LOOP) != 0;
    memset(mixer->scratch, 0, frames * channels * sizeof(int16_t));
    size_t copied = 0;
    for (; copied < frames; ++copied) {
        if (voice->cursor_frames == voice->source.frames) {
            if (!loop)
                break;
            voice->cursor_frames = 0;
        }
        for (unsigned channel = 0; channel < channels; ++channel)
            if (!ab_audio_resampler_sample(
                    mixer->resampler, voice->source, voice->cursor_frames,
                    voice->rate_phase, ratio, channel, loop,
                    mixer->scratch + copied * channels + channel, error))
                return false;
        if (++voice->rate_phase == ratio) {
            voice->rate_phase = 0;
            ++voice->cursor_frames;
        }
    }
    if (!copied && !loop)
        voice->flags |= AB_AUDIO_VOICE_ENDED;
    return true;
}

bool ab_audio_mixer_mix(AbAudioMixer *mixer, size_t frames, int16_t *output,
                        size_t capacity, AbError *error) {
    if (!mixer || frames > mixer->limits.frames ||
        frames > capacity / mixer->limits.channels || (frames && !output)) {
        return ab_fail(error, "Invalid audio mix output bounds");
    }
    publish_pending(mixer);
    size_t count = frames * mixer->limits.channels;
    memset(mixer->accumulation, 0, count * sizeof(*mixer->accumulation));
    AbAudioMixBuffer accumulation = {mixer->accumulation, frames,
                                     mixer->limits.channels};
    for (size_t index = 0; frames && index < mixer->active_count; ++index) {
        AbAudioVoice *voice = mixer->active + index;
        if (voice->flags & (AB_AUDIO_VOICE_PAUSED | AB_AUDIO_VOICE_ENDED)) {
            continue;
        }
        bool fetched = voice->sample_rate == mixer->sample_rate
                           ? fetch_source(voice, frames, mixer->scratch, error)
                           : fetch_resampled(mixer, voice, frames, error);
        if (!fetched) {
            return false;
        }
        AbPcm16View source = {mixer->scratch, frames, voice->source.channels};
        AbAudioMixGains gains = {voice->gain, mixer->track_gains[voice->track],
                                 mixer->master_gain};
        if (!ab_audio_pcm16_accumulate(source, gains, accumulation, error)) {
            return false;
        }
    }
    return ab_audio_pcm16_store(accumulation, output, capacity, error);
}
