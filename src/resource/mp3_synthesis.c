#include "ab/mp3.h"

#include <float.h>
#include <math.h>
#include <string.h>

/* Quantized codec-window parameters recovered from the pinned APK. Native
 * observations and parameter provenance are recorded in docs/evidence.md. */
static const int32_t window_parameters[257] = {
    0,     -1,    -1,    -1,    -1,    -1,    -1,    -2,    -2,    -2,    -2,    -3,
    -3,    -4,    -4,    -5,    -5,    -6,    -7,    -7,    -8,    -9,    -10,   -11,
    -13,   -14,   -16,   -17,   -19,   -21,   -24,   -26,   -29,   -31,   -35,   -38,
    -41,   -45,   -49,   -53,   -58,   -63,   -68,   -73,   -79,   -85,   -91,   -97,
    -104,  -111,  -117,  -125,  -132,  -139,  -147,  -154,  -161,  -169,  -176,  -183,
    -190,  -196,  -202,  -208,  -213,  -218,  -222,  -225,  -227,  -228,  -228,  -227,
    -224,  -221,  -215,  -208,  -200,  -189,  -177,  -163,  -146,  -127,  -106,  -83,
    -57,   -29,   2,     36,    72,    111,   153,   197,   244,   294,   347,   401,
    459,   519,   581,   645,   711,   779,   848,   919,   991,   1064,  1137,  1210,
    1283,  1356,  1428,  1498,  1567,  1634,  1698,  1759,  1817,  1870,  1919,  1962,
    2001,  2032,  2057,  2075,  2085,  2087,  2080,  2063,  2037,  2000,  1952,  1893,
    1822,  1739,  1644,  1535,  1414,  1280,  1131,  970,   794,   605,   402,   185,
    -45,   -288,  -545,  -814,  -1095, -1388, -1692, -2006, -2330, -2663, -3004, -3351,
    -3705, -4063, -4425, -4788, -5153, -5517, -5879, -6237, -6589, -6935, -7271, -7597,
    -7910, -8209, -8491, -8755, -8998, -9219, -9416, -9585, -9727, -9838, -9916, -9959,
    -9966, -9935, -9863, -9750, -9592, -9389, -9139, -8840, -8492, -8092, -7640, -7134,
    -6574, -5959, -5288, -4561, -3776, -2935, -2037, -1082, -70,   998,   2122,  3300,
    4533,  5818,  7154,  8540,  9975,  11455, 12980, 14548, 16155, 17799, 19478, 21189,
    22929, 24694, 26482, 28289, 30112, 31947, 33791, 35640, 37489, 39336, 41176, 43006,
    44821, 46617, 48390, 50137, 51853, 53534, 55178, 56778, 58333, 59838, 61289, 62684,
    64019, 65290, 66494, 67629, 68692, 69679, 70590, 71420, 72169, 72835, 73415, 73908,
    74313, 74630, 74856, 74992, 75038,
};

bool ab_mp3_synthesis_window_init(double gain, AbMp3SynthesisWindow *output,
                                  AbError *error) {
    if (!output || !isfinite(gain) || gain < 0.0) {
        return ab_fail(error, "Invalid MP3 synthesis-window gain/storage");
    }
    AbMp3SynthesisWindow result = {0};
    double scale = -0.5 * gain;
    int position = 0;
    for (size_t index = 0; index < 512; ++index) {
        if (position < 528) {
            size_t parameter = index < 256 ? index : 512 - index;
            double expanded = (double)window_parameters[parameter] * scale;
            if (!isfinite(expanded) || expanded < -FLT_MAX || expanded > FLT_MAX) {
                return ab_fail(error, "MP3 synthesis-window coefficient overflow");
            }
            float value = (float)expanded;
            result.values[position] = result.values[position + 16] = value;
        }
        if ((index & 31u) == 31) {
            position -= 1023;
        }
        position += 32;
        if ((index & 63u) == 63) {
            scale = -scale;
        }
    }
    *output = result;
    return true;
}

bool ab_mp3_synthesis_history_init(AbMp3SynthesisHistory *output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing MP3 synthesis history storage");
    }
    memset(output, 0, sizeof(*output));
    output->phase = 1;
    return true;
}

static void synthesis_cosine(const AbMp3SynthesisTransform *table, const float *input,
                             size_t width, float *output) {
    if (width == 1) {
        output[0] = input[0];
        return;
    }
    size_t half = width / 2;
    float even[16], odd[16], even_result[16], odd_result[16];
    for (size_t index = 0; index < half; ++index) {
        even[index] = input[index] + input[width - 1 - index];
        float difference = input[index] - input[width - 1 - index];
        odd[index] = difference * table->factors[32 - width + index];
    }
    synthesis_cosine(table, even, half, even_result);
    synthesis_cosine(table, odd, half, odd_result);
    for (size_t index = 0; index < half; ++index) {
        output[index * 2] = even_result[index];
        output[index * 2 + 1] = index + 1 < half
                                    ? odd_result[index] + odd_result[index + 1]
                                    : odd_result[index];
    }
}

