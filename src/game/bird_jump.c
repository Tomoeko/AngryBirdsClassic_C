#include "ab/bird_jump.h"
#include "ab/script_math.h"

#include <math.h>
#include <stdint.h>

static bool draw(const AbBirdJumpRandom *random, int minimum, int maximum,
                 float *output) {
    int value;
    if (!random || !random->integer ||
        !random->integer(random->context, minimum, maximum, &value) ||
        value < minimum || (int64_t)value > (int64_t)maximum + 1) {
        return false;
    }
    *output = (float)value;
    return true;
}

static void position_event(AbBirdJumpFrame *frame, AbVec2 position) {
    frame->next.position = position;
    frame->events[frame->event_count++] =
        (AbBirdJumpEvent){AB_BIRD_JUMP_POSITION, position};
}

static void rotation_event(AbBirdJumpFrame *frame, float angle) {
    frame->next.rotation = angle;
    frame->events[frame->event_count++] =
        (AbBirdJumpEvent){AB_BIRD_JUMP_ROTATION, {angle, 0}};
}

static bool start_jump(AbBirdJumpState *state, bool completed, bool allow_roll,
                       const AbBirdJumpRandom *random) {
    float timer, height;
    if (!draw(random, 0, 35, &timer) || !draw(random, 5, 15, &height)) {
        return false;
    }
    state->timer = timer / 10.0f;
    state->height = height / 10.0f;
    state->speed = (7.0f / state->height) * 0x1.333334p+0f;
    if (completed) {
        if (!draw(random, 25, 35, &height)) {
            return false;
        }
        state->timer = 0;
        state->height = height / 10.0f;
        state->speed = (7.0f / state->height) * 0x1.99999ap+0f;
    }
    state->phase = 0;
    state->jumping = true;
    state->rebound = false;
    state->roll = 0;
    if (allow_roll && 0x1.ccccccp-1f < state->height) {
        if (!draw(random, 0, 4, &state->roll)) {
            return false;
        }
        state->roll = state->roll - 1;
    }
    state->saved_y = state->position.y;
    state->saved_angle = state->rotation;
    return true;
}

bool ab_bird_jump_frame(const AbBirdJumpState *state, float seconds,
                        bool level_completed, bool allow_roll,
                        const AbBirdJumpRandom *random, AbBirdJumpFrame *output) {
    if (!state || !output || !isfinite(seconds) || !isfinite(state->position.x) ||
        !isfinite(state->position.y) || !isfinite(state->rotation) ||
        !isfinite(state->timer)) {
        return false;
    }
    AbBirdJumpFrame frame = {.next = *state};
    AbBirdJumpState *next = &frame.next;
    if (!next->jumping) {
        next->timer = next->timer - seconds;
    }
    if (next->timer < 0 && !start_jump(next, level_completed, allow_roll, random)) {
        return false;
    }
    if (next->jumping) {
        if (!isfinite(next->height) || !isfinite(next->speed) ||
            !isfinite(next->phase) || !isfinite(next->roll) ||
            !isfinite(next->saved_y) || !isfinite(next->saved_angle)) {
            return false;
        }
        next->phase = next->phase + seconds * next->speed;
        float sine;
        if (!ab_script_sine(next->phase, &sine)) {
            return false;
        }
        AbVec2 position = {next->position.x, next->saved_y - sine * next->height};
        if (!isfinite(position.y)) {
            return false;
        }
        position_event(&frame, position);
        if ((next->roll == -1 || next->roll == 1) && !next->rebound) {
            float angle = (next->phase * 2.0f) * next->roll;
            if (!isfinite(angle)) {
                return false;
            }
            rotation_event(&frame, angle);
        }
        if (0x1.921fb6p+1f < next->phase) {
            if (next->rebound) {
                next->jumping = false;
                position_event(&frame, (AbVec2){next->position.x, next->saved_y});
                rotation_event(&frame, next->saved_angle);
            } else {
                next->rebound = true;
                next->phase = 0;
                next->height = next->height * 0x1.333334p-2f;
                next->speed = next->speed * 2.0f;
                if (!isfinite(next->height) || !isfinite(next->speed)) {
                    return false;
                }
            }
        }
    }
    if (!isfinite(next->timer)) {
        return false;
    }
    *output = frame;
    return true;
}
