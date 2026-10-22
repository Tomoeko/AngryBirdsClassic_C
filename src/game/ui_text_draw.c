#include "ab/ui_text_draw.h"

#include <math.h>

bool ab_ui_text_on_screen(int32_t max_ascending, int32_t max_descending, float y,
                          float scale_y, int32_t vertical, int32_t screen_height) {
    float ascending = (float)max_ascending * scale_y;
    float descending = (float)max_descending * scale_y;
    float bottom = (y + ascending) + descending;
    if (vertical == AB_ANCHOR_BOTTOM) {
        float height = descending + ascending;
        y = y - height;
        bottom = bottom - height;
    } else if (vertical == AB_ANCHOR_VCENTER) {
        y = y - ascending;
        bottom = bottom - ascending;
    }
    return !(y >= (float)screen_height) && !(bottom < 0);
}

static bool valid_text(AbSpan value) {
    return !value.size || value.data;
}

static bool finite_point(AbVec2 point) {
    return isfinite(point.x) && isfinite(point.y);
}

static bool resolve_transform(AbVec2 position, AbVec2 scale, const float *arguments,
                              size_t count, AbUiDrawTransform *output) {
    if (count < 2 || count > 6 || !arguments || !finite_point(position) ||
        !finite_point(scale)) {
        return false;
    }
    for (size_t index = 0; index < count; ++index) {
        if (index == 2 && count == 3) {
            continue;
        }
        if (!isfinite(arguments[index])) {
            return false;
        }
    }
    float factor = count >= 6 ? arguments[5] : 1;
    float sx = scale.x * factor, sy = scale.y * factor;
    AbUiDrawTransform result = {.position = {arguments[0] + factor * position.x,
                                             arguments[1] + factor * position.y},
                                .scale = {(count >= 4 ? arguments[2] : 1) * sx,
                                          (count >= 4 ? arguments[3] : 1) * sy},
                                .angle = count >= 5 ? arguments[4] : 0,
                                .local_factor = factor};
    if (!finite_point(result.position) || !finite_point(result.scale) ||
        result.scale.x == 0 || result.scale.y == 0) {
        return false;
    }
    *output = result;
    return true;
}

static bool make_plan(const AbUiTextDraw *text, AbUiDrawTransform transform,
                      float current_alpha, AbUiTextDrawPlan *output) {
    AbUiTextDrawPlan plan = {0};
    if (text->clipped || text->color_present || !isfinite(current_alpha) ||
        (text->rotation_pivot_present & ~3u) ||
        (text->alpha_present && !isfinite(text->alpha)) ||
        (unsigned)text->vertical > AB_ANCHOR_VPIVOT ||
        (unsigned)text->horizontal > AB_ANCHOR_HPIVOT || !valid_text(text->group) ||
        !valid_text(text->text) || (text->font_present && !valid_text(text->font))) {
        return false;
    }
    plan.scale = transform.scale;
    AbVec2 position = transform.position;
    position.x = position.x / plan.scale.x;
    position.y = position.y / plan.scale.y;
    if (text->floor_coordinates) {
        position.x = floorf(position.x);
        position.y = floorf(position.y);
    }
    if (!finite_point(position)) {
        return false;
    }
    AbVec2 pivot = {0};
    if (text->rotation_pivot_present == 3) {
        if (!finite_point(text->rotation_pivot)) {
            return false;
        }
        pivot = text->rotation_pivot;
    }
    plan.angle = transform.angle;
    if (!ab_rotation_from_angle(plan.angle, &plan.rotation)) {
        return false;
    }
    plan.font =
        text->font_present ? text->font : (AbSpan){(const uint8_t *)"FONT_BASIC", 10};
    plan.group = text->group;
    plan.text = text->text;
    plan.alpha_override = text->alpha_present && text->alpha < 1;
    plan.draw_alpha = plan.alpha_override ? text->alpha : current_alpha;
    plan.after_alpha = plan.alpha_override ? 1 : current_alpha;
    plan.layout = (AbFontPlacement){
        .placement = {position.x, position.y, 1, 1, text->vertical, text->horizontal},
        .rotation_origin_x = pivot.x,
        .rotation_origin_y = pivot.y};
    plan.draw = true;
    *output = plan;
    return true;
}

bool ab_ui_text_draw_plan(const AbUiTextDraw *text, const float *arguments,
                          size_t count, float current_alpha, AbUiTextDrawPlan *output) {
    if (!text || !output) {
        return false;
    }
    if (!text->visible) {
        *output = (AbUiTextDrawPlan){0};
        return true;
    }
    AbUiDrawTransform transform;
    return resolve_transform(text->position, text->scale, arguments, count,
                             &transform) &&
           make_plan(text, transform, current_alpha, output);
}

bool ab_ui_text_clip_begin(const AbUiTextDraw *root, const float *arguments,
                           size_t count, AbUiTextClipFrame *output) {
    if (!root || !output) {
        return false;
    }
    AbUiTextClipFrame frame = {0};
    if (!root->visible) {
        *output = frame;
        return true;
    }
    AbUiDrawTransform transform;
    if (!root->clipped || !valid_text(root->group) ||
        !resolve_transform(root->position, root->scale, arguments, count, &transform)) {
        return false;
    }
    frame.parent_position = (AbVec2){arguments[0], arguments[1]};
    frame.position = root->position;
    frame.scale = transform.scale;
    frame.factor = transform.local_factor;
    frame.angle = transform.angle;
    frame.group = root->group;
    frame.visible = true;
    *output = frame;
    return true;
}

bool ab_ui_text_clip_line_plan(const AbUiTextClipFrame *frame, const AbUiTextDraw *line,
                               const AbBitmapFont *current_font, int32_t screen_height,
                               float current_alpha, AbUiTextDrawPlan *output) {
    if (!frame || !line || !output) {
        return false;
    }
    AbUiTextDrawPlan plan = {0};
    if (!frame->visible) {
        *output = plan;
        return true;
    }
    if (!current_font || !finite_point(frame->parent_position) ||
        !finite_point(frame->position) || !finite_point(frame->scale) ||
        !isfinite(frame->factor) || !isfinite(frame->angle) ||
        !valid_text(frame->group) || !finite_point(line->position) ||
        !valid_text(line->text) || (unsigned)line->vertical > AB_ANCHOR_VPIVOT ||
        (unsigned)line->horizontal > AB_ANCHOR_HPIVOT) {
        return false;
    }
    AbVec2 position = {frame->parent_position.x +
                           (frame->position.x + line->position.x) * frame->factor,
                       frame->parent_position.y +
                           (frame->position.y + line->position.y) * frame->factor};
    if (!finite_point(position)) {
        return false;
    }
    if (!ab_ui_text_on_screen(current_font->max_ascending, current_font->max_descending,
                              position.y, frame->scale.y, (int32_t)line->vertical,
                              screen_height)) {
        *output = plan;
        return true;
    }
    AbUiTextDraw resolved = *line;
    resolved.group = frame->group;
    const float arguments[] = {0, 0, frame->scale.x, frame->scale.y, frame->angle, 1};
    AbUiDrawTransform transform;
    if (!resolve_transform((AbVec2){0, 0}, line->scale, arguments, 6, &transform)) {
        return false;
    }
    /* Supplied anchor/position bypasses native self-position evaluation. Keep
     * signed zero rather than adding an artificial zero translation. */
    transform.position = position;
    return make_plan(&resolved, transform, current_alpha, output);
}