bool ab_mp3_synthesis_transform(const AbMp3SynthesisTransform *transform,
                                const float frequency[32], float output[32],
                                AbError *error) {
    if (!transform || !frequency || !output) {
        return ab_fail(error, "Missing MP3 synthesis-transform inputs/storage");
    }
    for (size_t index = 0; index < 31; ++index) {
        if (!isfinite(transform->factors[index])) {
            return ab_fail(error, "Nonfinite MP3 synthesis-transform coefficient");
        }
    }
    for (size_t index = 0; index < 32; ++index) {
        if (!isfinite(frequency[index])) {
            return ab_fail(error, "Nonfinite MP3 synthesis-transform input");
        }
    }
    float coefficients[32];
    synthesis_cosine(transform, frequency, 32, coefficients);
    for (size_t index = 0; index < 32; ++index) {
        if (!isfinite(coefficients[index])) {
            return ab_fail(error, "MP3 synthesis-transform result overflow");
        }
    }
    memcpy(output, coefficients, sizeof(coefficients));
    return true;
}

/* Keep each binary32 product and sum separate, in the observed order. This
 * preserves signed zeros and integer conversion at sample boundaries. */
static float forward_sample(const float *history, const float *window) {
    float product = history[1] * window[1];
    float total = -product;
    product = history[0] * window[0];
    total += product;
    for (size_t index = 2; index < 16; ++index) {
        product = history[index] * window[index];
        total = (index & 1u) ? total - product : total + product;
    }
    return total;
}

static float center_sample(const float *history, const float *window) {
    float product = history[2] * window[2];
    float total = product;
    product = history[0] * window[0];
    total += product;
    for (size_t index = 4; index < 16; index += 2) {
        product = history[index] * window[index];
        total += product;
    }
    return total;
}

static float reverse_sample(const float *history, const float *window) {
    float product = history[1] * window[-1];
    float total = -product;
    product = history[0] * window[0];
    total -= product;
    for (size_t index = 2; index < 16; ++index) {
        product = history[index] * window[-(ptrdiff_t)index];
        total -= product;
    }
    return total;
}

bool ab_mp3_synthesize(const AbMp3SynthesisTransform *transform,
                       const AbMp3SynthesisWindow *window, const float frequency[32],
                       AbMp3SynthesisHistory *history, int16_t output[32],
                       uint8_t *clipped, AbError *error) {
    if (!window || !history || !output || !clipped || history->phase > 15) {
        return ab_fail(error, "Invalid MP3 synthesis window/history/storage");
    }
    for (size_t index = 0; index < 544; ++index) {
        if (!isfinite(window->values[index]) ||
            !isfinite(history->values[index / 272][index % 272])) {
            return ab_fail(error, "Nonfinite MP3 synthesis window/history");
        }
    }
    float coefficients[32];
    if (!ab_mp3_synthesis_transform(transform, frequency, coefficients, error)) {
        return false;
    }
    AbMp3SynthesisHistory following = *history;
    following.phase = (uint8_t)((following.phase + 15u) & 15u);
    size_t phase = following.phase;
    size_t active = (phase & 1u) ? 0 : 1;
    size_t first = 1 - active;
    size_t first_offset = (phase & 1u) ? (phase + 1u) & 15u : phase;
    size_t second_offset = (phase & 1u) ? phase : phase + 1;
    for (size_t index = 0; index <= 16; ++index) {
        following.values[first][(16 - index) * 16 + first_offset] = coefficients[index];
    }
    for (size_t index = 0; index < 16; ++index) {
        following.values[active][index * 16 + second_offset] = coefficients[16 + index];
    }
    size_t window_phase = phase | 1u;
    const float *retained = following.values[active];
    float samples[32];
    for (size_t index = 0; index < 16; ++index) {
        samples[index] = forward_sample(
            retained + index * 16, window->values + 16 - window_phase + index * 32);
    }
    samples[16] = center_sample(retained + 256, window->values + 528 - window_phase);
    for (size_t index = 0; index < 15; ++index) {
        samples[17 + index] =
            reverse_sample(retained + 240 - index * 16,
                           window->values + 495 + window_phase - index * 32);
    }
    int16_t result[32];
    uint8_t clamped = 0;
    for (size_t index = 0; index < 32; ++index) {
        if (!isfinite(samples[index])) {
            return ab_fail(error, "MP3 synthesis sample overflow");
        }
        if (samples[index] > 32767.0f) {
            result[index] = INT16_MAX;
            ++clamped;
        } else if (samples[index] < -32768.0f) {
            result[index] = INT16_MIN;
            ++clamped;
        } else {
            result[index] = (int16_t)samples[index];
        }
    }
    *history = following;
    memcpy(output, result, sizeof(result));
    *clipped = clamped;
    return true;
}
