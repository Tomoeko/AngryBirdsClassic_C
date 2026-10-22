#include "ab/ui_image_draw.h"

#include <math.h>
#include <string.h>

static bool finite_point(AbVec2 point) {
    return isfinite(point.x) && isfinite(point.y);
}

static float quotient(float first, float second) {
    float result = first / second;
    if (isnan(result)) {
        uint32_t word = UINT32_C(0x7fc00000);
        memcpy(&result, &word, sizeof(result));
    }
    return result;
}

static bool outside(AbVec2 position, AbVec2 scale, AbVec2 size, AbVec2 pivot,
                    AbVec2 screen) {
    if (position.x >= 0 && position.x <= screen.x && position.y >= 0 &&
        position.y <= screen.y) {
        return false;
    }
    float width = floorf(size.x), height = floorf(size.y);
    float scaled_width = width * scale.x, scaled_height = height * scale.y;
    float horizontal = quotient(pivot.x, width), vertical = quotient(pivot.y, height);
    float right = position.x + (1 - horizontal) * scaled_width;
    float left = position.x - horizontal * scaled_width;
    float bottom = position.y + (1 - vertical) * scaled_height;
    float top = position.y - vertical * scaled_height;
    return right < 0 || screen.x < left || bottom < 0 || screen.y < top;
}

static void render_state(AbUiImageDrawPlan *plan, AbVec2 translation, AbVec2 scale,
                         float angle, AbVec2 pivot, bool alpha_present, float alpha) {
    AbUiImageDrawCall *call = plan->calls + plan->count++;
    call->kind = AB_UI_IMAGE_RENDER_STATE;
    float arguments[] = {translation.x, translation.y, scale.x, scale.y,
                         angle,         pivot.x,       pivot.y, alpha};
    memcpy(call->state.arguments, arguments, sizeof(arguments));
    call->state.count = alpha_present ? 8 : 7;
}

bool ab_ui_image_draw_plan(const AbUiImageDraw *image, const AbUiDrawInput *input,
                           AbVec2 screen_size, AbUiImageDrawPlan *output) {
    AbUiDrawTransform parent;
    if (!image || !output || !ab_ui_draw_resolve(input, &parent) ||
        (image->pivot_present & ~15u) || !finite_point(image->frame.position) ||
        !finite_point(image->frame.scale) || !isfinite(image->frame.angle) ||
        !finite_point(screen_size) ||
        (image->alpha_present && !isfinite(image->alpha))) {
        return false;
    }
    AbVec2 scale = {parent.scale.x * image->frame.scale.x,
                    parent.scale.y * image->frame.scale.y};
    scale.x = scale.x * parent.local_factor;
    scale.y = scale.y * parent.local_factor;
    AbVec2 position = {
        parent.position.x + image->frame.position.x * parent.local_factor,
        parent.position.y + image->frame.position.y * parent.local_factor};
    float angle = parent.angle + image->frame.angle;
    if (!finite_point(scale) || !finite_point(position) || !isfinite(angle)) {
        return false;
    }
    AbUiImageDrawPlan plan = {0};
    if (image->image_present && image->visible) {
        if ((image->image.size && !image->image.data) || !finite_point(image->size)) {
            return false;
        }
        AbVec2 pivot = {image->pivot_present & 1 ? image->pivot.x : 0,
                        image->pivot_present & 2 ? image->pivot.y : 0};
        AbVec2 rotation_pivot = {
            image->pivot_present & 4 ? image->rotation_pivot.x : pivot.x,
            image->pivot_present & 8 ? image->rotation_pivot.y : pivot.y};
        if (!finite_point(pivot) || !finite_point(rotation_pivot)) {
            return false;
        }
        if (image->floor_coordinates) {
            position.x = floorf(position.x);
            position.y = floorf(position.y);
        }
        if (angle == 0 &&
            outside(position, scale, image->size, rotation_pivot, screen_size)) {
            plan.culled = true;
            *output = plan;
            return true;
        }
        AbVec2 translation = {quotient(position.x, scale.x),
                              quotient(position.y, scale.y)};
        render_state(&plan, translation, scale, angle, rotation_pivot,
                     image->alpha_present, image->alpha);
        plan.calls[plan.count++] = (AbUiImageDrawCall){
            .kind = AB_UI_IMAGE_SPRITE,
            .sprite = {image->image, {floorf(image->size.x), floorf(image->size.y)}}};
    }
    render_state(&plan, (AbVec2){0, 0}, (AbVec2){1, 1}, 0, (AbVec2){0, 0},
                 image->alpha_present, 1);
    *output = plan;
    return true;
}
