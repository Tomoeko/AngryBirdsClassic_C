#ifndef AB_RANDOM_H
#define AB_RANDOM_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint64_t value;
} AbRandom48;

typedef struct {
    uint32_t words[4096];
    uint32_t cursor, carry;
    bool initialized;
} AbEngineRandom;

/* Native math::RandomUtil stream used by particle creation, separate from Lua's
 * Android random48 state. Seed all 4096 words through the recovered four-word
 * recurrence. Next returns the original unsigned-word / 2^32 binary64 result;
 * consumers perform their own observed float conversion. A zero-initialized
 * owner lazily uses native default seed 123456789. No clock or allocation.
 * Share one stable owner across engine consumers. Invalid pointers/cursor leave
 * output/state unchanged. Explicit seed resets the entire stream. */
bool ab_engine_random_seed(AbEngineRandom *state, uint32_t seed);
bool ab_engine_random_next(AbEngineRandom *state, double *output);

typedef enum {
    AB_SCRIPT_RANDOM_OK,
    AB_SCRIPT_RANDOM_ARGUMENT_COUNT,
    AB_SCRIPT_RANDOM_EMPTY_INTERVAL,
    AB_SCRIPT_RANDOM_INVALID_STORAGE,
} AbScriptRandomResult;

/* Android reference srand48/lrand48 numeric boundary. Seed is the original
 * 32-bit word, including negative seeds' bit patterns. State is caller-owned,
 * explicitly shared by consumers; no hidden global or platform RNG is used.
 * Invalid storage or a state above 48 bits leaves state/output unchanged. */
bool ab_random48_seed(AbRandom48 *state, uint32_t seed);
/* gamelogic initialization uses math.randomseed(os.time()). Android's signed
 * 32-bit epoch seconds first round to the target's float Lua number, then convert
 * back with the ARM signed conversion's saturation at the positive endpoint.
 * Epoch -1 is os.time's failure sentinel and preserves state. The caller
 * supplies time explicitly; nearest rounding is required. No host
 * clock is read and no later game-consumer order is implied. */
bool ab_random48_seed_time(AbRandom48 *state, int32_t epoch_seconds);
bool ab_random48_next(AbRandom48 *state, uint32_t *output);

/* Original Lua math.random arithmetic after integer argument conversion.
 * Zero arguments returns a unit float; one uses [1, first], two [first, second].
 * Every valid-storage call consumes a draw, including bad counts/empty ranges.
 * Errors preserve output. Binary32 rounding can return one above an interval's
 * upper bound; the native integer span wraps at 32 bits. Keep both behaviors.
 * Seed policy, argument type conversion and complete game consumption order are
 * separate. Requires nearest rounding and gradual underflow like other numerics. */
AbScriptRandomResult ab_script_random(AbRandom48 *state, unsigned argument_count,
                                      int32_t first, int32_t second, float *output);

#endif
