#include "ab/game_sling_mouse.h"

#include <math.h>
#include <string.h>

static bool finite_vector(AbVec2 value) {
    return isfinite(value.x) && isfinite(value.y);
}

static void append_call(AbSlingMousePlan *plan, AbSlingMouseCallKind kind,
                        AbVec2 vector) {
    plan->calls[plan->count++] =
        (AbSlingMouseCall){kind, vector, plan->bird_dragged, plan->next.drag_started,
                           plan->next.in_game_pressed};
}

static bool begin_press(AbSlingMousePlan *plan, const AbSlingMouseInput *input,
                        AbError *error) {
    if (!finite_vector(input->cursor) || !finite_vector(input->cursor_physics) ||
        !isfinite(input->seconds)) {
        return ab_fail(error, "Invalid sling mouse press coordinates/time");
    }
    AbSlingMouseState *state = &plan->next;
    state->in_game_pressed = true;
    state->start_physics = input->cursor_physics;
    state->start_screen = input->cursor;
    state->old_cursor = input->cursor;
    state->drag_started = true;
    memset(state->samples, 0, sizeof(state->samples));
    state->sample_index = 1;
    state->sample_mask = 1;
    state->samples[0].seconds = input->seconds;
    if (input->touch_count != 1) {
        return true;
    }
    append_call(plan, AB_SLING_MOUSE_DISABLED, (AbVec2){0});
    if (input->sling_disabled || !input->bird_present) {
        return true;
    }
    append_call(plan, AB_SLING_MOUSE_RANGE, (AbVec2){0});
    append_call(plan, AB_SLING_MOUSE_ZOOM, (AbVec2){0});
    if (!finite_vector(input->bird_position) || !isfinite(input->shoot_range) ||
        !isfinite(input->zoom) || input->zoom == 0 || !isfinite(input->screen_width)) {
        return ab_fail(error, "Invalid sling mouse selection geometry");
    }
    AbVec2 difference = {input->bird_position.x - input->cursor_physics.x,
                         input->bird_position.y - input->cursor_physics.y};
    float squared = difference.x * difference.x + difference.y * difference.y;
    float threshold = input->shoot_range / input->zoom;
    threshold *= input->screen_width;
    threshold /= 480.0f;
    if (!isfinite(squared) || !isfinite(threshold)) {
        return ab_fail(error, "Sling mouse selection exceeds finite range");
    }
    if (sqrtf(squared) < threshold) {
        append_call(plan, AB_SLING_MOUSE_SELECT, input->bird_position);
        plan->bird_dragged = true;
    }
    return true;
}

static bool sample_drag(AbSlingMouseState *state, const AbSlingMouseInput *input,
                        AbError *error) {
    if (!finite_vector(input->cursor) || !finite_vector(state->old_cursor) ||
        !isfinite(input->seconds) || state->sample_index < 1 ||
        state->sample_index > AB_SLING_MOUSE_SAMPLES ||
        !(state->sample_mask & (1u << (state->sample_index - 1)))) {
        return ab_fail(error, "Invalid sling mouse drag history");
    }
    AbVec2 delta = {input->cursor.x - state->old_cursor.x,
                    input->cursor.y - state->old_cursor.y};
    size_t index = state->sample_index - 1;
    AbSlingMouseSample sample = state->samples[index];
    if (!isfinite(sample.seconds)) {
        return ab_fail(error, "Invalid sling mouse sample time");
    }
    if (sample.seconds >= 0.017f) {
        index = (index + 1) % AB_SLING_MOUSE_SAMPLES;
        state->sample_index = (uint32_t)index + 1;
        state->sample_mask |= 1u << index;
        sample = (AbSlingMouseSample){delta, input->seconds};
    } else {
        sample.displacement.x += delta.x;
        sample.displacement.y += delta.y;
        sample.seconds += input->seconds;
    }
    if (!finite_vector(sample.displacement) || !isfinite(sample.seconds)) {
        return ab_fail(error, "Sling mouse sample exceeds finite range");
    }
    state->samples[index] = sample;
    return true;
}

static bool held_drag(AbSlingMousePlan *plan, const AbSlingMouseInput *input,
                      AbError *error) {
    if (!sample_drag(&plan->next, input, error)) {
        return false;
    }
    append_call(plan, AB_SLING_MOUSE_IS_DRAGGED, (AbVec2){0});
    if (!plan->bird_dragged) {
        return true;
    }
    append_call(plan, AB_SLING_MOUSE_REST, (AbVec2){0});
    if (!finite_vector(input->rest)) {
        return ab_fail(error, "Invalid sling mouse rest position");
    }
    plan->next.start_physics = input->rest;
    if (input->bird_ready) {
        append_call(plan, AB_SLING_MOUSE_VISIBLE, (AbVec2){0});
        if (input->sling_visible) {
            AbVec2 stretch = {plan->next.start_physics.x - input->cursor_physics.x,
                              plan->next.start_physics.y - input->cursor_physics.y};
            if (!finite_vector(stretch)) {
                return ab_fail(error, "Sling mouse stretch exceeds finite range");
            }
            append_call(plan, AB_SLING_MOUSE_STRETCH, stretch);
        }
    }
    return true;
}

bool ab_game_sling_mouse_plan(const AbSlingMouseState *state,
                              const AbSlingMouseInput *input, AbSlingMousePlan *output,
                              AbError *error) {
    if (!state || !input || !output || state->sample_mask >> AB_SLING_MOUSE_SAMPLES) {
        return ab_fail(error, "Invalid sling mouse state storage");
    }
    AbSlingMousePlan plan = {.next = *state, .bird_dragged = input->bird_dragged};
    if (input->pressed && !input->level_completed && !input->skip_input &&
        !begin_press(&plan, input, error)) {
        return false;
    }
    if (input->held && !input->level_completed && plan.next.in_game_pressed &&
        !input->skip_input && plan.next.drag_started &&
        !held_drag(&plan, input, error)) {
        return false;
    }
    if ((input->released || !input->held) && !input->skip_input &&
        plan.next.drag_started) {
        plan.next.drag_started = false;
        append_call(&plan, AB_SLING_MOUSE_IS_DRAGGED, (AbVec2){0});
        if (plan.bird_dragged && !input->level_completed) {
            append_call(&plan, AB_SLING_MOUSE_RELEASE, (AbVec2){0});
        }
    }
    *output = plan;
    return true;
}
