#include "ab/wave.h"

#include <float.h>
#include <string.h>

_Static_assert(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24,
               "Wave decoding requires binary32 float");

static bool wave_format(AbSpan payload, AbWave *wave, AbError *error) {
    if (payload.size < 16 || ab_u16le(payload.data) != 1 ||
        ab_u16le(payload.data + 14) != 16) {
        return ab_fail(error, "Unsupported or truncated WAVE format");
    }
    uint16_t channels = ab_u16le(payload.data + 2);
    uint32_t rate = ab_u32le(payload.data + 4);
    uint32_t byte_rate = ab_u32le(payload.data + 8);
    uint16_t align = ab_u16le(payload.data + 12);
    if (!channels || channels > 2 || !rate || align != channels * 2 ||
        (uint64_t)rate * align != byte_rate) {
        return ab_fail(error, "Invalid WAVE channels, rate or block alignment");
    }
    wave->channels = channels;
    wave->sample_rate = rate;
    return true;
}

bool ab_wave_read(AbSpan input, AbWave *output, AbError *error) {
    if (!output || !input.data || input.size < 12 || memcmp(input.data, "RIFF", 4) ||
        memcmp(input.data + 8, "WAVE", 4) ||
        (uint64_t)ab_u32le(input.data + 4) + 8 != input.size) {
        return ab_fail(error, "Invalid or truncated RIFF/WAVE input");
    }
    AbWave wave = {0};
    bool format_present = false, data_present = false;
    for (size_t cursor = 12; cursor < input.size;) {
        if (input.size - cursor < 8) {
            return ab_fail(error, "Truncated RIFF chunk header");
        }
        const uint8_t *header = input.data + cursor;
        size_t length = ab_u32le(header + 4);
        size_t payload = cursor + 8;
        size_t padding = length & 1;
        if (length > input.size - payload || padding > input.size - payload - length) {
            return ab_fail(error, "RIFF chunk or padding exceeds input");
        }
        AbSpan bytes = {input.data + payload, length};
        if (!memcmp(header, "fmt ", 4)) {
            if (format_present) {
                return ab_fail(error, "Duplicate WAVE format chunk");
            }
            if (!wave_format(bytes, &wave, error)) {
                return false;
            }
            format_present = true;
        } else if (!memcmp(header, "data", 4)) {
            if (data_present) {
                return ab_fail(error, "Duplicate WAVE data chunk");
            }
            wave.pcm = bytes;
            data_present = true;
        }
        cursor = payload + length + padding;
    }
    if (!format_present || !data_present || wave.pcm.size % (wave.channels * 2)) {
        return ab_fail(error, "Missing WAVE format/data or incomplete sample frame");
    }
    wave.frames = wave.pcm.size / (wave.channels * 2);
    *output = wave;
    return true;
}

bool ab_wave_decode(const AbWave *wave, float *samples, size_t capacity,
                    AbError *error) {
    if (!wave || !wave->channels || wave->channels > 2 || !wave->sample_rate ||
        (wave->pcm.size && !wave->pcm.data) || wave->pcm.size % (wave->channels * 2) ||
        wave->frames != wave->pcm.size / (wave->channels * 2)) {
        return ab_fail(error, "Invalid PCM16 WAVE view");
    }
    size_t count = wave->pcm.size / 2;
    if (count > capacity || (count && !samples)) {
        return ab_fail(error, "PCM16 WAVE output is too small");
    }
    for (size_t index = 0; index < count; ++index) {
        uint16_t bits = ab_u16le(wave->pcm.data + index * 2);
        int32_t value = bits < UINT16_C(0x8000) ? bits : (int32_t)bits - 65536;
        samples[index] = (float)value * 0x1p-15f;
    }
    return true;
}
