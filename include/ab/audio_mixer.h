#ifndef AB_AUDIO_MIXER_H
#define AB_AUDIO_MIXER_H

#include "ab/audio_mix.h"

typedef struct AbAudioMixer AbAudioMixer;

enum {
    AB_AUDIO_VOICE_LOOP = 1,
    AB_AUDIO_VOICE_PAUSED = 2,
    AB_AUDIO_VOICE_ENDED = 4,
};

typedef enum {
    AB_AUDIO_PAUSE,
    AB_AUDIO_RESUME,
    AB_AUDIO_STOP,
} AbAudioVoiceAction;

typedef struct {
    uint32_t id;
    uint32_t clip_id;
    AbPcm16View source;
    size_t cursor_frames;
    float gain;
    uint32_t track;
    uint32_t flags;
} AbAudioVoice;

typedef struct {
    size_t voices;
    size_t frames;
    uint16_t channels;
    size_t storage_bytes;
} AbAudioMixerLimits;

typedef struct {
    const AbAudioVoice *active;
    size_t active_count;
    const AbAudioVoice *pending;
    size_t pending_count;
    const float *track_gains;
    const int32_t *track_limits;
    float master_gain;
    uint32_t next_id;
    bool started;
} AbAudioMixerView;

/* Own fixed active/pending voice arrays and PCM/float scratch. Bound all retained
 * allocations by storage_bytes. Start stopped, with master/track gains one and
 * eight unlimited tracks. One/two output channels, signed16, equal rate and unit
 * speed only. No fades, delays, decoder, threads or device output. Clip PCM stays
 * borrowed until every referencing voice is removed and any snapshots expire.
 * Caller serializes access. No allocations occur after open. Output starts NULL;
 * fully clean up failure. */
bool ab_audio_mixer_open(AbAudioMixerLimits limits, AbAudioMixer **output,
                         AbError *error);
void ab_audio_mixer_close(AbAudioMixer *mixer);
AbAudioMixerView ab_audio_mixer_view(const AbAudioMixer *mixer);

bool ab_audio_mixer_start(AbAudioMixer *mixer, bool started, AbError *error);
/* Queue an independent cursor over nonempty PCM. id is an asset identity supplied
 * by the caller. Stopped mixers and track-limit rejection succeed with handle
 * UINT32_MAX, consuming no id. Storage exhaustion is an explicit C error.
 * Gains must be finite; source length must fit the target's signed32 byte range.
 * Failure preserves handle/state. Play order is retained at the next mix. */
bool ab_audio_mixer_play(AbAudioMixer *mixer, uint32_t id, AbPcm16View source,
                         float gain, bool loop, uint32_t track, uint32_t *handle,
                         AbError *error);
bool ab_audio_mixer_voice_action(AbAudioMixer *mixer, uint32_t handle,
                                 AbAudioVoiceAction action, AbError *error);
bool ab_audio_mixer_all_action(AbAudioMixer *mixer, AbAudioVoiceAction action,
                               AbError *error);
bool ab_audio_mixer_voice_gain(AbAudioMixer *mixer, uint32_t handle, float gain,
                               AbError *error);
bool ab_audio_mixer_track_gain(AbAudioMixer *mixer, uint32_t track, float gain,
                               AbError *error);
bool ab_audio_mixer_master_gain(AbAudioMixer *mixer, float gain, AbError *error);
bool ab_audio_mixer_track_limit(AbAudioMixer *mixer, uint32_t track, int32_t limit,
                                AbError *error);
bool ab_audio_mixer_track_count(const AbAudioMixer *mixer, uint32_t track,
                                size_t *output, AbError *error);
/* Pending and paused voices count as playing; ended voices do not. */
bool ab_audio_mixer_is_playing(const AbAudioMixer *mixer, uint32_t handle);

/* Native clip-pointer operations use the caller's stable clip_id identity and
 * affect every matching active/pending voice, including ended voices awaiting
 * removal. Clip actions support PAUSE and STOP; native resume uses a voice
 * handle instead. Gain must be finite. Missing identities are no-ops. Paused
 * voices count as playing, ended voices do not. No allocation or device work. */
bool ab_audio_mixer_clip_action(AbAudioMixer *mixer, uint32_t clip_id,
                                AbAudioVoiceAction action, AbError *error);
bool ab_audio_mixer_clip_gain(AbAudioMixer *mixer, uint32_t clip_id, float gain,
                              AbError *error);
bool ab_audio_mixer_clip_is_playing(const AbAudioMixer *mixer, uint32_t clip_id);

/* Remove ended voices and publish pending voices in order, even at zero frames.
 * Paused voices keep their cursor. Non-looping partial reads pad silence without
 * marking ended until a later zero read; removal waits for the next mix boundary.
 * Looping reads reset at EOF as needed. Stopping admission does not stop this
 * explicit mix operation. Capacity counts signed16 samples, not frames. Output
 * must be disjoint from borrowed clip data. Reject invalid output bounds before
 * state changes; later arithmetic failure can follow cursor changes, requiring
 * the caller to discard the mixer. Borrowed views must not overlap mutations. */
bool ab_audio_mixer_mix(AbAudioMixer *mixer, size_t frames, int16_t *output,
                        size_t capacity, AbError *error);

#endif
