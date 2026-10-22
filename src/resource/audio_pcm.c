#include "ab/audio_pcm.h"
#include "ab/mp3.h"
#include "ab/wave.h"

#include <stdlib.h>
#include <string.h>

static bool decode_wave(AbSpan input, size_t max_frames, AbPcm16 *output,
                        AbError *error) {
    AbWave wave;
    if (!ab_wave_read(input, &wave, error)) {
        return false;
    }
    if (wave.frames > max_frames ||
        wave.frames > SIZE_MAX / (sizeof(int16_t) * wave.channels)) {
        return ab_fail(error, "Decoded WAVE exceeds PCM allocation bound");
    }
    AbPcm16 result = {.frames = wave.frames,
                      .sample_rate = wave.sample_rate,
                      .channels = wave.channels};
    size_t count = wave.frames * wave.channels;
    if (count) {
        result.samples = malloc(count * sizeof(int16_t));
        if (!result.samples) {
            return ab_fail(error, "Unable to allocate decoded WAVE samples");
        }
    }
    for (size_t index = 0; index < count; ++index) {
        uint16_t encoded = ab_u16le(wave.pcm.data + index * 2);
        int32_t value = encoded;
        if (encoded > INT16_MAX) {
            value -= 65536;
        }
        result.samples[index] = (int16_t)value;
    }
    *output = result;
    return true;
}

bool ab_audio_pcm_decode(AbSpan input, size_t max_frames, AbPcm16 *output,
                         AbError *error) {
    if (!output || !input.data || !input.size) {
        return ab_fail(error, "Missing encoded audio input or PCM output");
    }
    if (input.size >= 4 && memcmp(input.data, "RIFF", 4) == 0) {
        return decode_wave(input, max_frames, output, error);
    }
    return ab_mp3_decode(input, max_frames, output, error);
}

bool ab_audio_pcm_load(const AbZip *archive, const char *path, size_t max_input_bytes,
                       size_t max_frames, AbPcm16 *output, AbError *error) {
    if (!archive || !path || !output) {
        return ab_fail(error, "Missing audio archive, path or PCM output");
    }
    const AbZipEntry *entry = ab_zip_find(archive, path);
    if (!entry) {
        return ab_fail(error, "Audio resource path is absent from archive");
    }
    AbBuffer encoded = {0};
    if (!ab_zip_read(archive, (size_t)(entry - archive->entries), max_input_bytes,
                     &encoded, error)) {
        return false;
    }
    bool okay = ab_audio_pcm_decode((AbSpan){encoded.data, encoded.size}, max_frames,
                                    output, error);
    ab_buffer_free(&encoded);
    return okay;
}
