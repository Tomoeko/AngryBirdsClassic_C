#include "ab/flame_effect.h"

#include "ab/object_body.h"
#include "ab/rotation.h"
#include "ab/script_math.h"

#include <math.h>
#include <string.h>

static bool valid_span(AbSpan span) {
    return !span.size || span.data;
}

static bool same_span(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || memcmp(first.data, second.data, first.size) == 0);
}

static bool select_offset(const AbFlameDefinition *definition, const AbFlameStep *step,
                          AbVec2 *output, AbError *error) {
    if (step->object_sprite_present) {
        for (size_t index = 0; index < definition->offset_count; ++index) {
            const AbFlameOffset *offset = definition->offsets + index;
            if (!valid_span(offset->sprite)) {
                return ab_fail(error, "Invalid flame offset name");
            }
            if (same_span(offset->sprite, step->object_sprite)) {
                *output = offset->point;
                return true;
            }
        }
    }
    *output = definition->default_present ? definition->default_offset : (AbVec2){0};
    return true;
}

bool ab_flame_update(AbFlameState *state, const AbFlameDefinition *definition,
                     const AbFlameStep *step, AbRandom48 *random, AbError *error) {
    if (!state || !definition || !step || (definition->count && !definition->sprites) ||
        (definition->offset_count && !definition->offsets) ||
        definition->count > UINT32_C(16777216) ||
        definition->offset_count > UINT32_C(16777216) || !isfinite(state->timer) ||
        !isfinite(state->timer_speed) ||
        (state->angle_present && !isfinite(state->angle)) || !isfinite(step->seconds) ||
        !isfinite(step->body_angle) || !isfinite(step->velocity.x) ||
        !isfinite(step->velocity.y) ||
        (step->object_sprite_present && !valid_span(step->object_sprite))) {
        return ab_fail(error, "Invalid resolved flame update");
    }
    AbFlameState next = *state;
    next.timer = state->timer + step->seconds * state->timer_speed;
    if (next.timer >= 1) {
        next.timer = next.timer - 1;
    }
    float key = floorf(next.timer * (float)definition->count) + 1;
    if (!isfinite(next.timer) || !isfinite(key)) {
        return ab_fail(error, "Flame timer exceeds finite range");
    }
    next.sprite_present = key >= 1 && (double)key <= (double)definition->count;
    next.sprite =
        next.sprite_present ? definition->sprites[(size_t)key - 1] : (AbSpan){0};
    if (next.sprite_present && !valid_span(next.sprite)) {
        return ab_fail(error, "Invalid selected flame sprite");
    }
    next.new_angle = step->body_angle;
    float direction;
    if (!ab_script_angle_from_components(-step->velocity.y, step->velocity.x,
                                         &direction)) {
        return ab_fail(error, "Invalid flame velocity angle");
    }
    float squared =
        step->velocity.x * step->velocity.x + step->velocity.y * step->velocity.y;
    float target = squared > 2 ? -direction - 0x1.921fb6p+0f : 0;
    if (!ab_game_angle_wrap(target, &target)) {
        return ab_fail(error, "Invalid clamped flame angle");
    }
    float unit;
    if (ab_script_random(random, 0, 0, 0, &unit) != AB_SCRIPT_RANDOM_OK) {
        return ab_fail(error, "Invalid shared flame random state");
    }
    target = target + (unit - 0.5f) * 0x1.921fb6p+1f;
    next.old_angle = state->angle_present ? state->angle : 0;
    float delta = target - next.old_angle;
    if (delta < -0x1.921fb6p+1f) {
        delta = delta + 0x1.921fb6p+2f;
    } else if (delta > 0x1.921fb6p+1f) {
        delta = delta - 0x1.921fb6p+2f;
    }
    float blend = 5 * step->seconds;
    blend = blend < 1 ? blend : 1;
    next.angle = next.old_angle + blend * delta;
    next.angle_present = true;
    if (!isfinite(next.angle) ||
        !select_offset(definition, step, &next.offset, error)) {
        return ab_fail(error, "Invalid flame angle or offset selection");
    }
    *state = next;
    return true;
}

bool ab_flame_draw(const AbFlameState *state, bool object_present,
                   AbVec2 object_position, float body_scale, AbFlameDraw *output,
                   AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing flame draw output");
    }
    if (!object_present) {
        *output = (AbFlameDraw){.remove_callback = true};
        return true;
    }
    if (!state || !state->angle_present || !isfinite(state->angle) ||
        !isfinite(state->scale) || !isfinite(body_scale) ||
        !isfinite(object_position.x) || !isfinite(object_position.y) ||
        !isfinite(state->offset.x) || !isfinite(state->offset.y) ||
        (state->sprite_present && !valid_span(state->sprite))) {
        return ab_fail(error, "Invalid resolved flame draw");
    }
    float cosine, sine;
    if (!ab_script_cosine(state->new_angle, &cosine) ||
        !ab_script_sine(state->new_angle, &sine)) {
        return ab_fail(error, "Unsupported flame offset rotation");
    }
    AbVec2 scaled = {state->offset.x * body_scale, state->offset.y * body_scale};
    AbVec2 rotated = {scaled.x * cosine - scaled.y * sine,
                      scaled.x * sine + scaled.y * cosine};
    AbFlameDraw next = {
        .sprite = state->sprite,
        .position = {object_position.x + rotated.x, object_position.y + rotated.y},
        .scale = body_scale * state->scale,
        .angle = state->angle,
        .sprite_present = state->sprite_present};
    if (!isfinite(next.position.x) || !isfinite(next.position.y) ||
        !isfinite(next.scale)) {
        return ab_fail(error, "Flame draw exceeds finite range");
    }
    *output = next;
    return true;
}

bool ab_flame_apply(AbFlameState *state, const AbFlameCreation *creation,
                    AbRandom48 *random, AbFlameHost host, AbError *error) {
    if (!state || !creation || !creation->definition || !creation->body ||
        !creation->z_order || !host.register_update || !host.register_draw ||
        (creation->definition->count && !creation->definition->sprites)) {
        return ab_fail(error, "Invalid flame construction");
    }
    const AbFlameDefinition *definition = creation->definition;
    AbFlameState initial = {.timer_speed = creation->timer_speed,
                            .scale = creation->scale,
                            .sprite_present = definition->count != 0};
    if (initial.sprite_present) {
        initial.sprite = definition->sprites[0];
        if (!valid_span(initial.sprite)) {
            return ab_fail(error, "Invalid initial flame sprite");
        }
    }
    *state = initial;
    if (!host.register_update(host.context, state, definition, error)) {
        return false;
    }
    AbFlameStep step = *creation->body;
    step.seconds = 0;
    if (!ab_flame_update(state, definition, &step, random, error)) {
        return false;
    }
    float z_order = *creation->z_order + 1;
    if (!isfinite(z_order)) {
        return ab_fail(error, "Invalid flame draw order");
    }
    return host.register_draw(host.context, state, z_order, error);
}
