#ifndef AB_EXTRA_DRAW_CASES_H
#define AB_EXTRA_DRAW_CASES_H

#include <stdbool.h>
#include <stdint.h>

enum { AB_EXTRA_DRAW_CASES = 256, AB_EXTRA_DRAW_MAXIMUM = 16 };

typedef struct {
    uint32_t count, remove;
    float z[AB_EXTRA_DRAW_MAXIMUM];
    bool z_present[AB_EXTRA_DRAW_MAXIMUM], before[AB_EXTRA_DRAW_MAXIMUM];
} ExtraDrawCase;

static ExtraDrawCase extra_draw_case(uint32_t sample) {
    ExtraDrawCase result = {.count = sample % (AB_EXTRA_DRAW_MAXIMUM + 1)};
    if (result.count && (sample & 1))
        result.remove = sample % result.count + 1;
    for (uint32_t index = 0; index < result.count; ++index) {
        result.z_present[index] = ((sample + index) % 4) != 0;
        result.before[index] = ((sample + index * 3) % 3) == 0;
        switch ((sample / (AB_EXTRA_DRAW_MAXIMUM + 1)) % 5) {
            case 0:
                result.z[index] = 1;
                break;
            case 1:
                result.z[index] = (float)((int32_t)(index % 8) - 3);
                break;
            case 2:
                result.z[index] = index & 1 ? -0.0f : 0.0f;
                break;
            case 3:
                result.z[index] = (float)(result.count - index) / 2;
                break;
            default:
                result.z[index] = index & 1 ? 2e10f : -1;
                break;
        }
    }
    return result;
}

static void extra_draw_name(uint32_t id, char output[9]) {
    const char prefix[] = "extra_";
    for (unsigned index = 0; index < 6; ++index)
        output[index] = prefix[index];
    output[6] = (char)('0' + id / 10);
    output[7] = (char)('0' + id % 10);
    output[8] = 0;
}

#endif
