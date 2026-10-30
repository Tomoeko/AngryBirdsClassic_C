#ifndef AB_AUDIO_RESAMPLE_H
#define AB_AUDIO_RESAMPLE_H

#include "ab/pcm.h"

enum { AB_AUDIO_RESAMPLE_TAPS = 64 };

typedef struct {
    float weights[2][3][AB_AUDIO_RESAMPLE_TAPS];
} AbAudioResampler;

/* First-party desktop interpolation for the pinned APK's 16/24-kHz PCM into
 * 48-kHz output. Ratios two and three use normalized Blackman-windowed sinc
 * phases. Integer phases preserve source PCM exactly. No allocation or latency;
 * finite clips extend with silence and looping clips wrap their sample window.
 * This presentation filter is separate from recovered native mixer arithmetic. */
bool ab_audio_resampler_init(AbAudioResampler *output, AbError *error);
bool ab_audio_resampler_sample(const AbAudioResampler *resampler, AbPcm16View source,
                               size_t frame, unsigned phase, unsigned ratio,
                               unsigned channel, bool loop, int16_t *output,
                               AbError *error);

#endif
