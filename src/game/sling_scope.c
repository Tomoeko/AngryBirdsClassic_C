#include "ab/sling_scope.h"
#include "ab/rotation.h"

#include <math.h>

bool ab_sling_scope_update(AbSlingScopeState *state, AbVec2 velocity, float seconds,
                           AbError *error) {
    if (!state || !isfinite(state->timer) || !isfinite(state->angle) ||
        !isfinite(seconds) || !isfinite(velocity.x) || !isfinite(velocity.y))
        return ab_fail(error, "Invalid Sling Scope update numbers");
    float timer = state->timer + seconds;
    float angle = state->angle;
    if (velocity.x != 0 || velocity.y != 0) {
        float target;
        if (!ab_script_angle_from_components(velocity.y, velocity.x, &target))
            return ab_fail(error, "Unsupported Sling Scope target angle");
        const float pi = 0x1.921fb6p+1f;
        if (target < angle - pi)
            target += pi * 2;
        else if (angle + pi < target)
            target -= pi * 2;
        float change = target - angle;
        change = (change * seconds) * 2;
        angle = (float)fmod((double)(angle + change), (double)(pi * 2));
    }
    if (!isfinite(timer) || !isfinite(angle))
        return ab_fail(error, "Sling Scope update exceeds finite range");
    *state = (AbSlingScopeState){timer, angle, velocity};
    return true;
}

static bool finite_vector(AbVec2 value) {
    return isfinite(value.x) && isfinite(value.y);
}

static float height(float origin, float velocity_step, float index, float gravity_step,
                    float vertical_step) {
    float squared = index * index;
    float result = origin + velocity_step * index;
    result = result + (squared * 0.5f) * gravity_step;
    result = result + (index * 0.5f) * gravity_step;
    return result + ((squared + index) * 0.5f) * vertical_step;
}

bool ab_sling_scope_plan(const AbSlingScopeInput *input, AbSlingScopePlan *output,
                         AbError *error) {
    if (!input || !output || !finite_vector(input->velocity) ||
        !finite_vector(input->position) || !finite_vector(input->render.translation) ||
        !finite_vector(input->render.scale) || !isfinite(input->timer) ||
        !isfinite(input->vertical_force) || !isfinite(input->gravity_y) ||
        !isfinite(input->maximum_translation) || input->maximum_translation <= 0 ||
        !isfinite(input->physics_to_world) || !isfinite(input->length) ||
        input->length < 0 || input->length > 12 || !isfinite(input->alpha_factor))
        return ab_fail(error, "Unsupported native Sling Scope input");
    const float step = 0x1.111112p-5f;
    const float step_squared = 0x1.23456ap-10f;
    AbVec2 velocity = input->velocity;
    float speed_squared = velocity.y * velocity.y;
    speed_squared = speed_squared + velocity.x * velocity.x;
    float speed = (float)sqrt((double)speed_squared);
    float maximum = input->maximum_translation / step;
    if (!isfinite(speed) || !isfinite(maximum))
        return ab_fail(error, "Sling Scope speed exceeds finite range");
    if (maximum < speed) {
        velocity.x = (velocity.x / speed) * maximum;
        velocity.y = (velocity.y / speed) * maximum;
    }
    velocity.x = velocity.x * step;
    velocity.y = velocity.y * step;
    float gravity = input->gravity_y * step_squared;
    float vertical = 0;
    if (fabsf(input->vertical_force) > 0.01f && input->body_present) {
        if (!isfinite(input->mass) || input->mass <= 0)
            return ab_fail(error, "Sling Scope vertical force requires its body mass");
        vertical = (input->vertical_force * step_squared) / input->mass;
    }
    float phase = input->timer * 0.2f;
    phase = (float)fmod((double)phase, (double)0.2f);
    AbSlingScopePlan plan = {.final_render = input->render, .final_alpha = 1};
    /* The original restores these from the saved scales, not translations. */
    plan.final_render.translation = input->render.scale;
    while (phase < input->length) {
        if (phase != 0) {
            if (input->length == 0 || plan.count == AB_SLING_SCOPE_DOTS)
                return ab_fail(error,
                               "Sling Scope dot plan exceeds its supported bound");
            float ratio = phase / step;
            float before = floorf(ratio);
            float after = before + 1;
            float y_after =
                height(input->position.y, velocity.y, after, gravity, vertical);
            float y_before =
                height(input->position.y, velocity.y, before, gravity, vertical);
            float x_after = input->position.x + velocity.x * after;
            float x_before = input->position.x + velocity.x * before;
            float shrink = 1 - phase / input->length;
            float fraction = ratio - before;
            float y = y_before + fraction * (y_after - y_before);
            float x = x_before + (x_after - x_before) * fraction;
            y = y * input->physics_to_world;
            x = x * input->physics_to_world;
            float inverse = 1 / shrink;
            AbSlingScopeDot dot = {.position = {x / shrink, y / shrink},
                                   .render = input->render,
                                   .alpha = input->alpha_decay
                                                ? input->alpha_factor * shrink
                                                : input->alpha_factor};
            dot.render.translation = (AbVec2){inverse * input->render.translation.x,
                                              inverse * input->render.translation.y};
            dot.render.scale = (AbVec2){shrink * input->render.scale.x,
                                        shrink * input->render.scale.y};
            if (!finite_vector(dot.position) ||
                !finite_vector(dot.render.translation) ||
                !finite_vector(dot.render.scale) || !isfinite(dot.alpha))
                return ab_fail(error, "Sling Scope dot exceeds finite range");
            plan.dots[plan.count++] = dot;
        }
        phase = phase + 0.2f;
    }
    *output = plan;
    return true;
}
