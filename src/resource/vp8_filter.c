#include "vp8_filter.h"
#include "vp8_math.h"

static int absolute(int value) {
    return value < 0 ? -value : value;
}

static int clamp_signed(int value) {
    return value < -128 ? -128 : value > 127 ? 127 : value;
}

static uint8_t clamp_pixel(int value) {
    return (uint8_t)(value < 0 ? 0 : value > 255 ? 255 : value);
}

static void adjust_pair(uint8_t *edge, ptrdiff_t normal, int amount) {
    int after = ab_vp8_floor_shift(clamp_signed(amount + 4), 3);
    int before = ab_vp8_floor_shift(clamp_signed(amount + 3), 3);
    edge[0] = clamp_pixel(edge[0] - after);
    edge[-normal] = clamp_pixel(edge[-normal] + before);
}

static void filter_sample(uint8_t *edge, ptrdiff_t normal, uint8_t limit,
                          uint8_t interior, uint8_t variance, bool macroblock,
                          bool simple) {
    int before[4], after[4];
    for (ptrdiff_t index = 0; index < 4; ++index) {
        before[index] = edge[-(index + 1) * normal];
        after[index] = edge[index * normal];
    }
    int difference = after[0] - before[0];
    if (2 * absolute(difference) + absolute(before[1] - after[1]) / 2 > limit) {
        return;
    }
    int outer = clamp_signed(before[1] - after[1]);
    if (simple) {
        adjust_pair(edge, normal, outer + 3 * difference);
        return;
    }
    for (size_t index = 0; index < 3; ++index) {
        if (absolute(before[index + 1] - before[index]) > interior ||
            absolute(after[index + 1] - after[index]) > interior) {
            return;
        }
    }
    bool high_variance = absolute(before[1] - before[0]) > variance ||
                         absolute(after[1] - after[0]) > variance;
    if (high_variance) {
        adjust_pair(edge, normal, outer + 3 * difference);
    } else if (macroblock) {
        int amount = clamp_signed(outer + 3 * difference);
        static const int weights[3] = {27, 18, 9};
        for (ptrdiff_t index = 0; index < 3; ++index) {
            int adjustment = ab_vp8_floor_shift(amount * weights[index] + 63, 7);
            edge[-(index + 1) * normal] = clamp_pixel(before[index] + adjustment);
            edge[index * normal] = clamp_pixel(after[index] - adjustment);
        }
    } else {
        int amount = clamp_signed(3 * difference);
        int after_adjustment = ab_vp8_floor_shift(clamp_signed(amount + 4), 3);
        adjust_pair(edge, normal, amount);
        int secondary = ab_vp8_floor_shift(after_adjustment + 1, 1);
        edge[-2 * normal] = clamp_pixel(before[1] + secondary);
        edge[normal] = clamp_pixel(after[1] - secondary);
    }
}

void ab_vp8_filter_edge(uint8_t *edge, size_t normal, size_t along, size_t count,
                        uint8_t limit, uint8_t interior, uint8_t variance,
                        bool macroblock, bool simple) {
    for (size_t index = 0; index < count; ++index) {
        filter_sample(edge + index * along, (ptrdiff_t)normal, limit, interior,
                      variance, macroblock, simple);
    }
}
