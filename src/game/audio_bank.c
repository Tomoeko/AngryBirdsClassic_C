#include "ab/audio_bank.h"
#include "ab/audio_definition.h"
#include "ab/audio_group.h"
#include "ab/audio_pcm.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AbAudioDefinition definition;
    AbPcm16 pcm;
    AbDataTable metadata;
    AbDataEntry fields[5];
    uint8_t path[512];
} BankClip;

struct AbAudioBank {
    AbAudioBankConfig config;
    AbAudioCatalog catalog;
    BankClip *clips;
    AbAudioClipBinding *bindings;
    AbDataEntry *sound_entries;
    AbDataTable sounds, channel_volumes;
    AbDataEntry channel_entries[7];
    AbAudioResources resources;
    size_t count, capacity, bytes;
};

const AbDataTable *ab_audio_bank_groups(const AbAudioBank *bank) {
    return bank ? bank->catalog.groups : NULL;
}

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(name));
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static int compare_names(AbSpan first, AbSpan second) {
    size_t length = first.size < second.size ? first.size : second.size;
    int order = length ? memcmp(first.data, second.data, length) : 0;
    return order ? order : (first.size > second.size) - (first.size < second.size);
}

static int compare_bindings(const void *first, const void *second) {
    return compare_names(((const AbAudioClipBinding *)first)->name,
                         ((const AbAudioClipBinding *)second)->name);
}

static BankClip *find_clip(const AbAudioBank *bank, AbSpan name) {
    size_t begin = 0, end = bank->count;
    while (begin < end) {
        size_t middle = begin + (end - begin) / 2;
        int order = compare_names(name, bank->bindings[middle].name);
        if (!order)
            return bank->clips + bank->bindings[middle].clip_id;
        if (order < 0)
            end = middle;
        else
            begin = middle + 1;
    }
    return NULL;
}

static bool reserve(AbAudioBank *bank, size_t count, size_t element, AbError *error) {
    if (bank->bytes > bank->config.storage_bytes ||
        count > (bank->config.storage_bytes - bank->bytes) / element)
        return ab_fail(error, "Audio bank exceeds retained byte limit");
    bank->bytes += count * element;
    return true;
}

static bool load_clip(AbAudioBank *bank, const AbZip *archive,
                      const AbDataTable *properties, AbError *error) {
    AbDataValue name = field(properties, "clipName");
    const AbDataValue *existing = ab_data_get(&bank->sounds, name);
    BankClip *clip = bank->clips + bank->count;
    bool created = false;
    if (!ab_audio_definition_read(properties, existing ? *existing : (AbDataValue){0},
                                  span("assets/data/audio"), true, clip->path,
                                  sizeof(clip->path) - 1, &clip->definition, &created,
                                  error))
        return false;
    if (!created)
        return true;
    size_t available = bank->config.storage_bytes - bank->bytes;
    size_t frames = available / 4;
    if (frames > bank->config.frames_per_clip)
        frames = bank->config.frames_per_clip;
    clip->path[clip->definition.path.size] = 0;
    if (!ab_audio_pcm_load(archive, (const char *)clip->path, bank->config.entry_bytes,
                           frames, &clip->pcm, error))
        return false;
    /* Adopt the PCM before any later failure so close owns its release. */
    size_t index = bank->count++;
    if (!reserve(bank, clip->pcm.frames, clip->pcm.channels * sizeof(int16_t), error))
        return false;
    clip->fields[0] = (AbDataEntry){text("name"), clip->definition.name};
    clip->fields[1] = (AbDataEntry){text("channel"), clip->definition.channel};
    clip->fields[2] = (AbDataEntry){text("volume"), clip->definition.volume};
    clip->fields[3] =
        (AbDataEntry){text("canBeOverWritten"), clip->definition.can_be_overwritten};
    clip->fields[4] = (AbDataEntry){text("loop"), clip->definition.loop};
    clip->metadata = (AbDataTable){clip->fields, 5, 5, NULL};
    bank->sound_entries[bank->sounds.count++] =
        (AbDataEntry){name, {.kind = AB_DATA_TABLE, .table = &clip->metadata}};
    bank->bindings[index] =
        (AbAudioClipBinding){clip->definition.name.string,
                             (uint32_t)index,
                             {clip->pcm.samples, clip->pcm.frames, clip->pcm.channels}};
    return true;
}

