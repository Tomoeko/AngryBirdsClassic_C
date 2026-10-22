#ifndef AB_AUDIO_RESOURCES_H
#define AB_AUDIO_RESOURCES_H

#include "ab/audio_mixer.h"

typedef struct {
    AbSpan name;
    uint32_t clip_id;
    AbPcm16View pcm;
} AbAudioClipBinding;

typedef struct {
    AbAudioMixer *mixer;
    const AbAudioClipBinding *clips;
    size_t count;
} AbAudioResources;

/* Bind an already decoded equal-rate PCM16 resource map. Names must be unique
 * in full-byte lexicographic order. Empty/embedded-zero names are supported.
 * All arrays, name bytes, PCM and mixer remain borrowed and immutable in their
 * bindings; serialize mixer operations. Aliases sharing clip_id must identify
 * the same PCM asset. No allocation, decoding, resource-group selection or
 * device output. A NULL mixer represents absent native output. Invalid input
 * preserves output. This is a resolved resource boundary, not an APK loader. */
bool ab_audio_resources_bind(AbAudioMixer *mixer, const AbAudioClipBinding *clips,
                             size_t count, AbAudioResources *output, AbError *error);

/* Resource name operations with explicit unit speed and no fades/delays.
 * Missing names play as UINT32_MAX without consuming a voice ID; stop/gain are
 * no-ops. Play/stop/gain require output even when the name is absent. A playing
 * query without output succeeds with false. Clip gain and stop affect every
 * matching active/pending voice, independent of track. Output handles/queries
 * commit only on success. Names/bindings must remain valid during calls. */
bool ab_audio_resources_play(AbAudioResources *resources, AbSpan name, float gain,
                             bool loop, uint32_t track, uint32_t *handle,
                             AbError *error);
bool ab_audio_resources_stop(AbAudioResources *resources, AbSpan name, AbError *error);
bool ab_audio_resources_gain(AbAudioResources *resources, AbSpan name, float gain,
                             AbError *error);
bool ab_audio_resources_is_playing(const AbAudioResources *resources, AbSpan name,
                                   bool *output, AbError *error);

#endif
