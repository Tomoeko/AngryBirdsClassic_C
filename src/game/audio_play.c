#include "ab/audio_play.h"

#include <string.h>

static bool valid_table(const AbDataTable *table) {
    return table && table->count <= table->capacity &&
           (!table->count || table->entries);
}

static bool scalar(AbDataValue value) {
    return (unsigned)value.kind <= AB_DATA_STRING &&
           (value.kind != AB_DATA_STRING || !value.string.size || value.string.data);
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static AbDataValue property(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(
        table, (AbDataValue){.kind = AB_DATA_STRING,
                             .string = {(const uint8_t *)name, strlen(name)}});
    return value ? *value : (AbDataValue){0};
}

static void override_truthy(AbDataValue *destination, AbDataValue value) {
    if (truthy(value)) {
        *destination = value;
    }
}

bool ab_audio_play_resolve(const AbDataTable *sounds,
                           const AbDataTable *channel_volumes, AbAudioPlay request,
                           AbAudioPlay *output, bool *play, AbError *error) {
    if (!output || !play || !valid_table(sounds) || !valid_table(channel_volumes) ||
        !scalar(request.name) || !scalar(request.volume) || !scalar(request.loop) ||
        !scalar(request.channel)) {
        return ab_fail(error, "Invalid audio playback request or table storage");
    }
    const AbDataValue *sound = ab_data_get(sounds, request.name);
    if (sound && sound->kind != AB_DATA_NIL) {
        if (sound->kind != AB_DATA_TABLE || !valid_table(sound->table)) {
            return ab_fail(error, "Registered audio metadata is not a constant table");
        }
        override_truthy(&request.channel, property(sound->table, "channel"));
        if (!scalar(request.channel)) {
            return ab_fail(error, "Audio channel table keys are unsupported");
        }
        const AbDataValue *volume = ab_data_get(channel_volumes, request.channel);
        if (volume) {
            override_truthy(&request.volume, *volume);
        }
        override_truthy(&request.loop, property(sound->table, "loop"));
        override_truthy(&request.volume, property(sound->table, "volume"));
    }
    if (request.name.kind == AB_DATA_NIL ||
        (request.name.kind == AB_DATA_STRING && !request.name.string.size)) {
        *play = false;
        return true;
    }
    if (!scalar(request.volume) || !scalar(request.loop)) {
        return ab_fail(error, "Audio playback metadata requires scalar values");
    }
    if (!truthy(request.volume)) {
        request.volume =
            (AbDataValue){.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3f800000)};
    }
    if (!truthy(request.loop)) {
        request.loop = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    }
    if (!truthy(request.channel)) {
        request.channel = (AbDataValue){.kind = AB_DATA_NUMBER};
    }
    *output = request;
    *play = true;
    return true;
}
