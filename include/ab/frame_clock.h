#ifndef AB_FRAME_CLOCK_H
#define AB_FRAME_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float time;
    float accumulator;
} AbFrameClock;

typedef struct {
    float seconds;
    float speed_factor;
    bool speed_factor_present;
    bool physics_enabled;
} AbFrameClockInput;

typedef struct {
    AbFrameClock next;
    float physics_seconds;
    float interpolation;
    uint32_t steps;
    uint32_t history_updates;
    bool interpolation_present;
} AbFrameClockPlan;

/* GameApp::update caps its incoming delta at binary32 0.1 before GameLua.
 * NaNs and positive infinity select the cap; negatives and signed zero pass
 * through. This is separate from the unclamped GameLua clock arithmetic below.
 * Floating-point exception/status flags are outside this result contract. */
float ab_app_frame_seconds(float seconds);

/* GameLua::update clock arithmetic, separate from its callbacks and world work.
 * Time receives unscaled seconds even with physics disabled. Enabled physics
 * adds seconds (or seconds * an explicitly present numeric speed factor) to
 * the accumulator and repeatedly subtracts binary32 1/30. Each completed step
 * whose remaining accumulator is <= twice that timestep updates pose history.
 * Enabled interpolation is the final accumulator divided by the timestep.
 *
 * This plans arithmetic only; callers must execute every step and history pass
 * in native order. It does not implement pause policy, callbacks or presentation.
 * All used inputs/results must be finite. step_limit is a caller budget in
 * [0, 4096]; exceeding it or a subtraction that makes no progress rejects the
 * plan. No time clamp, dropped steps or accumulator reset is substituted.
 * Unused speed_factor may be nonfinite. Failure leaves output unchanged.
 */
bool ab_frame_clock_advance(const AbFrameClock *clock, const AbFrameClockInput *input,
                            uint32_t step_limit, AbFrameClockPlan *output);

#endif
