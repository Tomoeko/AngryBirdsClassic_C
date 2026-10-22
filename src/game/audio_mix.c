#include "ab/audio_mix.h"

#include <float.h>
#include <math.h>

_Static_assert(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24 &&
                   FLT_MAX_EXP == 128,
               "Audio mixing requires binary32 float");

static bool mix_buffer_valid(AbAudioMixBuffer buffer) {
    return buffer.channels >= 1 && buffer.channels <= 2 &&
           buffer.frames <= SIZE_MAX / buffer.channels &&
           (!buffer.frames || buffer.samples);
}

bool ab_audio_pcm16_accumulate(AbPcm16View source, AbAudioMixGains gains,
                               AbAudioMixBuffer destination, AbError *error) {
    if (!mix_buffer_valid(destination) || source.channels < 1 || source.channels > 2 ||
        source.frames > SIZE_MAX / source.channels ||
        source.frames < destination.frames || (destination.frames && !source.samples) ||
        !isfinite(gains.clip) || !isfinite(gains.track) || !isfinite(gains.master)) {
        return ab_fail(error, "Invalid equal-rate PCM mixing views or gains");
    }
    float track = gains.track < 0 ? 0 : gains.track > 1 ? 1 : gains.track;
    float gain = gains.clip * track;
    gain *= gains.master;
    if (!isfinite(gain)) {
        return ab_fail(error, "PCM mixing gain product is nonfinite");
    }
    size_t count = destination.frames * destination.channels;
    for (size_t index = 0; index < count; ++index) {
        if (!isfinite(destination.samples[index])) {
            return ab_fail(error, "PCM accumulation requires finite initial samples");
        }
    }
    float weight = source.channels > destination.channels
                       ? (float)destination.channels / (float)source.channels
                       : 1.0f;
    float coefficient = weight * gain;
    unsigned channels =
        source.channels > destination.channels ? source.channels : destination.channels;
    for (size_t frame = 0; frame < destination.frames; ++frame) {
        for (unsigned channel = 0; channel < channels; ++channel) {
            unsigned output_channel = channel % destination.channels;
            unsigned source_channel = output_channel % source.channels;
            size_t input = frame * source.channels + source_channel;
            float sample = (float)source.samples[input] / 32767.0f;
            if (sample != 0) {
                size_t output = frame * destination.channels + output_channel;
                float contribution = coefficient * sample;
                destination.samples[output] += contribution;
            }
        }
    }
    return true;
}

bool ab_audio_pcm16_store(AbAudioMixBuffer source, int16_t *output, size_t capacity,
                          AbError *error) {
    if (!mix_buffer_valid(source)) {
        return ab_fail(error, "Invalid PCM accumulation view");
    }
    size_t count = source.frames * source.channels;
    if (count > capacity || (count && !output)) {
        return ab_fail(error, "PCM output capacity is too small");
    }
    for (size_t index = 0; index < count; ++index) {
        if (isnan(source.samples[index])) {
            return ab_fail(error, "PCM conversion rejects NaN accumulation");
        }
    }
    for (size_t index = 0; index < count; ++index) {
        float sample = source.samples[index];
        if (sample > 1) {
            output[index] = INT16_MAX;
        } else if (sample < -1) {
            output[index] = INT16_MIN;
        } else {
            float scaled = sample * 32767.0f;
            output[index] = (int16_t)scaled;
        }
    }
    return true;
}