static bool load_catalog(AbAudioBank *bank, const AbZip *archive, AbSpan key,
                         AbError *error) {
    if (!ab_audio_catalog_open(archive, key, bank->config.storage_bytes - bank->bytes,
                               &bank->catalog, error))
        return false;
    if (!reserve(bank, bank->catalog.owned_bytes.size, 1, error) ||
        !reserve(bank, bank->catalog.chunk.allocated_bytes, 1, error) ||
        !reserve(bank, bank->catalog.data.allocated_bytes, 1, error))
        return false;
    bank->capacity = bank->catalog.registrations->count;
    if (bank->capacity > UINT32_MAX ||
        !reserve(bank, bank->capacity, sizeof(BankClip), error) ||
        !reserve(bank, bank->capacity, sizeof(AbAudioClipBinding), error) ||
        !reserve(bank, bank->capacity, sizeof(AbDataEntry), error))
        return false;
    bank->clips = calloc(bank->capacity, sizeof(*bank->clips));
    bank->bindings = calloc(bank->capacity, sizeof(*bank->bindings));
    bank->sound_entries = calloc(bank->capacity, sizeof(*bank->sound_entries));
    if (!bank->clips || !bank->bindings || !bank->sound_entries)
        return ab_fail(error, "Cannot allocate audio bank bindings");
    bank->sounds = (AbDataTable){bank->sound_entries, 0, bank->capacity, NULL};
    for (size_t index = 0; index < bank->capacity; ++index) {
        const AbDataValue *properties =
            ab_data_get(bank->catalog.registrations, number((float)(index + 1)));
        if (!properties || properties->kind != AB_DATA_TABLE)
            return ab_fail(error, "Invalid ordered audio registration");
        if (!load_clip(bank, archive, properties->table, error))
            return false;
    }
    qsort(bank->bindings, bank->count, sizeof(*bank->bindings), compare_bindings);
    if (!ab_audio_resources_bind(bank->config.mixer, bank->bindings, bank->count,
                                 &bank->resources, error))
        return false;
    /* Pinned loadAudioAssets: channels 1..7 have wrapper volume one and these
     * native voice limits. Channel zero retains the mixer's existing policy. */
    static const int32_t limits[] = {4, 4, 6, 4, 6, 4, 3};
    bank->channel_volumes = (AbDataTable){bank->channel_entries, 7, 7, NULL};
    for (size_t index = 0; index < 7; ++index) {
        bank->channel_entries[index] =
            (AbDataEntry){number((float)(index + 1)), number(1)};
        if (!ab_audio_mixer_track_limit(bank->config.mixer, (uint32_t)index + 1,
                                        limits[index], error))
            return false;
    }
    return true;
}

bool ab_audio_bank_open(const AbZip *archive, AbSpan key, AbAudioBankConfig config,
                        AbAudioBank **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing audio bank output");
    *output = NULL;
    if (!archive || !config.mixer || !config.random || !config.entry_bytes ||
        !config.frames_per_clip || config.storage_bytes < sizeof(AbAudioBank))
        return ab_fail(error, "Invalid audio bank bindings or limits");
    AbAudioBank *bank = calloc(1, sizeof(*bank));
    if (!bank)
        return ab_fail(error, "Cannot allocate audio bank");
    bank->config = config;
    bank->bytes = sizeof(*bank);
    if (!load_catalog(bank, archive, key, error)) {
        ab_audio_bank_close(bank);
        return false;
    }
    *output = bank;
    return true;
}

void ab_audio_bank_close(AbAudioBank *bank) {
    if (!bank)
        return;
    for (size_t index = 0; index < bank->count; ++index)
        ab_pcm16_free(&bank->clips[index].pcm);
    free(bank->sound_entries);
    free(bank->bindings);
    free(bank->clips);
    ab_audio_catalog_close(&bank->catalog);
    free(bank);
}

size_t ab_audio_bank_bytes(const AbAudioBank *bank) {
    return bank ? bank->bytes : 0;
}

size_t ab_audio_bank_count(const AbAudioBank *bank) {
    return bank ? bank->count : 0;
}

bool ab_audio_bank_clip(const AbAudioBank *bank, AbSpan name, AbAudioBankClip *output) {
    if (!bank || !output || (name.size && !name.data))
        return false;
    const BankClip *clip = find_clip(bank, name);
    if (!clip)
        return false;
    *output =
        (AbAudioBankClip){clip->definition.name.string,
                          clip->definition.path,
                          (uint32_t)(clip - bank->clips),
                          clip->pcm.sample_rate,
                          {clip->pcm.samples, clip->pcm.frames, clip->pcm.channels},
                          truthy(clip->definition.stream)};
    return true;
}

