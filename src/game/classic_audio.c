#include "ab/classic_audio.h"

bool ab_classic_audio_rolling(void *context, AbRollingAudioCall *call, AbError *error) {
    if (!call) {
        return ab_fail(error, "Missing classic rolling audio request");
    }
    AbAudioResources *resources = context;
    switch (call->operation) {
        case AB_ROLLING_AUDIO_QUERY:
            return ab_audio_resources_is_playing(resources, call->name, &call->playing,
                                                 error);
        case AB_ROLLING_AUDIO_PLAY: {
            uint32_t handle;
            return ab_audio_resources_play(resources, call->name, call->gain, true, 2,
                                           &handle, error);
        }
        case AB_ROLLING_AUDIO_GAIN:
            return ab_audio_resources_gain(resources, call->name, call->gain, error);
        case AB_ROLLING_AUDIO_STOP:
            return ab_audio_resources_stop(resources, call->name, error);
    }
    return ab_fail(error, "Invalid classic rolling audio operation");
}

bool ab_classic_audio_bank_rolling(void *context, AbRollingAudioCall *call,
                                   AbError *error) {
    if (!call)
        return ab_fail(error, "Missing classic bank rolling audio request");
    AbAudioBank *bank = context;
    switch (call->operation) {
        case AB_ROLLING_AUDIO_QUERY:
            return ab_audio_bank_is_playing(bank, call->name, &call->playing, error);
        case AB_ROLLING_AUDIO_PLAY: {
            uint32_t handle;
            return ab_audio_bank_play_resource(bank, call->name, call->gain, true, 2,
                                               &handle, error);
        }
        case AB_ROLLING_AUDIO_GAIN:
            return ab_audio_bank_gain(bank, call->name, call->gain, error);
        case AB_ROLLING_AUDIO_STOP:
            return ab_audio_bank_stop(bank, call->name, error);
    }
    return ab_fail(error, "Invalid classic bank rolling audio operation");
}
