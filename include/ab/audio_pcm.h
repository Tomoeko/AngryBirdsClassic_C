#ifndef AB_AUDIO_PCM_H
#define AB_AUDIO_PCM_H

#include "ab/pcm.h"
#include "ab/zip.h"

/* Decode supported RIFF/WAVE PCM16 or MPEG-2 Layer III to owned interleaved
 * PCM16, preserving source sample rate/channel count. max_frames bounds output
 * allocation. Input stays borrowed during decode. No resampling or playback.
 * Failure preserves output; release preceding owned output before replacement. */
bool ab_audio_pcm_decode(AbSpan input, size_t max_frames, AbPcm16 *output,
                         AbError *error);

/* Read one explicit APK path and decode it with the same ownership rules.
 * Archive/path stay borrowed during load. Bound the temporary encoded buffer
 * by max_input_bytes and retained PCM by max_frames. Resource-group selection,
 * name registration, caching and mixer lifetime belong to the caller. */
bool ab_audio_pcm_load(const AbZip *archive, const char *path, size_t max_input_bytes,
                       size_t max_frames, AbPcm16 *output, AbError *error);

#endif
