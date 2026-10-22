#ifndef AB_AUDIO_OUTPUT_H
#define AB_AUDIO_OUTPUT_H

#include "ab/audio_mixer.h"

typedef struct AbAudioOutput AbAudioOutput;

typedef struct {
    uint32_t queued_frames;
    uint64_t rendered_frames, nonzero_samples, underrun_frames;
} AbAudioOutputView;

/* Native stereo output over the target's mono signed16 mixer. The application
 * thread exclusively owns mixer calls and pumps a bounded SPSC queue; the
 * device callback only consumes prepared floats. Duplicate the mono channel
 * to both device channels at signed16/32768 gain, without resampling.
 * Open starts the real mixer/device at 16000 Hz. Missing output fails explicitly.
 * No callback allocations, decoding, locks or game-state access. Close joins
 * device callbacks before releasing queue storage; the borrowed mixer outlives it.
 * Pump at each application frame, including menus and paused game scenes.
 * Queue underruns output silence and are observable in the view. */
bool ab_audio_output_open(AbAudioMixer *mixer, AbAudioOutput **output, AbError *error);
bool ab_audio_output_pump(AbAudioOutput *output, AbError *error);
AbAudioOutputView ab_audio_output_view(const AbAudioOutput *output);
void ab_audio_output_close(AbAudioOutput *output);

#endif
