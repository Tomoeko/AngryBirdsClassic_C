#include "ab/mighty_eagle_timer.h"

#include <math.h>
#include <string.h>

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind == AB_DATA_NUMBER) {
        memcpy(output, &value.bits, sizeof(*output));
        if (isfinite(*output))
            return true;
    }
    return ab_fail(error, "Mighty Eagle timer requires a finite numeric source value");
}

static bool literal_true(AbDataValue value) {
    return value.kind == AB_DATA_BOOLEAN && value.bits;
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool call(AbMightyEagleTimerHost host, void *context,
                 AbMightyEagleTimerOperation operation, AbVec2 position,
                 AbError *error) {
    return host ? host(context, (AbMightyEagleTimerCall){operation, position}, error)
                : ab_fail(error, "Missing actual Mighty Eagle timer service");
}

bool ab_mighty_eagle_timer_update(AbMightyEagleTimer *state,
                                  AbDataValue *specialty_available, float seconds,
                                  AbMightyEagleTimerHost host, void *context,
                                  AbError *error) {
    if (!state)
        return ab_fail(error, "Missing Mighty Eagle timer state");
    if (state->timer.kind == AB_DATA_NIL)
        return true;
    float timer;
    if (!numeric(state->timer, &timer, error) || !isfinite(seconds) ||
        !isfinite(timer - seconds))
        return ab_fail(error, "Invalid Mighty Eagle timer decrement");
    timer -= seconds;
    state->timer = number(timer);
    if (timer < 6.2f && literal_true(state->moving) &&
        state->bait_x.kind != AB_DATA_NIL && state->bait_y.kind != AB_DATA_NIL) {
        if (!specialty_available)
            return ab_fail(error, "Missing live Eagle specialty-availability slot");
        *specialty_available = (AbDataValue){.kind = AB_DATA_BOOLEAN};
        state->moving = (AbDataValue){.kind = AB_DATA_BOOLEAN};
        AbVec2 position = {0};
        if (!numeric(state->bait_x, &position.x, error) ||
            !numeric(state->bait_y, &position.y, error) ||
            !call(host, context, AB_MIGHTY_EAGLE_CREATE, position, error))
            return false;
    } else if (timer < 7.7f && !literal_true(state->sound_played)) {
        if (!call(host, context, AB_MIGHTY_EAGLE_YELL, (AbVec2){0}, error) ||
            !call(host, context, AB_MIGHTY_EAGLE_FLY, (AbVec2){0}, error))
            return false;
        state->sound_played = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    }
    if (state->camera_shake.kind != AB_DATA_NIL) {
        float shake;
        if (!numeric(state->camera_shake, &shake, error))
            return false;
        if (shake > 0) {
            float decay = shake * seconds;
            decay *= 2.2f;
            float remaining = shake - decay;
            if (!isfinite(decay) || !isfinite(remaining))
                return ab_fail(error, "Mighty Eagle camera-shake decay overflow");
            if (remaining < 0)
                remaining = 0;
            state->camera_shake = number(remaining);
        }
    }
    if (!numeric(state->timer, &timer, error))
        return false;
    if (timer < 0 &&
        !call(host, context, AB_MIGHTY_EAGLE_REMOVE_GOALS, (AbVec2){0}, error))
        return false;
    if (truthy(state->destroy_joints)) {
        if (!call(host, context, AB_MIGHTY_EAGLE_DESTROY_JOINTS, (AbVec2){0}, error))
            return false;
        state->destroy_joints = (AbDataValue){0};
    }
    return true;
}
