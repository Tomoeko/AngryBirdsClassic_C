#include "ab/mp3.h"

#include <math.h>
#include <string.h>

static void alias_coefficients(float cosine[8], float sine[8]) {
    static const double ratios[] = {-0.6,   -0.535, -0.33,   -0.185,
                                    -0.095, -0.041, -0.0142, -0.0037};
    for (size_t index = 0; index < 8; ++index) {
        double ratio = ratios[index];
        double square = 1.0 + ratio * ratio;
        double root = 1.0;
        /* These eight radicands lie in [1, 1.36]. Eight fixed Newton steps
         * settle the binary64 estimate before the final binary32 conversion. */
        for (size_t iteration = 0; iteration < 8; ++iteration) {
            root = (root + square / root) * 0.5;
        }
        cosine[index] = (float)(1.0 / root);
        sine[index] = (float)(ratio / root);
    }
}

bool ab_mp3_alias_spectrum(const AbMp3Granule *granule, uint8_t active_subbands,
                           float frequency[576], AbError *error) {
    if (!granule || !frequency || granule->block_type > 3 || active_subbands == 0 ||
        active_subbands > 32 || granule->window_switch != (granule->block_type != 0) ||
        (granule->block_type == 2 && granule->mixed_block)) {
        return ab_fail(error, "Invalid or unsupported MP3 alias-reduction layout");
    }
    for (size_t index = 0; index < 576; ++index) {
        if (!isfinite(frequency[index])) {
            return ab_fail(error, "Nonfinite MP3 alias-reduction input");
        }
    }
    if (granule->block_type == 2) {
        return true;
    }
    float cosine[8], sine[8], values[576];
    alias_coefficients(cosine, sine);
    memcpy(values, frequency, sizeof(values));
    for (size_t boundary = 18; boundary < (size_t)active_subbands * 18;
         boundary += 18) {
        for (size_t index = 0; index < 8; ++index) {
            float previous = values[boundary - 1 - index];
            float next = values[boundary + index];
            float previous_cosine = previous * cosine[index];
            float next_sine = next * sine[index];
            float previous_sine = previous * sine[index];
            float next_cosine = next * cosine[index];
            values[boundary - 1 - index] = previous_cosine - next_sine;
            values[boundary + index] = previous_sine + next_cosine;
            if (!isfinite(values[boundary - 1 - index]) ||
                !isfinite(values[boundary + index])) {
                return ab_fail(error, "MP3 alias-reduction result overflow");
            }
        }
    }
    memcpy(frequency, values, sizeof(values));
    return true;
}
