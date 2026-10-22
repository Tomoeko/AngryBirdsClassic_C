#include "ab/ambient_animation.h"

#include <math.h>
#include <string.h>

static bool valid_span(AbSpan value) {
    return !value.size || value.data;
}

bool ab_ambient_animation_start(AbAmbientAnimationState *state,
                                AbAmbientAnimationVariant *variants, size_t count,
                                AbSpan object_sprite, bool sprite_present,
                                AbRandom48 *random, AbError *error) {
    if (!state || !variants || !count || count > UINT32_C(16777216)) {
        return ab_fail(error, "Invalid ambient animation variants");
    }
    float selection;
    if (ab_script_random(random, 2, 1, (int32_t)count, &selection) !=
            AB_SCRIPT_RANDOM_OK ||
        selection < 1 || (double)selection > (double)count) {
        return ab_fail(error, "Ambient animation selection is outside its array");
    }
    size_t selected = (size_t)selection - 1;
    AbAmbientAnimationVariant *variant = variants + selected;
    float duration = variant->delay;
    if (!variant->delay_present) {
        float unit;
        if (!isfinite(variant->min_delay) || !isfinite(variant->max_delay) ||
            ab_script_random(random, 0, 0, 0, &unit) != AB_SCRIPT_RANDOM_OK) {
            return ab_fail(error, "Invalid ambient animation random delay");
        }
        duration =
            variant->min_delay + unit * (variant->max_delay - variant->min_delay);
    }
    if (!isfinite(duration) || (sprite_present && !valid_span(object_sprite))) {
        return ab_fail(error, "Invalid ambient animation initial state");
    }
    *state = (AbAmbientAnimationState){.current = selected,
                                       .delay_duration = duration,
                                       .original_sprite = object_sprite,
                                       .current_present = true,
                                       .original_sprite_present = sprite_present};
    variant->timer = 0;
    variant->timer_present = true;
    return true;
}

bool ab_ambient_animation_advance(AbAmbientAnimationState *state,
                                  AbAmbientAnimationVariant *variants, size_t count,
                                  float seconds, AbSpan object_sprite,
                                  bool sprite_present, AbAmbientAnimationHost host,
                                  AbAmbientAnimationResult *result, AbError *error) {
    if (!state || !variants || !result || count > UINT32_C(16777216) ||
        !state->current_present || state->current >= count || !isfinite(seconds) ||
        !isfinite(state->delay_timer) || !isfinite(state->delay_duration)) {
        return ab_fail(error, "Invalid ambient animation advance");
    }
    if (state->delay_timer < state->delay_duration) {
        float timer = state->delay_timer + seconds;
        if (!isfinite(timer)) {
            return ab_fail(error, "Ambient animation delay exceeds finite range");
        }
        state->delay_timer = timer;
        *result = AB_AMBIENT_ANIMATION_WAITING;
        return true;
    }
    AbAmbientAnimationVariant *variant = variants + state->current;
    if (!variant->timer_present || !isfinite(variant->timer) ||
        !isfinite(variant->speed) || variant->count > UINT32_C(16777216) ||
        (variant->count && !variant->sprites)) {
        return ab_fail(error, "Invalid selected ambient animation");
    }
    float timer = variant->timer + seconds;
    float key = ceilf(timer * variant->speed);
    if (!isfinite(timer) || !isfinite(key)) {
        return ab_fail(error, "Ambient animation frame exceeds finite range");
    }
    bool present = key >= 1 && (double)key <= (double)variant->count;
    AbSpan frame = present ? variant->sprites[(size_t)key - 1] : (AbSpan){0};
    if (present && !valid_span(frame)) {
        return ab_fail(error, "Invalid selected ambient animation sprite");
    }
    variant->timer = timer;
    if (!present) {
        variant->timer_present = false;
        if (!host.set_sprite ||
            (state->original_sprite_present && !valid_span(state->original_sprite))) {
            return ab_fail(error, "Invalid ambient animation restoration");
        }
        if (!host.set_sprite(host.context, state->original_sprite,
                             state->original_sprite_present, error)) {
            return false;
        }
        *result = AB_AMBIENT_ANIMATION_FINISHED;
        return true;
    }
    if (sprite_present && !valid_span(object_sprite)) {
        return ab_fail(error, "Invalid current ambient animation sprite");
    }
    bool same =
        sprite_present && frame.size == object_sprite.size &&
        (!frame.size || memcmp(frame.data, object_sprite.data, frame.size) == 0);
    if (!same) {
        if (!host.set_sprite) {
            return ab_fail(error, "Missing ambient animation sprite binding");
        }
        if (!host.set_sprite(host.context, frame, true, error)) {
            return false;
        }
    }
    *result = AB_AMBIENT_ANIMATION_PLAYING;
    return true;
}
