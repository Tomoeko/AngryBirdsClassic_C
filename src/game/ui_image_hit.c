#include "ab/ui_image_hit.h"

#include "ab/rotation.h"
#include "ab/script_math.h"

#include <math.h>

static bool finite_point(AbVec2 point) {
    return isfinite(point.x) && isfinite(point.y);
}

bool ab_ui_rectangle_hit(const AbUiHitRectangle *rectangle, AbVec2 point,
                         bool *output) {
    if (!rectangle || !output || !finite_point(rectangle->center) ||
        !finite_point(rectangle->size) || !finite_point(point)) {
        return false;
    }
    AbRotation rotation;
    if (!ab_rotation_from_angle(rectangle->angle_radians, &rotation)) {
        return false;
    }
    if (rectangle->angle_radians == 0) {
        rotation = (AbRotation){0, 1};
    }
    float x = point.x - rectangle->center.x;
    float y = point.y - rectangle->center.y;
    float local_x = rotation.sine * y;
    local_x = local_x + rotation.cosine * x;
    float local_y = rotation.cosine * y;
    local_y = local_y + (-rotation.sine) * x;
    if (!isfinite(local_x) || !isfinite(local_y)) {
        return false;
    }
    if (local_x < 0) {
        local_x = -local_x;
    }
    if (local_y < 0) {
        local_y = -local_y;
    }
    *output = rectangle->size.x * 0.5f > local_x && rectangle->size.y * 0.5f > local_y;
    return true;
}

static AbVec2 rotate_corner(AbVec2 point, AbVec2 origin, float sine, float cosine) {
    float x = origin.x + (point.x - origin.x) * cosine;
    x = x - (point.y - origin.y) * sine;
    float y = origin.y + (point.x - origin.x) * sine;
    y = y + (point.y - origin.y) * cosine;
    return (AbVec2){x, y};
}

bool ab_ui_image_button_rectangle(const AbUiImageHit *button, size_t maximum_depth,
                                  AbUiHitRectangle *rectangle, bool *present) {
    if (!button || !button->frame || !rectangle || !present) {
        return false;
    }
    AbVec2 scale;
    if (!ab_ui_frame_cumulative_scale(button->frame, maximum_depth, &scale)) {
        return false;
    }
    scale.x = fabsf(scale.x);
    scale.y = fabsf(scale.y);
    if (scale.x == 0 || scale.y == 0 || !button->image_present) {
        *present = false;
        return true;
    }
    float sine, cosine;
    if (!finite_point(button->frame->position) || !finite_point(button->sprite_size) ||
        !finite_point(button->sprite_pivot) || !isfinite(button->padding_left) ||
        !isfinite(button->padding_right) || !isfinite(button->padding_top) ||
        !isfinite(button->padding_bottom) ||
        !ab_script_sine(button->angle_radians, &sine) ||
        !ab_script_cosine(button->angle_radians, &cosine)) {
        return false;
    }
    float left = button->padding_left * scale.x;
    float right = button->padding_right * scale.x;
    float top = button->padding_top * scale.y;
    float bottom = button->padding_bottom * scale.y;
    float width = button->sprite_size.x * scale.x;
    width = (width + left) + right;
    float height = button->sprite_size.y * scale.y;
    height = (height + top) + bottom;
    AbVec2 pivot = {button->sprite_pivot.x * scale.x, button->sprite_pivot.y * scale.y};
    AbVec2 minimum = {(button->frame->position.x - pivot.x) - left,
                      (button->frame->position.y - pivot.y) - top};
    AbVec2 maximum = {minimum.x + width, minimum.y + height};
    minimum = rotate_corner(minimum, button->frame->position, sine, cosine);
    maximum = rotate_corner(maximum, button->frame->position, sine, cosine);
    AbUiHitRectangle result = {
        .center = {minimum.x + (maximum.x - minimum.x) / 2,
                   minimum.y + (maximum.y - minimum.y) / 2},
        .size = {width, height},
        .angle_radians = button->angle_radians,
    };
    if (!finite_point(result.center) || !finite_point(result.size)) {
        return false;
    }
    *rectangle = result;
    *present = true;
    return true;
}

bool ab_ui_image_button_hit(const AbUiImageHit *button, size_t maximum_depth,
                            AbVec2 point, bool *output) {
    AbUiHitRectangle rectangle;
    bool present;
    if (!output ||
        !ab_ui_image_button_rectangle(button, maximum_depth, &rectangle, &present)) {
        return false;
    }
    if (!present) {
        *output = false;
        return true;
    }
    return ab_ui_rectangle_hit(&rectangle, point, output);
}
