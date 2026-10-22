#include "ab/audio_resources.h"

#include <string.h>

static int compare_names(AbSpan first, AbSpan second) {
    size_t common = first.size < second.size ? first.size : second.size;
    int order = common ? memcmp(first.data, second.data, common) : 0;
    return order ? order : (first.size > second.size) - (first.size < second.size);
}

bool ab_audio_resources_bind(AbAudioMixer *mixer, const AbAudioClipBinding *clips,
                             size_t count, AbAudioResources *output, AbError *error) {
    if (!output || (count && !clips) || count > UINT32_C(16777216)) {
        return ab_fail(error, "Invalid named PCM resource map");
    }
    for (size_t index = 0; index < count; ++index) {
        const AbAudioClipBinding *clip = clips + index;
        if ((clip->name.size && !clip->name.data) || !clip->pcm.samples ||
            !clip->pcm.frames || clip->pcm.channels < 1 || clip->pcm.channels > 2 ||
            clip->pcm.frames > (size_t)INT32_MAX / (clip->pcm.channels * 2u) ||
            (index && compare_names(clips[index - 1].name, clip->name) >= 0)) {
            return ab_fail(error, "Invalid or unordered named PCM binding at %zu",
                           index);
        }
    }
    *output = (AbAudioResources){mixer, clips, count};
    return true;
}

static bool valid_request(const AbAudioResources *resources, AbSpan name,
                          AbError *error) {
    if (!resources || (resources->count && !resources->clips) ||
        resources->count > UINT32_C(16777216) || (name.size && !name.data)) {
        return ab_fail(error, "Invalid named PCM resource request");
    }
    return true;
}

static const AbAudioClipBinding *find_clip(const AbAudioResources *resources,
                                           AbSpan name) {
    size_t begin = 0, end = resources->count;
    while (begin < end) {
        size_t middle = begin + (end - begin) / 2;
        const AbAudioClipBinding *clip = resources->clips + middle;
        int order = compare_names(name, clip->name);
        if (!order) {
            return clip;
        }
        if (order < 0) {
            end = middle;
        } else {
            begin = middle + 1;
        }
    }
    return NULL;
}

bool ab_audio_resources_play(AbAudioResources *resources, AbSpan name, float gain,
                             bool loop, uint32_t track, uint32_t *handle,
                             AbError *error) {
    if (!valid_request(resources, name, error)) {
        return false;
    }
    if (!handle || !resources->mixer) {
        return ab_fail(error, "Missing named PCM output or playback handle");
    }
    const AbAudioClipBinding *clip = find_clip(resources, name);
    if (!clip) {
        *handle = UINT32_MAX;
        return true;
    }
    return ab_audio_mixer_play(resources->mixer, clip->clip_id, clip->pcm, gain, loop,
                               track, handle, error);
}

bool ab_audio_resources_stop(AbAudioResources *resources, AbSpan name, AbError *error) {
    if (!valid_request(resources, name, error)) {
        return false;
    }
    if (!resources->mixer) {
        return ab_fail(error, "Missing named PCM output");
    }
    const AbAudioClipBinding *clip = find_clip(resources, name);
    return !clip || ab_audio_mixer_clip_action(resources->mixer, clip->clip_id,
                                               AB_AUDIO_STOP, error);
}

bool ab_audio_resources_gain(AbAudioResources *resources, AbSpan name, float gain,
                             AbError *error) {
    if (!valid_request(resources, name, error)) {
        return false;
    }
    if (!resources->mixer) {
        return ab_fail(error, "Missing named PCM output");
    }
    const AbAudioClipBinding *clip = find_clip(resources, name);
    return !clip ||
           ab_audio_mixer_clip_gain(resources->mixer, clip->clip_id, gain, error);
}

bool ab_audio_resources_is_playing(const AbAudioResources *resources, AbSpan name,
                                   bool *output, AbError *error) {
    if (!valid_request(resources, name, error)) {
        return false;
    }
    if (!output) {
        return ab_fail(error, "Missing named PCM playing output");
    }
    const AbAudioClipBinding *clip =
        resources->mixer ? find_clip(resources, name) : NULL;
    *output = clip && ab_audio_mixer_clip_is_playing(resources->mixer, clip->clip_id);
    return true;
}
