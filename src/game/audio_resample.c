#include "ab/audio_resample.h"

#include <math.h>
#include <string.h>

bool ab_audio_resampler_init(AbAudioResampler *output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing audio interpolation storage");
    memset(output, 0, sizeof(*output));
    const double pi = 3.14159265358979323846264338327950288;
    for (unsigned ratio = 2; ratio <= 3; ++ratio) {
        output->weights[ratio - 2][0][31] = 1;
        for (unsigned phase = 1; phase < ratio; ++phase) {
            double weights[AB_AUDIO_RESAMPLE_TAPS] = {0}, sum = 0;
            for (unsigned tap = 0; tap < AB_AUDIO_RESAMPLE_TAPS; ++tap) {
                double distance = (double)tap - 31 - (double)phase / ratio;
                double window = .42 + .5 * cos(pi * distance / 32) +
                                .08 * cos(2 * pi * distance / 32);
                weights[tap] = (sin(pi * distance) / (pi * distance)) * window;
                sum += weights[tap];
            }
            if (!isfinite(sum) || sum <= 0)
                return ab_fail(error, "Invalid audio interpolation normalization");
            for (unsigned tap = 0; tap < AB_AUDIO_RESAMPLE_TAPS; ++tap)
                output->weights[ratio - 2][phase][tap] = (float)(weights[tap] / sum);
        }
    }
    return true;
}

bool ab_audio_resampler_sample(const AbAudioResampler *resampler, AbPcm16View source,
                               size_t frame, unsigned phase, unsigned ratio,
                               unsigned channel, bool loop, int16_t *output,
                               AbError *error) {
    if (!resampler || !output || !source.samples || !source.frames ||
        source.frames > INT32_MAX || source.channels < 1 || source.channels > 2 ||
        frame >= source.frames || channel >= source.channels || ratio < 2 ||
        ratio > 3 || phase >= ratio)
        return ab_fail(error, "Invalid audio interpolation sample");
    if (!phase) {
        *output = source.samples[frame * source.channels + channel];
        return true;
    }
    double value = 0;
    for (unsigned tap = 0; tap < AB_AUDIO_RESAMPLE_TAPS; ++tap) {
        int64_t position = (int64_t)frame + (int64_t)tap - 31;
        if (loop) {
            position %= (int64_t)source.frames;
            if (position < 0)
                position += (int64_t)source.frames;
        } else if (position < 0 || position >= (int64_t)source.frames) {
            continue;
        }
        size_t index = (size_t)position * source.channels + channel;
        value +=
            (double)source.samples[index] * resampler->weights[ratio - 2][phase][tap];
    }
    if (!isfinite(value))
        return ab_fail(error, "Nonfinite interpolated audio sample");
    value = value < 0 ? ceil(value - .5) : floor(value + .5);
    *output = value >= INT16_MAX   ? INT16_MAX
              : value <= INT16_MIN ? INT16_MIN
                                   : (int16_t)value;
    return true;
}
