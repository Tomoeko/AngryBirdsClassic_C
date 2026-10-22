#include "ab/random.h"

#include <stddef.h>

#define RANDOM48_MASK UINT64_C(0xffffffffffff)

bool ab_engine_random_seed(AbEngineRandom *state, uint32_t seed) {
    if (!state)
        return false;
    uint32_t a = seed, b = UINT32_C(0x159a55e5), c = UINT32_C(0x1f123bb5),
             d = UINT32_C(0x05491333);
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t shifted = a ^ (a << 11);
        uint32_t next = d ^ (d >> 19) ^ shifted ^ (shifted >> 8);
        a = b;
        b = c;
        c = d;
        d = next;
        state->words[index] = next;
    }
    state->cursor = 4095;
    state->carry = UINT32_C(0x587c4);
    state->initialized = true;
    return true;
}

bool ab_engine_random_next(AbEngineRandom *state, double *output) {
    if (!state || !output || (state->initialized && state->cursor >= 4096))
        return false;
    if (!state->initialized)
        ab_engine_random_seed(state, UINT32_C(123456789));
    state->cursor = (state->cursor + 1) & 4095;
    uint64_t product =
        (uint64_t)state->words[state->cursor] * UINT64_C(18782) + state->carry;
    uint32_t high = (uint32_t)(product >> 32);
    uint64_t sum = (uint64_t)(uint32_t)product + high;
    uint32_t overflow = sum > UINT32_MAX ? 1u : 0u;
    state->carry = high + overflow;
    uint32_t folded = (uint32_t)sum + overflow;
    uint32_t word = UINT32_C(0xfffffffe) - folded;
    state->words[state->cursor] = word;
    *output = (double)word * 0x1p-32;
    return true;
}

bool ab_random48_seed(AbRandom48 *state, uint32_t seed) {
    if (!state) {
        return false;
    }
    state->value = ((uint64_t)seed << 16) | UINT64_C(0x330e);
    return true;
}

bool ab_random48_seed_time(AbRandom48 *state, int32_t epoch_seconds) {
    if (!state || epoch_seconds == -1) {
        return false;
    }
    float rounded = (float)epoch_seconds;
    int32_t seed = rounded >= 0x1p31f ? INT32_MAX : (int32_t)rounded;
    return ab_random48_seed(state, (uint32_t)seed);
}

bool ab_random48_next(AbRandom48 *state, uint32_t *output) {
    if (!state || !output || state->value > RANDOM48_MASK) {
        return false;
    }
    state->value =
        (state->value * UINT64_C(0x5deece66d) + UINT64_C(11)) & RANDOM48_MASK;
    *output = (uint32_t)(state->value >> 17);
    return true;
}

AbScriptRandomResult ab_script_random(AbRandom48 *state, unsigned argument_count,
                                      int32_t first, int32_t second, float *output) {
    uint32_t draw;
    if (!output || !ab_random48_next(state, &draw)) {
        return AB_SCRIPT_RANDOM_INVALID_STORAGE;
    }
    float unit = (float)(draw == UINT32_C(0x7fffffff) ? 0 : draw) * 0x1p-31f;
    if (!argument_count) {
        *output = unit;
        return AB_SCRIPT_RANDOM_OK;
    }
    if (argument_count > 2) {
        return AB_SCRIPT_RANDOM_ARGUMENT_COUNT;
    }
    if ((argument_count == 1 && first < 1) || (argument_count == 2 && first > second)) {
        return AB_SCRIPT_RANDOM_EMPTY_INTERVAL;
    }
    int64_t width = first;
    int64_t offset = 1;
    if (argument_count == 2) {
        uint32_t wrapped = (uint32_t)second - (uint32_t)first + UINT32_C(1);
        width = wrapped <= INT32_MAX ? (int64_t)wrapped
                                     : (int64_t)wrapped - INT64_C(4294967296);
        offset = first;
    }
    float product = (float)width * unit;
    int64_t integer = (int64_t)product;
    if ((double)product < (double)integer) {
        --integer;
    }
    /* The original promotes the product for floor/add, then rounds once back
     * to float. Both integral operands and their sum are exact in this range. */
    *output = (float)(integer + offset);
    return AB_SCRIPT_RANDOM_OK;
}
