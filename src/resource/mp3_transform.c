#include "ab/mp3.h"

#include <math.h>
#include <string.h>

static double bounded_cosine(double angle) {
    double square = angle * angle, term = 1.0, result = 1.0;
    /* Coefficient generation reduces every angle into [0, pi/2]. */
    for (size_t order = 1; order <= 18; ++order) {
        double divisor = (double)((2 * order - 1) * (2 * order));
        term *= -square / divisor;
        result += term;
    }
    return result;
}

static double window_cosine(double angle) {
    const double pi = 3.14159265358979323846264338327950288;
    if (angle > pi) {
        angle = 2.0 * pi - angle;
    }
    return angle > pi * 0.5 ? -bounded_cosine(pi - angle) : bounded_cosine(angle);
}

static double window_sine(double angle) {
    const double pi = 3.14159265358979323846264338327950288;
    if (angle > pi * 0.5) {
        angle = pi - angle;
    }
    double square = angle * angle, term = angle, result = angle;
    for (size_t order = 1; order <= 18; ++order) {
        double divisor = (double)((2 * order) * (2 * order + 1));
        term *= -square / divisor;
        result += term;
    }
    return result;
}

bool ab_mp3_hybrid_windows_init(AbMp3HybridWindows *output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing MP3 hybrid-window storage");
    }
    const double pi = 3.14159265358979323846264338327950288;
    AbMp3HybridWindows windows = {0};
    for (size_t index = 0; index < 36; ++index) {
        double denominator = window_cosine((double)(2 * index + 19) * pi / 72.0);
        double sine = window_sine((double)(2 * index + 1) * (pi / 72.0));
        windows.values[0][0][index] = (float)((sine * 0.5) / denominator);
        if (index < 18) {
            windows.values[0][1][index] = windows.values[0][0][index];
        } else {
            windows.values[0][3][index] = windows.values[0][0][index];
        }
        if (index >= 18 && index < 24) {
            windows.values[0][1][index] = (float)(0.5 / denominator);
        } else if (index >= 24 && index < 30) {
            double short_sine = window_sine((double)(2 * index - 35) * (pi / 24.0));
            windows.values[0][1][index] = (float)((short_sine * 0.5) / denominator);
        }
        if (index >= 6 && index < 12) {
            double short_sine = window_sine((double)(2 * index - 11) * (pi / 24.0));
            windows.values[0][3][index] = (float)((short_sine * 0.5) / denominator);
        } else if (index >= 12 && index < 18) {
            windows.values[0][3][index] = (float)(0.5 / denominator);
        }
        if (index < 12) {
            double short_sine = window_sine((double)(2 * index + 1) * (pi / 24.0));
            double short_cosine = window_cosine((double)(2 * index + 7) * pi / 24.0);
            windows.values[0][2][index] = (float)((short_sine * 0.5) / short_cosine);
        }
    }
    for (size_t block = 0; block < 4; ++block) {
        for (size_t index = 0; index < 36; ++index) {
            float coefficient = windows.values[0][block][index];
            if ((index & 1u) && (block != 2 || index < 12)) {
                coefficient = -coefficient;
            }
            windows.values[1][block][index] = coefficient;
        }
    }
    *output = windows;
    return true;
}

bool ab_mp3_transform_init(AbMp3TransformCoefficients *output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing MP3 long-transform coefficient storage");
    }
    const double pi = 3.14159265358979323846264338327950288;
    AbMp3TransformCoefficients table;
    for (size_t index = 0; index < 9; ++index) {
        table.cosine[index] = (float)bounded_cosine((double)index * pi / 18.0);
        table.half_cosine[index] =
            (float)(1.0 / (2.0 * bounded_cosine((double)(2 * index + 1) * pi / 36.0)));
    }
    *output = table;
    return true;
}

bool ab_mp3_synthesis_transform_init(AbMp3SynthesisTransform *output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing MP3 synthesis-transform coefficient storage");
    }
    const double pi = 3.14159265358979323846264338327950288;
    AbMp3SynthesisTransform table;
    size_t destination = 0;
    for (size_t width = 32; width > 1; width /= 2) {
        for (size_t index = 0; index < width / 2; ++index) {
            double angle = (double)(2 * index + 1) * pi / (double)(2 * width);
            table.factors[destination++] = (float)(1.0 / (2.0 * bounded_cosine(angle)));
        }
    }
    *output = table;
    return true;
}

static void long_components(const AbMp3TransformCoefficients *table,
                            const float input[9], bool odd, float output[9]) {
    float half_six = input[6] * table->cosine[6];
    float baseline = input[0] + half_six;
    float difference = (input[0] - half_six) - half_six;
    float quarter = ((input[8] + input[4]) - input[2]) * table->cosine[6];
    float bridge = input[3] * table->cosine[3];
    float diagonal = ((input[7] + input[5]) - input[1]) * table->cosine[3];
    float sum_a = (input[4] + input[2]) * table->cosine[2];
    float difference_a = (input[4] - input[8]) * -table->cosine[8];
    float sum_b = (input[5] + input[1]) * table->cosine[1];
    float difference_b = (input[5] - input[7]) * -table->cosine[7];
    float cross_a = (input[8] + input[2]) * -table->cosine[4];
    float cross_b = (input[7] + input[1]) * -table->cosine[5];

    /* The target's two branches use distinct associations for these sums.
     * Preserve the measured binary32 operations rather than reassociating
     * their algebraically equivalent nine-point cosine components. */
    float outer_a =
        odd ? (baseline + difference_a) + sum_a : (baseline + sum_a) + difference_a;
    float outer_b = (sum_b + difference_b) + bridge;
    float middle_a = (baseline - sum_a) - cross_a;
    float middle_b =
        odd ? difference_b - (cross_b + bridge) : (difference_b - bridge) - cross_b;
    float inner_a =
        odd ? (cross_a - difference_a) + baseline : (baseline - difference_a) + cross_a;
    float inner_b = odd ? (cross_b - bridge) + sum_b : (sum_b - bridge) + cross_b;
    output[0] = outer_a + outer_b;
    output[1] = (difference - quarter) - diagonal;
    output[2] = middle_a + middle_b;
    output[3] = inner_a + inner_b;
    output[4] = (difference + quarter) + quarter;
    output[5] = inner_a - inner_b;
    output[6] = middle_a - middle_b;
    output[7] = (difference - quarter) + diagonal;
    output[8] = outer_a - outer_b;
}

