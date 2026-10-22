#include "ab/camera_drag.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

static bool finite_point(AbVec2 value) {
    return isfinite(value.x) && isfinite(value.y);
}

static uint32_t float_bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static float float_word(uint32_t value) {
    float result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

/* Match ARM's NaN selection at the drag buffer's add/divide boundaries. Newly
 * invalid operations produce positive default NaN; signaling operands take
 * priority before quiet operands and retain their sign/payload when quieted.
 * Floating exception/status flags are outside this numeric contract. */
static float velocity_binary(float first, float second, bool divide) {
    uint32_t a = float_bits(first), b = float_bits(second);
    bool a_nan = (a & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000);
    bool b_nan = (b & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000);
    uint32_t selected = 0;
    if (a_nan && !(a & UINT32_C(0x00400000))) {
        selected = a;
    } else if (b_nan && !(b & UINT32_C(0x00400000))) {
        selected = b;
    } else if (a_nan) {
        selected = a;
    } else if (b_nan) {
        selected = b;
    }
    if (selected) {
        return float_word(selected | UINT32_C(0x00400000));
    }
    float result = divide ? first / second : first + second;
    return isnan(result) ? float_word(UINT32_C(0x7fc00000)) : result;
}

bool ab_camera_drag_init(AbCameraDrag *state) {
    if (!state) {
        return false;
    }
    *state = (AbCameraDrag){0};
    return true;
}

bool ab_camera_drag_start(AbCameraDrag *state, AbVec2 touch, AbVec2 anchor) {
    if (!state || !finite_point(touch) || !finite_point(anchor)) {
        return false;
    }
    *state = (AbCameraDrag){.start = touch,
                            .anchor = anchor,
                            .current_screen = touch,
                            .old_screen = touch,
                            .initialized = true,
                            .dragging = true};
    return true;
}

bool ab_camera_drag_is_dragging(const AbCameraDrag *state) {
    return state && state->initialized && state->dragging;
}

bool ab_camera_drag_sample(AbCameraDrag *state, AbVec2 velocity) {
    if (!state || !state->initialized || state->count > AB_CAMERA_DRAG_SAMPLES) {
        return false;
    }
    if (state->count == AB_CAMERA_DRAG_SAMPLES) {
        memmove(state->velocity, state->velocity + 1,
                (AB_CAMERA_DRAG_SAMPLES - 1) * sizeof(*state->velocity));
        --state->count;
    }
    state->velocity[state->count++] = velocity;
    return true;
}

bool ab_camera_drag_velocity(const AbCameraDrag *state, AbVec2 pixel_size,
                             AbVec2 *output) {
    if (!state || !state->initialized || state->count > AB_CAMERA_DRAG_SAMPLES ||
        !output || !finite_point(pixel_size) || pixel_size.x == 0 ||
        pixel_size.y == 0) {
        return false;
    }
    if (!state->count) {
        uint32_t word = UINT32_C(0x7fc00000);
        float quiet_nan;
        memcpy(&quiet_nan, &word, sizeof(quiet_nan));
        *output = (AbVec2){quiet_nan, quiet_nan};
        return true;
    }
    AbVec2 sum = {0};
    for (size_t index = 0; index < state->count; ++index) {
        sum.x = velocity_binary(sum.x, state->velocity[index].x, false);
        sum.y = velocity_binary(sum.y, state->velocity[index].y, false);
    }
    float x = velocity_binary(-sum.x, (float)state->count, true);
    float y = velocity_binary(-sum.y, (float)state->count, true);
    AbVec2 result = {velocity_binary(x, pixel_size.x, true),
                     velocity_binary(y, pixel_size.y, true)};
    *output = result;
    return true;
}

bool ab_camera_drag_update(AbCameraDrag *state, float seconds, AbVec2 touch,
                           const AbCoordinateCamera *camera, AbVec2 *target) {
    if (!ab_camera_drag_is_dragging(state) || !target || !isfinite(seconds) ||
        !finite_point(touch)) {
        return false;
    }
    AbCameraDrag next = *state;
    next.old_screen = next.current_screen;
    next.current_screen = touch;
    AbVec2 velocity = {
        velocity_binary(next.current_screen.x - next.old_screen.x, seconds, true),
        velocity_binary(next.current_screen.y - next.old_screen.y, seconds, true)};
    AbVec2 delta = {touch.x - next.start.x, touch.y - next.start.y}, world;
    if (!ab_camera_drag_sample(&next, velocity) ||
        !ab_coordinate_distance(camera, AB_SPACE_SCREEN, AB_SPACE_WORLD, delta,
                                &world)) {
        return false;
    }
    AbVec2 result = {next.anchor.x - world.x, next.anchor.y - world.y};
    if (!finite_point(result)) {
        return false;
    }
    *state = next;
    *target = result;
    return true;
}

bool ab_camera_drag_release(AbCameraDrag *state, AbVec2 pixel_size,
                            AbCameraDragRelease *output) {
    AbVec2 velocity;
    if (!state || !output || !ab_camera_drag_velocity(state, pixel_size, &velocity)) {
        return false;
    }
    AbCameraDragRelease result = {state->current_screen, velocity};
    state->dragging = false;
    *output = result;
    return true;
}
