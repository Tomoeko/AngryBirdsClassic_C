#include "ab/mp3.h"

#include <math.h>
#include <string.h>

bool ab_mp3_hybrid_channel(const AbMp3TransformCoefficients *transform,
                           const AbMp3HybridWindows *windows,
                           const AbMp3Granule *granule, uint8_t active_subbands,
                           const float frequency[576], AbMp3HybridOverlap *overlap,
                           float output[576], AbError *error) {
    if (!transform || !windows || !granule || !frequency || !overlap || !output ||
        output == overlap->values || granule->block_type > 3 ||
        granule->window_switch != (granule->block_type != 0) ||
        (granule->block_type == 2 && granule->mixed_block) || active_subbands == 0 ||
        active_subbands > 32) {
        return ab_fail(error, "Invalid or unsupported MP3 hybrid layout/storage");
    }
    for (size_t index = 0; index < 576; ++index) {
        if (!isfinite(frequency[index]) || !isfinite(overlap->values[index])) {
            return ab_fail(error, "Nonfinite MP3 hybrid spectrum/overlap");
        }
    }
    size_t active = ((size_t)active_subbands + 1u) & ~(size_t)1u;
    float matrix[576];
    AbMp3HybridOverlap following = {0};
    for (size_t subband = 0; subband < 32; ++subband) {
        size_t offset = subband * 18;
        const float *previous = overlap->values + offset;
        float samples[18];
        if (subband < active) {
            const float *window = windows->values[subband & 1u][granule->block_type];
            bool okay = granule->block_type == 2
                            ? ab_mp3_short_transform(transform, frequency + offset,
                                                     previous, window, samples,
                                                     following.values + offset, error)
                            : ab_mp3_long_transform(transform, frequency + offset,
                                                    previous, window, samples,
                                                    following.values + offset, error);
            if (!okay) {
                return false;
            }
        } else {
            memcpy(samples, previous, sizeof(samples));
        }
        for (size_t slot = 0; slot < 18; ++slot) {
            matrix[slot * 32 + subband] = samples[slot];
        }
    }
    *overlap = following;
    memcpy(output, matrix, sizeof(matrix));
    return true;
}
