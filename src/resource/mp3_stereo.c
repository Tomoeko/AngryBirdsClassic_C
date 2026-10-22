#include "ab/mp3.h"

#include <math.h>
#include <string.h>

bool ab_mp3_mid_side_spectrum(uint8_t active_subbands, float left[576],
                              float right[576], AbError *error) {
    if (!left || !right || left == right || active_subbands == 0 ||
        active_subbands > 32) {
        return ab_fail(error, "Invalid MP3 mid/side spectral storage");
    }
    float output_left[576], output_right[576];
    for (size_t index = 0; index < 576; ++index) {
        float middle = left[index], side = right[index];
        if (!isfinite(middle) || !isfinite(side)) {
            return ab_fail(error, "Nonfinite MP3 mid/side spectral input");
        }
        output_left[index] =
            index < (size_t)active_subbands * 18 ? middle + side : middle;
        output_right[index] =
            index < (size_t)active_subbands * 18 ? middle - side : side;
        if (!isfinite(output_left[index]) || !isfinite(output_right[index])) {
            return ab_fail(error, "MP3 mid/side spectral result overflow");
        }
    }
    memcpy(left, output_left, sizeof(output_left));
    memcpy(right, output_right, sizeof(output_right));
    return true;
}
