#ifndef AB_AUDIO_PLAY_H
#define AB_AUDIO_PLAY_H

#include "ab/lua_data.h"

typedef struct {
    AbDataValue name;
    AbDataValue volume;
    AbDataValue loop;
    AbDataValue channel;
} AbAudioPlay;

/* Resolve the original playAudio wrapper's final four resource arguments.
 * A present sound must be a constant table. Its truthy channel overrides the
 * caller channel; a truthy channel-volume entry then replaces caller volume;
 * truthy stored loop/volume entries override their corresponding values.
 * Remaining falsy values become volume one, loop false and channel zero.
 * Numeric zero and empty strings are truthy. A nil or empty-string name
 * suppresses the final request after metadata resolution. The original fifth
 * argument is unused. No numeric conversion, multiplication or playback occurs.
 *
 * Tables/strings stay borrowed; scalar names only, no metatables or table keys.
 * No allocation or mutation occurs. Failure preserves output and play. A
 * suppressed request sets play false and preserves output. Original resource
 * return values and downstream native argument conversions remain separate. */
bool ab_audio_play_resolve(const AbDataTable *sounds,
                           const AbDataTable *channel_volumes, AbAudioPlay request,
                           AbAudioPlay *output, bool *play, AbError *error);

#endif
