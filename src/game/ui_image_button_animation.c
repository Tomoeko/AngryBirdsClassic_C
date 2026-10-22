#include "ab/ui_image_button_animation.h"

#include <math.h>

static bool finite_point(AbVec2 point) {
    return isfinite(point.x) && isfinite(point.y);
}

static bool hover_count(AbUiImageButtonAnimation *animation, bool entering) {
    if (!animation || !isfinite(animation->hover_count)) {
        return false;
    }
    float count = entering ? animation->hover_count + 1 : animation->hover_count - 1;
    if (!isfinite(count)) {
        return false;
    }
    animation->hover_count = count;
    if (animation->hover_enabled && count == (entering ? 1 : 0)) {
        animation->hover_state = entering ? AB_UI_HOVER_ENTER : AB_UI_HOVER_LEAVE;
    }
    return true;
}

bool ab_ui_image_button_mouse_enter(AbUiImageButtonAnimation *animation) {
    return hover_count(animation, true);
}

bool ab_ui_image_button_mouse_leave(AbUiImageButtonAnimation *animation) {
    return hover_count(animation, false);
}

bool ab_ui_image_button_hover_scale(const AbUiImageButtonAnimation *animation,
                                    float *output) {
    if (!animation || !output) {
        return false;
    }
    float result = 1;
    if (animation->hover_enabled) {
        if (!isfinite(animation->hover_scale)) {
            return false;
        }
        result = 0.1f * animation->hover_scale;
        result = result / 1;
        result = result + 1;
        if (!isfinite(result)) {
            return false;
        }
    }
    *output = result;
    return true;
}

static bool touch_scale(AbUiImageButton *button, AbUiImageButtonAnimation *animation,
                        const AbUiImageButtonAnimationBoundaries *boundaries,
                        float seconds, bool *pressed, AbError *error) {
    AbVec2 screen;
    if (!boundaries->screen_position ||
        !boundaries->screen_position(boundaries->context, &screen, error)) {
        return ab_fail(error, "Missing or failed ImageButton screen-position query");
    }
    if (!finite_point(screen)) {
        return ab_fail(error, "Nonfinite ImageButton screen position");
    }
    bool hit = false;
    AbVec2 saved = animation->position;
    animation->position = screen;
    if (button->click_started && boundaries->left_button_held &&
        ab_ui_pointer_value_truthy(&button->enabled)) {
        if (!boundaries->hit_test || !finite_point(boundaries->cursor)) {
            animation->position = saved;
            return ab_fail(error, "Invalid ImageButton animation hit-test boundary");
        }
        bool okay =
            boundaries->hit_test(boundaries->context, boundaries->cursor, &hit, error);
        animation->position = saved;
        if (!okay) {
            return false;
        }
    } else {
        animation->position = saved;
    }
    float change = seconds * 20;
    if (hit) {
        float next = animation->hover_scale - change;
        if (!isfinite(change) || !isfinite(next)) {
            return ab_fail(error, "Nonfinite ImageButton press scale");
        }
        animation->hover_scale = next > -1 ? next : -1;
        *pressed = true;
    } else if (animation->hover_state == AB_UI_HOVER_NONE) {
        float next = animation->hover_scale + change;
        if (!isfinite(change) || !isfinite(next)) {
            return ab_fail(error, "Nonfinite ImageButton scale recovery");
        }
        animation->hover_scale = next < 0 ? next : 0;
    }
    return true;
}

bool ab_ui_image_button_update_after_image(
    AbUiImageButton *button, AbUiImageButtonAnimation *animation,
    const AbUiImageButtonAnimationBoundaries *boundaries, float seconds,
    AbError *error) {
    if (!button || !animation || !boundaries || !isfinite(seconds) ||
        !finite_point(animation->position) || !isfinite(animation->hover_scale) ||
        (unsigned)animation->hover_state > AB_UI_HOVER_OTHER ||
        !ab_ui_pointer_value_valid(&button->enabled)) {
        return ab_fail(error, "Invalid ImageButton animation input");
    }
    if (button->should_clear_pointer_flags) {
        ab_ui_image_button_clear_flags(button);
    }
    bool pressed = false;
    if (boundaries->scale_buttons_on_touch && animation->scale_on_touch &&
        !touch_scale(button, animation, boundaries, seconds, &pressed, error)) {
        return false;
    }
    if (!pressed && (animation->hover_state == AB_UI_HOVER_ENTER ||
                     animation->hover_state == AB_UI_HOVER_LEAVE)) {
        float change = seconds * 6.66f;
        float next = animation->hover_state == AB_UI_HOVER_ENTER
                         ? animation->hover_scale + change
                         : animation->hover_scale - change;
        if (!isfinite(change) || !isfinite(next)) {
            return ab_fail(error, "Nonfinite ImageButton hover transition");
        }
        animation->hover_scale = next;
        if (animation->hover_state == AB_UI_HOVER_ENTER && next >= 1) {
            animation->hover_scale = 1;
            animation->hover_state = AB_UI_HOVER_HOVER;
        } else if (animation->hover_state == AB_UI_HOVER_LEAVE && next <= 0) {
            animation->hover_scale = 0;
            animation->hover_state = AB_UI_HOVER_NONE;
        }
    }
    return true;
}

bool ab_ui_image_button_update(AbUiImageButton *button,
                               AbUiImageButtonAnimation *animation,
                               const AbUiImageButtonAnimationBoundaries *boundaries,
                               float seconds, float game_time, AbError *error) {
    if (!button || !animation || !boundaries || !boundaries->image_update ||
        !isfinite(seconds) || !isfinite(game_time) ||
        !finite_point(animation->position) || !isfinite(animation->hover_scale) ||
        (unsigned)animation->hover_state > AB_UI_HOVER_OTHER ||
        !ab_ui_pointer_value_valid(&button->enabled))
        return ab_fail(error, "Invalid ImageButton animation input");
    return boundaries->image_update(boundaries->context, seconds, game_time, error) &&
           ab_ui_image_button_update_after_image(button, animation, boundaries, seconds,
                                                 error);
}
