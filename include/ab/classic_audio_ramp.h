#ifndef AB_CLASSIC_AUDIO_RAMP_H
#define AB_CLASSIC_AUDIO_RAMP_H

#include "ab/audio_mixer.h"
#include "ab/lua_data.h"

typedef struct {
    AbDataValue music_volume, music_length, effects_volume, effects_length;
} AbClassicAudioRampState;

typedef struct {
    AbClassicAudioRampState *state;
    AbAudioMixer *mixer;
    bool initialized, updating, failed;
} AbClassicAudioRamp;

/* Source setMusicVolume92869 targets track7; setEffectsVolume92729 visits0..6.
 * Apply actual first-party mixer track policy, not per-clip or master gain. */
bool ab_classic_set_music_volume(AbAudioMixer *mixer, float volume, AbError *error);
bool ab_classic_set_effects_volume(AbAudioMixer *mixer, float volume, AbError *error);
bool ab_classic_audio_ramp_initialize(AbClassicAudioRampState *state,
                                      AbAudioMixer *mixer, AbClassicAudioRamp *output,
                                      AbError *error);
/* Actual updateAudioRamp61785: music first, then effects. Nil/false is inactive;
 * numeric zero is active. Publish volume += seconds/length, then clear to nil
 * at <=0 or >1 before applying0/1; otherwise apply volume squared. Exactly1
 * remains active. Finite used inputs, including zero length whose infinity
 * result reaches a clamp. Indeterminate arithmetic rejects after its state
 * publication. Unused time/length fields are not read. No allocation/output
 * device; borrowed state/mixer outlive the owner. Failure stops this owner and
 * retains prior music/effects state and track changes. Reentry rejects. */
bool ab_classic_audio_ramp_update(AbClassicAudioRamp *ramp, float seconds,
                                  AbError *error);

#endif
