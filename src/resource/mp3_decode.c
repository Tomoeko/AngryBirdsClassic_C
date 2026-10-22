#include "ab/mp3.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    AbMp3TransformCoefficients transform;
    AbMp3HybridWindows hybrid_windows;
    AbMp3SynthesisTransform synthesis_transform;
    AbMp3SynthesisWindow synthesis_window;
    AbMp3HybridOverlap overlap[2];
    AbMp3SynthesisHistory history[2];
    AbMp3Reservoir reservoir;
} Mp3Decoder;

static bool init_decoder(Mp3Decoder *decoder, AbError *error) {
    memset(decoder, 0, sizeof(*decoder));
    return ab_mp3_transform_init(&decoder->transform, error) &&
           ab_mp3_hybrid_windows_init(&decoder->hybrid_windows, error) &&
           ab_mp3_synthesis_transform_init(&decoder->synthesis_transform, error) &&
           ab_mp3_synthesis_window_init(1.0, &decoder->synthesis_window, error) &&
           ab_mp3_synthesis_history_init(decoder->history, error) &&
           ab_mp3_synthesis_history_init(decoder->history + 1, error);
}

static bool frame_spectra(const AbMp3FrameInfo *frame, AbSpan main_data,
                          float frequency[2][576], AbMp3SpectralLimits limits[2],
                          AbError *error) {
    bool joint = frame->channels == 2 && ((frame->header >> 6) & 3u) == 1;
    bool mid_side = joint && (frame->header & 32u) != 0;
    if (joint && (frame->header & 16u)) {
        return ab_fail(error,
                       "MP3 intensity-stereo reconstruction remains unsupported");
    }
    size_t bit_offset = 0;
    for (size_t channel = 0; channel < frame->channels; ++channel) {
        const AbMp3Granule *granule = frame->side.channel + channel;
        AbMp3Scalefactors scales;
        int16_t quantized[576];
        if (!ab_mp3_scalefactors(granule, false, main_data, bit_offset, &scales,
                                 error) ||
            !ab_mp3_quantized_spectrum(granule, frame->sample_rate, main_data,
                                       bit_offset, scales.bit_count, quantized,
                                       error) ||
            !ab_mp3_requantize_spectrum(granule, &scales, frame->sample_rate, mid_side,
                                        quantized, frequency[channel], error) ||
            !ab_mp3_spectral_limits(granule, frame->sample_rate, quantized,
                                    limits + channel, error)) {
            return false;
        }
        bit_offset += granule->part23_length;
    }
    if (mid_side) {
        uint8_t shared = limits[0].active_subbands > limits[1].active_subbands
                             ? limits[0].active_subbands
                             : limits[1].active_subbands;
        if (!ab_mp3_mid_side_spectrum(shared, frequency[0], frequency[1], error)) {
            return false;
        }
        limits[0].active_subbands = limits[1].active_subbands = shared;
    }
    return true;
}

static bool decode_frame(Mp3Decoder *decoder, const AbMp3FrameInfo *frame,
                         AbSpan main_data, uint64_t raw_offset, uint64_t first_sample,
                         AbPcm16 *output, AbError *error) {
    float frequency[2][576], matrix[2][576];
    AbMp3SpectralLimits limits[2];
    if (!frame_spectra(frame, main_data, frequency, limits, error)) {
        return false;
    }
    for (size_t channel = 0; channel < frame->channels; ++channel) {
        if (!ab_mp3_alias_spectrum(frame->side.channel + channel,
                                   limits[channel].active_subbands, frequency[channel],
                                   error) ||
            !ab_mp3_hybrid_channel(&decoder->transform, &decoder->hybrid_windows,
                                   frame->side.channel + channel,
                                   limits[channel].active_subbands, frequency[channel],
                                   decoder->overlap + channel, matrix[channel],
                                   error)) {
            return false;
        }
    }
    for (size_t slot = 0; slot < 18; ++slot) {
        for (size_t channel = 0; channel < frame->channels; ++channel) {
            int16_t pcm[32];
            uint8_t clipped;
            if (!ab_mp3_synthesize(&decoder->synthesis_transform,
                                   &decoder->synthesis_window,
                                   matrix[channel] + slot * 32,
                                   decoder->history + channel, pcm, &clipped, error)) {
                return false;
            }
            for (size_t index = 0; index < 32; ++index) {
                uint64_t sample = raw_offset + slot * 32 + index;
                if (sample >= first_sample && sample - first_sample < output->frames) {
                    size_t position = (size_t)(sample - first_sample);
                    output->samples[position * frame->channels + channel] = pcm[index];
                }
            }
        }
    }
    return true;
}

bool ab_mp3_decode(AbSpan input, size_t max_frames, AbPcm16 *output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing MP3 PCM output storage");
    }
    AbMp3 stream;
    if (!ab_mp3_read(input, &stream, error)) {
        return false;
    }
    if (stream.sample_frames > max_frames ||
        stream.sample_frames > SIZE_MAX / (sizeof(int16_t) * stream.channels) ||
        stream.frame_count > UINT64_MAX / 576) {
        return ab_fail(error, "Decoded MP3 sample count exceeds allocation bound");
    }
    uint64_t first_sample = 0;
    if (stream.has_lame) {
        if (stream.encoder_padding < 529) {
            return ab_fail(error, "MP3 LAME padding below recovered synthesis delay");
        }
        first_sample = (uint64_t)stream.encoder_delay + 529;
    }
    Mp3Decoder decoder;
    if (!init_decoder(&decoder, error)) {
        return false;
    }
    AbPcm16 result = {.frames = (size_t)stream.sample_frames,
                      .sample_rate = stream.sample_rate,
                      .channels = stream.channels};
    if (result.frames) {
        result.samples = malloc(result.frames * result.channels * sizeof(int16_t));
        if (!result.samples) {
            return ab_fail(error, "Unable to allocate decoded MP3 samples");
        }
    }
    size_t offset = 0, parsed = 0;
    uint64_t raw_offset = 0;
    bool okay = false;
    while (offset < stream.frames.size) {
        AbMp3FrameInfo frame;
        AbSpan window;
        uint8_t main_data[1024];
        if (!ab_mp3_frame_info(
                (AbSpan){stream.frames.data + offset, stream.frames.size - offset},
                &frame, error) ||
            !ab_mp3_reservoir_push(&decoder.reservoir, &frame, main_data,
                                   sizeof(main_data), &window, error)) {
            goto release_pcm;
        }
        if (parsed || !stream.has_xing) {
            if (!decode_frame(&decoder, &frame, window, raw_offset, first_sample,
                              &result, error)) {
                goto release_pcm;
            }
            raw_offset += 576;
        }
        offset += frame.frame.size;
        ++parsed;
    }
    if (raw_offset < first_sample || raw_offset - first_sample < result.frames) {
        ab_fail(error, "Decoded MP3 contains fewer samples than its trim metadata");
        goto release_pcm;
    }
    *output = result;
    result.samples = NULL;
    okay = true;

release_pcm:
    ab_pcm16_free(&result);
    return okay;
}
