#include "ab/frame_clock.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool rejects_unchanged(const AbFrameClock *clock, const AbFrameClockInput *input,
                              uint32_t limit) {
    AbFrameClockPlan plan;
    unsigned char expected[sizeof(plan)];
    memset(&plan, 0x5a, sizeof(plan));
    memcpy(expected, &plan, sizeof(plan));
    return !ab_frame_clock_advance(clock, input, limit, &plan) &&
           memcmp(expected, &plan, sizeof(plan)) == 0;
}

static bool edges(void) {
    static const uint32_t app_inputs[] = {
        0,          0x80000000, 0x3dcccccc, 0x3dcccccd, 0x3dccccce, 0x3f800000,
        0xbf800000, 0x7f800000, 0xff800000, 0x7fc00001, 0xffc00123};
    static const uint32_t app_outputs[] = {
        0,          0x80000000, 0x3dcccccc, 0x3dcccccd, 0x3dcccccd, 0x3dcccccd,
        0xbf800000, 0x3dcccccd, 0xff800000, 0x3dcccccd, 0x3dcccccd};
    for (size_t index = 0; index < sizeof(app_inputs) / sizeof(*app_inputs); ++index) {
        if (float_bits(ab_app_frame_seconds(from_bits(app_inputs[index]))) !=
            app_outputs[index]) {
            return false;
        }
    }
    AbFrameClock clock = {0};
    AbFrameClockInput input = {.seconds = 0.1f, .physics_enabled = true};
    AbFrameClockPlan plan;
    if (!ab_frame_clock_advance(&clock, &input, 4, &plan) || plan.steps != 2 ||
        plan.history_updates != 2 ||
        float_bits(plan.next.accumulator) != UINT32_C(0x3d088887) ||
        !plan.interpolation_present) {
        return false;
    }
    clock = plan.next;
    if (!ab_frame_clock_advance(&clock, &input, 4, &plan) || plan.steps != 3 ||
        plan.history_updates != 2 ||
        float_bits(plan.next.accumulator) != UINT32_C(0x3d088887)) {
        return false;
    }
    input = (AbFrameClockInput){.seconds = 0.5f, .speed_factor = NAN};
    if (!ab_frame_clock_advance(&clock, &input, 0, &plan) || plan.steps ||
        plan.history_updates || plan.interpolation_present ||
        plan.next.accumulator != clock.accumulator ||
        plan.next.time != clock.time + input.seconds) {
        return false;
    }
    clock = (AbFrameClock){0};
    input = (AbFrameClockInput){.seconds = from_bits(UINT32_C(0x3d088889)),
                                .physics_enabled = true};
    if (!rejects_unchanged(&clock, &input, 0) ||
        !ab_frame_clock_advance(&clock, &input, 1, &plan) || plan.steps != 1 ||
        float_bits(plan.next.accumulator) != 0 || plan.interpolation != 0) {
        return false;
    }
    input.speed_factor_present = true;
    input.speed_factor = 0;
    if (!ab_frame_clock_advance(&clock, &input, 0, &plan) || plan.steps ||
        plan.next.time != input.seconds || plan.physics_seconds != 0) {
        return false;
    }
    input.speed_factor = NAN;
    if (!rejects_unchanged(&clock, &input, 1)) {
        return false;
    }
    input.speed_factor_present = false;
    clock.accumulator = FLT_MAX;
    if (!rejects_unchanged(&clock, &input, 4)) {
        return false;
    }
    clock = (AbFrameClock){.time = FLT_MAX};
    input.seconds = FLT_MAX;
    return rejects_unchanged(&clock, &input, 4) && rejects_unchanged(NULL, &input, 4) &&
           rejects_unchanged(&clock, NULL, 4) &&
           rejects_unchanged(&clock, &input, 4097) &&
           !ab_frame_clock_advance(&clock, &input, 4, NULL);
}

static bool native_clock(const char *path) {
    FILE *file = fopen(path, "rb");
    unsigned version, count;
    bool okay =
        file &&
        fscanf(file, "AB_FRAME_CLOCK_NATIVE %u\nCASES %u\n", &version, &count) == 2 &&
        version == 1 && count > 0 && count <= 4096;
    for (unsigned index = 0; okay && index < count; ++index) {
        unsigned actual_index, enabled, present;
        uint32_t words[6];
        okay = fscanf(file,
                      "C %u %" SCNx32 " %" SCNx32 " %" SCNx32 " %u %u %" SCNx32
                      " %" SCNx32 " %" SCNx32 "\n",
                      &actual_index, &words[0], &words[1], &words[2], &enabled,
                      &present, &words[3], &words[4], &words[5]) == 9 &&
               actual_index == index && enabled <= 1 && present <= 1;
        if (!okay) {
            break;
        }
        AbFrameClock clock = {from_bits(words[0]), from_bits(words[1])};
        AbFrameClockInput input = {.seconds = from_bits(words[2]),
                                   .speed_factor = from_bits(words[3]),
                                   .speed_factor_present = present != 0,
                                   .physics_enabled = enabled != 0};
        AbFrameClockPlan plan;
        okay = ab_frame_clock_advance(&clock, &input, 4096, &plan) &&
               float_bits(plan.next.time) == words[4] &&
               float_bits(plan.next.accumulator) == words[5];
        if (!okay) {
            fprintf(stderr, "Native frame clock mismatch at case %u\n", index);
        }
    }
    if (file) {
        okay = okay && fgetc(file) == EOF && !ferror(file);
        fclose(file);
    }
    if (okay) {
        printf("Matched %u original GameLua update clocks\n", count);
    }
    return okay;
}

int main(int argc, char **argv) {
    bool okay = argc == 2 ? native_clock(argv[1]) : argc == 1 && edges();
    if (!okay) {
        fprintf(stderr, "Frame clock comparison failed\n");
    }
    return okay ? 0 : 1;
}
