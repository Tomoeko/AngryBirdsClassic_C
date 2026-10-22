#ifndef AB_WAVE_H
#define AB_WAVE_H

#include "ab/base.h"

typedef struct {
    AbSpan pcm;
    size_t frames;
    uint32_t sample_rate;
    uint16_t channels;
} AbWave;

/* Bounded RIFF/WAVE PCM16 view. Support mono/stereo, little-endian signed
 * samples and positive sample rates with consistent byte rate/block alignment.
 * Require one format and one data chunk, allow either order and skip bounded
 * unknown chunks with word padding. The declared RIFF size covers the complete
 * input. Compressed, extended and other sample encodings reject explicitly.
 * Input stays borrowed; no allocation or playback occurs. Failure preserves
 * output. Metadata after data, including the target score loop's CDif, is not
 * included in its samples. */
bool ab_wave_read(AbSpan input, AbWave *output, AbError *error);

/* Decode interleaved channels to exact binary32 sample/32768 values. Capacity
 * counts floats, not frames. Validate view/capacity before any output write.
 * A zero-frame view accepts NULL samples. Input/output must be disjoint; no
 * allocation, resampling, channel remapping, mixing or device output occurs. */
bool ab_wave_decode(const AbWave *wave, float *samples, size_t capacity,
                    AbError *error);

#endif
