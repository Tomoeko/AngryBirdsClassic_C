#ifndef AB_AUDIO_MIX_H
#define AB_AUDIO_MIX_H

#include "ab/pcm.h"

typedef struct {
    float *samples;
    size_t frames;
    uint16_t channels;
} AbAudioMixBuffer;

typedef struct {
    float clip;
    float track;
    float master;
} AbAudioMixGains;

/* Accumulate the original mixer's equal-rate, unit-speed signed-16 projection.
 * Source has at least destination.frames; both views have one or two channels.
 * Clamp the requested track setting to [0,1], then multiply clip*track*master
 * in binary32. Normalize source samples by 32767. Preserve the original channel
 * mapping: stereo-to-mono adds the left channel twice with half weight.
 * Existing destination samples and gains must be finite, with a finite gain
 * product. Validate before writes. Arithmetic overflow follows binary32; later
 * storage clips infinities. Borrowed arrays must be disjoint. No allocation,
 * resampling, scheduling, looping, delay, fades or device output occurs. */
bool ab_audio_pcm16_accumulate(AbPcm16View source, AbAudioMixGains gains,
                               AbAudioMixBuffer destination, AbError *error);

/* Clamp each accumulator sample to the native signed-16 range. Inside [-1,1],
 * multiply by 32767 in binary32 and truncate toward zero; values below -1 become
 * -32768, values above 1 become 32767. Infinities clip; NaNs reject before any
 * write. Capacity counts samples, not frames. Arrays must be disjoint. */
bool ab_audio_pcm16_store(AbAudioMixBuffer source, int16_t *output, size_t capacity,
                          AbError *error);

#endif