static bool transform_inputs_finite(const AbMp3TransformCoefficients *table,
                                    const float frequency[18], const float previous[18],
                                    const float *window, size_t window_count) {
    for (size_t index = 0; index < 9; ++index) {
        if (!isfinite(table->cosine[index]) || !isfinite(table->half_cosine[index])) {
            return false;
        }
    }
    for (size_t index = 0; index < 18; ++index) {
        if (!isfinite(frequency[index]) || !isfinite(previous[index])) {
            return false;
        }
    }
    for (size_t index = 0; index < window_count; ++index) {
        if (!isfinite(window[index])) {
            return false;
        }
    }
    return true;
}

bool ab_mp3_long_transform(const AbMp3TransformCoefficients *transform,
                           const float frequency[18], const float previous[18],
                           const float window[36], float samples[18], float next[18],
                           AbError *error) {
    if (!transform || !frequency || !previous || !window || !samples || !next ||
        samples == next ||
        !transform_inputs_finite(transform, frequency, previous, window, 36)) {
        return ab_fail(error, "Invalid MP3 long-transform inputs/storage");
    }
    float adjacent[18], even[9], odd[9], even_output[9], odd_output[9];
    adjacent[0] = frequency[0];
    for (size_t index = 1; index < 18; ++index) {
        adjacent[index] = frequency[index] + frequency[index - 1];
    }
    for (size_t index = 0; index < 9; ++index) {
        even[index] = adjacent[index * 2];
        odd[index] =
            index ? adjacent[index * 2 + 1] + adjacent[index * 2 - 1] : adjacent[1];
    }
    long_components(transform, even, false, even_output);
    long_components(transform, odd, true, odd_output);
    float current[18], following[18];
    for (size_t index = 0; index < 9; ++index) {
        float scaled_odd = odd_output[index] * transform->half_cosine[index];
        float sum = even_output[index] + scaled_odd;
        float difference = even_output[index] - scaled_odd;
        size_t low = 8 - index, high = 9 + index;
        current[low] = previous[low] + difference * window[low];
        current[high] = previous[high] + difference * window[high];
        following[low] = sum * window[18 + low];
        following[high] = sum * window[18 + high];
        if (!isfinite(current[low]) || !isfinite(current[high]) ||
            !isfinite(following[low]) || !isfinite(following[high])) {
            return ab_fail(error, "MP3 long-transform result overflow");
        }
    }
    memcpy(samples, current, sizeof(current));
    memcpy(next, following, sizeof(following));
    return true;
}

static void short_components(const AbMp3TransformCoefficients *table,
                             const float input[6], float output[6]) {
    float pair = input[2] + input[3];
    float outer = (input[4] + input[5]) + pair;
    float first = input[0] + input[1];
    float middle = input[3] + input[4];
    float rotation = (first - outer) * table->half_cosine[4];
    float difference = input[0] - middle;
    float diagonal_a = (pair + first) * table->cosine[3];
    float base_a = first + outer * table->cosine[6];
    float base_b = input[0] + middle * table->cosine[6];
    float diagonal_b = (input[1] + input[2]) * table->cosine[3];
    float high = (base_a - diagonal_a) * table->half_cosine[7];
    float low = (base_a + diagonal_a) * table->half_cosine[1];
    output[0] = (base_b - diagonal_b) - high;
    output[1] = difference - rotation;
    output[2] = (base_b + diagonal_b) - low;
    output[3] = (base_b - diagonal_b) + high;
    output[4] = difference + rotation;
    output[5] = (base_b + diagonal_b) + low;
}

bool ab_mp3_short_transform(const AbMp3TransformCoefficients *transform,
                            const float frequency[18], const float previous[18],
                            const float window[12], float samples[18], float next[18],
                            AbError *error) {
    if (!transform || !frequency || !previous || !window || !samples || !next ||
        samples == next ||
        !transform_inputs_finite(transform, frequency, previous, window, 12)) {
        return ab_fail(error, "Invalid MP3 short-transform inputs/storage");
    }
    static const uint8_t component_order[] = {0, 1, 2, 2, 1, 0, 3, 4, 5, 5, 4, 3};
    float current[18], following[18] = {0};
    memcpy(current, previous, sizeof(current));
    for (size_t part = 0; part < 3; ++part) {
        float input[6], components[6];
        for (size_t index = 0; index < 6; ++index) {
            input[index] = frequency[index * 3 + part];
        }
        short_components(transform, input, components);
        for (size_t index = 0; index < 12; ++index) {
            float term = components[component_order[index]] * window[index];
            size_t position = 6 + part * 6 + index;
            if (position < 18) {
                current[position] += term;
            } else if (index >= 6) {
                following[position - 18] = term;
            } else {
                following[position - 18] += term;
            }
        }
    }
    for (size_t index = 0; index < 18; ++index) {
        if (!isfinite(current[index]) || !isfinite(following[index])) {
            return ab_fail(error, "MP3 short-transform result overflow");
        }
    }
    memcpy(samples, current, sizeof(current));
    memcpy(next, following, sizeof(following));
    return true;
}