bool ab_audio_bank_at(const AbAudioBank *bank, size_t index, AbAudioBankClip *output) {
    return bank && index < bank->count &&
           ab_audio_bank_clip(bank, bank->bindings[index].name, output);
}

bool ab_audio_bank_name_value(AbAudioBank *bank, AbDataValue requested,
                              AbDataValue *output, AbError *error) {
    if (!bank || !output)
        return ab_fail(error, "Invalid typed audio group-name query");
    return ab_audio_group_name(bank->catalog.groups, requested, bank->config.random,
                               output, error);
}

bool ab_audio_bank_name(AbAudioBank *bank, const char *requested, const char **output,
                        AbError *error) {
    if (!bank || !output)
        return ab_fail(error, "Invalid audio group-name query");
    AbDataValue result;
    if (!ab_audio_bank_name_value(bank, requested ? text(requested) : (AbDataValue){0},
                                  &result, error))
        return false;
    if (result.kind == AB_DATA_NIL) {
        *output = NULL;
        return true;
    }
    if (result.kind != AB_DATA_STRING ||
        (result.string.size && memchr(result.string.data, 0, result.string.size)))
        return ab_fail(error, "Unsupported audio group result string");
    /* Both inputs are terminated: the caller C string or a catalog constant
     * whose encoded terminator the pinned chunk reader has validated. */
    *output = (const char *)result.string.data;
    return true;
}

bool ab_audio_bank_play(AbAudioBank *bank, AbAudioPlay request, uint32_t *handle,
                        AbError *error) {
    if (!bank || !handle)
        return ab_fail(error, "Missing audio bank playback output");
    AbAudioPlay resolved;
    bool play;
    if (!ab_audio_play_resolve(&bank->sounds, &bank->channel_volumes, request,
                               &resolved, &play, error))
        return false;
    if (!play) {
        *handle = UINT32_MAX;
        return true;
    }
    float gain, channel;
    if (resolved.name.kind != AB_DATA_STRING ||
        resolved.volume.kind != AB_DATA_NUMBER ||
        resolved.channel.kind != AB_DATA_NUMBER ||
        resolved.loop.kind != AB_DATA_BOOLEAN)
        return ab_fail(error, "Unsupported native audio playback argument conversion");
    memcpy(&gain, &resolved.volume.bits, sizeof(gain));
    memcpy(&channel, &resolved.channel.bits, sizeof(channel));
    if (!isfinite(gain) || !isfinite(channel) || channel < 0 || channel > 7 ||
        channel != (float)(uint32_t)channel)
        return ab_fail(error, "Invalid audio gain or track");
    return ab_audio_bank_play_resource(bank, resolved.name.string, gain,
                                       resolved.loop.bits != 0, (uint32_t)channel,
                                       handle, error);
}

bool ab_audio_bank_play_resource(AbAudioBank *bank, AbSpan name, float gain, bool loop,
                                 uint32_t track, uint32_t *handle, AbError *error) {
    if (!bank || !handle || (name.size && !name.data))
        return ab_fail(error, "Invalid direct audio resource playback");
    const BankClip *clip = find_clip(bank, name);
    if (clip && clip->pcm.sample_rate != 16000)
        return ab_fail(error, "Unrecovered playback rate for registered audio clip");
    return ab_audio_resources_play(&bank->resources, name, gain, loop, track, handle,
                                   error);
}

bool ab_audio_bank_default_sling_launch(AbAudioBank *bank, uint32_t *handle,
                                        AbError *error) {
    const char *selected = NULL;
    if (!ab_audio_bank_name(bank, "bird_shot", &selected, error))
        return false;
    AbAudioPlay request = {.name = selected ? text(selected) : (AbDataValue){0},
                           .volume = number(1),
                           .loop = {.kind = AB_DATA_BOOLEAN}};
    return ab_audio_bank_play(bank, request, handle, error);
}

bool ab_audio_bank_is_playing(const AbAudioBank *bank, AbSpan name, bool *output,
                              AbError *error) {
    if (!bank)
        return ab_fail(error, "Missing audio bank");
    return ab_audio_resources_is_playing(&bank->resources, name, output, error);
}

bool ab_audio_bank_stop(AbAudioBank *bank, AbSpan name, AbError *error) {
    if (!bank)
        return ab_fail(error, "Missing audio bank");
    return ab_audio_resources_stop(&bank->resources, name, error);
}

bool ab_audio_bank_gain(AbAudioBank *bank, AbSpan name, float gain, AbError *error) {
    if (!bank)
        return ab_fail(error, "Missing audio bank");
    return ab_audio_resources_gain(&bank->resources, name, gain, error);
}
