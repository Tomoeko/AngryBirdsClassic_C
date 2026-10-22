#include "ab/classic_audio_ramp.h"

#include <math.h>
#include <string.h>

bool ab_classic_set_music_volume(AbAudioMixer *mixer, float volume, AbError *error) {
    return ab_audio_mixer_track_gain(mixer, 7, volume, error);
}

bool ab_classic_set_effects_volume(AbAudioMixer *mixer, float volume, AbError *error) {
    for (uint32_t track = 0; track <= 6; ++track)
        if (!ab_audio_mixer_track_gain(mixer, track, volume, error))
            return false;
    return true;
}

bool ab_classic_audio_ramp_initialize(AbClassicAudioRampState *state,
                                      AbAudioMixer *mixer, AbClassicAudioRamp *output,
                                      AbError *error) {
    if (!state || !mixer || !output)
        return ab_fail(error, "Incomplete classic audio ramp binding");
    *output = (AbClassicAudioRamp){.state = state, .mixer = mixer, .initialized = true};
    return true;
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Nonnumeric reached audio ramp field");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite reached audio ramp field");
}

static bool update_volume(AbClassicAudioRamp *ramp, AbDataValue *volume,
                          const AbDataValue *length, float seconds, bool music,
                          AbError *error) {
    if (volume->kind == AB_DATA_NIL ||
        (volume->kind == AB_DATA_BOOLEAN && !volume->bits))
        return true;
    float current, duration;
    if (!numeric(*volume, &current, error) || !numeric(*length, &duration, error) ||
        !isfinite(seconds))
        return ab_fail(error, "Invalid reached audio ramp arithmetic");
    float fraction = seconds / duration;
    current += fraction;
    *volume = (AbDataValue){.kind = AB_DATA_NUMBER};
    memcpy(&volume->bits, &current, sizeof(current));
    float gain;
    if (current <= 0) {
        *volume = (AbDataValue){0};
        gain = 0;
    } else if (current > 1) {
        *volume = (AbDataValue){0};
        gain = 1;
    } else
        gain = current * current;
    return music ? ab_classic_set_music_volume(ramp->mixer, gain, error)
                 : ab_classic_set_effects_volume(ramp->mixer, gain, error);
}

bool ab_classic_audio_ramp_update(AbClassicAudioRamp *ramp, float seconds,
                                  AbError *error) {
    if (!ramp || !ramp->initialized || ramp->failed || ramp->updating || !ramp->state ||
        !ramp->mixer)
        return ab_fail(error, "Invalid or stopped classic audio ramp update");
    ramp->updating = true;
    bool okay = update_volume(ramp, &ramp->state->music_volume,
                              &ramp->state->music_length, seconds, true, error) &&
                update_volume(ramp, &ramp->state->effects_volume,
                              &ramp->state->effects_length, seconds, false, error);
    ramp->updating = false;
    ramp->failed = ramp->failed || !okay;
    return okay;
}
