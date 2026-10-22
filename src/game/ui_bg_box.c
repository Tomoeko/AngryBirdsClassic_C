#include "ab/ui_bg_box.h"

#include "ab/fill_rectangle.h"
#include "ab/game_rectangle.h"

#include <math.h>
#include <string.h>

static AbUiPointerValue number(float value) {
    AbUiPointerValue result = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&result.number_bits, &value, sizeof(value));
    return result;
}

static AbUiPointerValue text(const char *value) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                              .text = {(const uint8_t *)value, strlen(value)}};
}

static bool named(AbUiPointerValue value, const char *name) {
    return value.kind == AB_UI_POINTER_TEXT && value.text.size == strlen(name) &&
           (!value.text.size ||
            (value.text.data && !memcmp(value.text.data, name, value.text.size)));
}

static bool numeric(AbUiPointerValue field, float *output, AbError *error) {
    float value;
    memcpy(&value, &field.number_bits, sizeof(value));
    if (field.kind != AB_UI_POINTER_NUMBER || !isfinite(value)) {
        ab_fail(error, "Unsupported BGBox numeric field");
        return false;
    }
    *output = value;
    return true;
}

static bool valid(AbUiBgBox *box, AbError *error) {
    return box && box->widget.initialized && box->widget.context &&
                   !box->widget.context->failed
               ? true
               : ab_fail(error, "Invalid or failed BGBox");
}

static bool finish(AbUiBgBox *box, bool okay) {
    if (!okay)
        box->widget.context->failed = true;
    return okay;
}

static bool scaled(AbUiBgBox *box) {
    return ab_ui_pointer_value_truthy(&box->scale_it);
}

static float coordinate(const AbUiBgBox *box, float value) {
    return ab_ui_pointer_value_truthy(&box->floor_coordinates) ? floorf(value) : value;
}

static bool bounds(AbUiBgBox *box, AbUiPointerValue image, AbVec2 *output,
                   AbError *error) {
    if (!ab_ui_pointer_value_truthy(&image) || named(image, "")) {
        *output = (AbVec2){0};
        return true;
    }
    AbUiImageResourceBoundaries *images = &box->widget.context->images;
    return image.kind == AB_UI_POINTER_TEXT && images->bounds
               ? images->bounds(images->context, image.text, output, error)
               : ab_fail(error, "Missing BGBox sprite bounds");
}

static bool draw_component(AbUiBgBox *box, AbUiBoxComponent component, AbVec2 position,
                           AbVec2 size, bool sized, AbVerticalAnchor vertical,
                           AbHorizontalAnchor horizontal, AbError *error) {
    AbUiPointerValue image = box->components[component];
    if (named(image, ""))
        return true;
    if (image.kind != AB_UI_POINTER_TEXT || (image.text.size && !image.text.data))
        return ab_fail(error, "Unsupported BGBox component identifier");
    AbUiWidgetContext *context = box->widget.context;
    AbUiSpriteRender sprite = {image.text, position, size, vertical, horizontal, sized};
    return ab_ui_sprite_render(&sprite, context->render_state, &context->render,
                               context->draw_list, error);
}

bool ab_ui_bg_box_edge(AbUiBgBox *box, AbUiBoxComponent edge, float *output,
                       AbError *error) {
    if (!valid(box, error) || !output || edge < AB_UI_BOX_TOP ||
        edge > AB_UI_BOX_BOTTOM)
        return ab_fail(error, "Invalid BGBox edge query");
    AbVec2 size;
    if (!bounds(box, box->components[edge], &size, error))
        return finish(box, false);
    *output = edge == AB_UI_BOX_TOP || edge == AB_UI_BOX_BOTTOM ? size.y : size.x;
    return true;
}

bool ab_ui_bg_box_total(AbUiBgBox *box, bool horizontal, float *output,
                        AbError *error) {
    if (!valid(box, error) || !output)
        return false;
    float first, second, scale = 1;
    AbUiBoxComponent start = horizontal ? AB_UI_BOX_LEFT : AB_UI_BOX_TOP;
    AbUiBoxComponent end = horizontal ? AB_UI_BOX_RIGHT : AB_UI_BOX_BOTTOM;
    bool scale_it = scaled(box);
    float extent = scale_it ? (horizontal ? box->width : box->height) : 0;
    if (!ab_ui_bg_box_edge(box, start, &first, error))
        return false;
    if (!scale_it)
        extent = first + (horizontal ? box->width : box->height);
    if (!ab_ui_bg_box_edge(box, end, &second, error))
        return false;
    if (scale_it && !numeric(horizontal ? box->widget.state.image.frame.fields.scale_x
                                        : box->widget.state.image.frame.fields.scale_y,
                             &scale, error))
        return finish(box, false);
    float total = scale_it ? extent + (first + second) * scale : extent + second;
    if (!isfinite(total))
        return finish(box, ab_fail(error, "Nonfinite BGBox total size"));
    *output = total;
    return true;
}

bool ab_ui_bg_box_set_total(AbUiBgBox *box, bool horizontal, float total,
                            AbError *error) {
    if (!valid(box, error))
        return false;
    float scale = 1, first, second;
    AbUiPointerValue field = horizontal ? box->widget.state.image.frame.fields.scale_x
                                        : box->widget.state.image.frame.fields.scale_y;
    if (scaled(box) && ab_ui_pointer_value_truthy(&field) &&
        !numeric(field, &scale, error))
        return finish(box, false);
    if (!ab_ui_bg_box_edge(box, horizontal ? AB_UI_BOX_LEFT : AB_UI_BOX_TOP, &first,
                           error) ||
        !ab_ui_bg_box_edge(box, horizontal ? AB_UI_BOX_RIGHT : AB_UI_BOX_BOTTOM,
                           &second, error))
        return false;
    float extent = total - floorf((first + second) * scale);
    if (horizontal)
        box->width = extent;
    else
        box->height = extent;
    return true;
}

void ab_ui_bg_box_set_size(AbUiBgBox *box, float width, float height) {
    box->width = width;
    box->height = height;
}

void ab_ui_bg_box_use_scale(AbUiBgBox *box, AbUiPointerValue value) {
    box->scale_it = value;
}

static bool measure_components(AbUiBgBox *box, AbVec2 sizes[AB_UI_BOX_COMPONENT_COUNT],
                               AbError *error) {
    AbUiWidgetContext *context = box->widget.context;
    for (size_t index = 0; index < AB_UI_BOX_COMPONENT_COUNT; ++index) {
        bool vertical = index == AB_UI_BOX_TOP || index == AB_UI_BOX_BOTTOM;
        AbUiPointerValue *image = box->components + index;
        if (image->kind == AB_UI_POINTER_NONE) {
            *image = text("");
            continue;
        }
        if (index == AB_UI_BOX_CENTER)
            continue;
        /* drawBox tests nil, rather than truthiness, before its bounds query. */
        if (image->kind != AB_UI_POINTER_TEXT)
            return ab_fail(error, "Unsupported BGBox component bounds");
        if (!bounds(box, *image, sizes + index, error))
            return false;
        if (index >= AB_UI_BOX_TOP) {
            context->discarded_sprite_bound =
                number(vertical ? sizes[index].x : sizes[index].y);
        }
        if (scaled(box)) {
            float scale = 1;
            if ((index < AB_UI_BOX_CENTER || index == AB_UI_BOX_LEFT ||
                 index == AB_UI_BOX_RIGHT) &&
                !numeric(box->widget.state.image.frame.fields.scale_x, &scale, error))
                return false;
            if (index < AB_UI_BOX_CENTER || index == AB_UI_BOX_LEFT ||
                index == AB_UI_BOX_RIGHT)
                sizes[index].x = coordinate(box, sizes[index].x * scale);
            if ((index < AB_UI_BOX_CENTER || vertical) &&
                !numeric(box->widget.state.image.frame.fields.scale_y, &scale, error))
                return false;
            if (index < AB_UI_BOX_CENTER || vertical)
                sizes[index].y = coordinate(box, sizes[index].y * scale);
        }
    }
    return true;
}

static bool draw_sides(AbUiBgBox *box, AbVec2 point, AbVec2 offset, AbVec2 size,
                       const AbVec2 *pieces, AbError *error) {
    float x = point.x, y = point.y, width = size.x, height = size.y;
    float top = pieces[AB_UI_BOX_TOP].y, bottom = pieces[AB_UI_BOX_BOTTOM].y;
    float left = pieces[AB_UI_BOX_LEFT].x, right = pieces[AB_UI_BOX_RIGHT].x;
    float fwidth = coordinate(box, width), fheight = coordinate(box, height);
    float top_y = scaled(box) ? (y + offset.y) - top : (y - top) + offset.y;
    float left_x = scaled(box) ? coordinate(box, x + offset.x) - left
                               : coordinate(box, (x - left) + offset.x);
    return draw_component(
               box, AB_UI_BOX_TOP,
               (AbVec2){coordinate(box, x + offset.x), coordinate(box, top_y)},
               (AbVec2){fwidth, coordinate(box, top)}, true, AB_ANCHOR_TOP,
               AB_ANCHOR_LEFT, error) &&
           draw_component(box, AB_UI_BOX_BOTTOM,
                          (AbVec2){coordinate(box, x + offset.x),
                                   coordinate(box, (y + height) + offset.y)},
                          (AbVec2){fwidth, coordinate(box, bottom)}, true,
                          AB_ANCHOR_TOP, AB_ANCHOR_LEFT, error) &&
           draw_component(box, AB_UI_BOX_LEFT,
                          (AbVec2){left_x, coordinate(box, y + offset.y)},
                          (AbVec2){scaled(box) ? left : coordinate(box, left), fheight},
                          true, AB_ANCHOR_TOP, AB_ANCHOR_LEFT, error) &&
           draw_component(box, AB_UI_BOX_RIGHT,
                          (AbVec2){coordinate(box, (x + width) + offset.x),
                                   coordinate(box, y + offset.y)},
                          (AbVec2){coordinate(box, right), fheight}, true,
                          AB_ANCHOR_TOP, AB_ANCHOR_LEFT, error);
}

static bool draw_corners(AbUiBgBox *box, AbVec2 point, AbVec2 offset, AbVec2 size,
                         const AbVec2 *pieces, AbError *error) {
    float x = point.x, y = point.y, width = size.x, height = size.y;
    float left = pieces[AB_UI_BOX_LEFT].x, top = pieces[AB_UI_BOX_TOP].y;
    if (scaled(box)) {
        return draw_component(box, AB_UI_BOX_TOP_LEFT,
                              (AbVec2){coordinate(box, (x - left) + offset.x),
                                       coordinate(box, (y - top) + offset.y)},
                              pieces[AB_UI_BOX_TOP_LEFT], true, AB_ANCHOR_TOP,
                              AB_ANCHOR_LEFT, error) &&
               draw_component(box, AB_UI_BOX_TOP_RIGHT,
                              (AbVec2){coordinate(box, (x + width) + offset.x),
                                       coordinate(box, (y - top) + offset.y)},
                              pieces[AB_UI_BOX_TOP_RIGHT], true, AB_ANCHOR_TOP,
                              AB_ANCHOR_LEFT, error) &&
               draw_component(box, AB_UI_BOX_BOTTOM_LEFT,
                              (AbVec2){coordinate(box, (x - left) + offset.x),
                                       coordinate(box, (y + height) + offset.y)},
                              pieces[AB_UI_BOX_BOTTOM_LEFT], true, AB_ANCHOR_TOP,
                              AB_ANCHOR_LEFT, error) &&
               draw_component(box, AB_UI_BOX_BOTTOM_RIGHT,
                              (AbVec2){coordinate(box, (x + width) + offset.x),
                                       coordinate(box, (y + height) + offset.y)},
                              pieces[AB_UI_BOX_BOTTOM_RIGHT], true, AB_ANCHOR_TOP,
                              AB_ANCHOR_LEFT, error);
    }
    return draw_component(
               box, AB_UI_BOX_TOP_LEFT,
               (AbVec2){coordinate(box, x + offset.x), coordinate(box, y + offset.y)},
               (AbVec2){0}, false, AB_ANCHOR_BOTTOM, AB_ANCHOR_RIGHT, error) &&
           draw_component(box, AB_UI_BOX_TOP_RIGHT,
                          (AbVec2){coordinate(box, (x + width) + offset.x),
                                   coordinate(box, y + offset.y)},
                          (AbVec2){0}, false, AB_ANCHOR_BOTTOM, AB_ANCHOR_LEFT,
                          error) &&
           draw_component(box, AB_UI_BOX_BOTTOM_LEFT,
                          (AbVec2){coordinate(box, x + offset.x),
                                   coordinate(box, (y + height) + offset.y)},
                          (AbVec2){0}, false, AB_ANCHOR_TOP, AB_ANCHOR_RIGHT, error) &&
           draw_component(box, AB_UI_BOX_BOTTOM_RIGHT,
                          (AbVec2){coordinate(box, (x + width) + offset.x),
                                   coordinate(box, (y + height) + offset.y)},
                          (AbVec2){0}, false, AB_ANCHOR_TOP, AB_ANCHOR_LEFT, error);
}

static bool draw_center(AbUiBgBox *box, AbVec2 point, AbVec2 offset, AbVec2 size,
                        const AbUiPointerValue *color, uint32_t solid_texture,
                        AbError *error) {
    if (!color)
        return draw_component(
            box, AB_UI_BOX_CENTER,
            (AbVec2){coordinate(box, point.x + offset.x),
                     coordinate(box, point.y + offset.y)},
            (AbVec2){coordinate(box, size.x), coordinate(box, size.y)}, true,
            AB_ANCHOR_TOP, AB_ANCHOR_LEFT, error);
    AbGameRectangle rectangle = {
        .left = coordinate(box, point.x + offset.x),
        .top = coordinate(box, point.y + offset.y),
        .right = coordinate(box, ((point.x + size.x) - 1) + offset.x),
        .bottom = coordinate(box, ((point.y + size.y) - 1) + offset.y)};
    for (size_t channel = 0; channel < 4; ++channel) {
        rectangle.color[channel] = 1;
        if (color[channel].kind != AB_UI_POINTER_NONE &&
            !numeric(color[channel], rectangle.color + channel, error))
            return false;
    }
    AbUiWidgetContext *context = box->widget.context;
    AbGameRectanglePlan plan;
    if (!ab_game_rectangle_plan(&rectangle, &plan, error))
        return false;
    context->render_state->geometry = (AbSpriteRenderState){.scale_x = 1, .scale_y = 1};
    context->render_state->alpha = 1;
    AbDrawCommand command = {0};
    bool visible;
    if (!ab_fill_rectangle_reset_command(
            (AbFillRectangle){plan.argb, plan.left, plan.top, plan.width, plan.height,
                              AB_FILL_BLEND_AUTO},
            context->render.viewport_width, context->render.viewport_height,
            solid_texture, &command, &visible, error))
        return false;
    command.scissor = context->render_state->clip;
    return !visible || ab_draw_list_push(context->draw_list, &command, error);
}

bool ab_ui_bg_box_draw_box(AbUiBgBox *box, AbVec2 position, AbVec2 size,
                           const AbUiPointerValue *color, uint32_t solid_texture,
                           AbError *error) {
    if (!valid(box, error) || !box->widget.context->render_state ||
        !isfinite(position.x) || !isfinite(position.y) || !isfinite(size.x) ||
        !isfinite(size.y))
        return ab_fail(error, "Invalid BGBox draw geometry/context");
    AbVec2 pieces[AB_UI_BOX_COMPONENT_COUNT] = {0};
    if (!measure_components(box, pieces, error))
        return finish(box, false);
    float offset_x = named(box->horizontal_anchor, "HCENTER") ? -size.x / 2
                     : named(box->horizontal_anchor, "RIGHT") ? -size.x
                                                              : 0;
    float offset_y = named(box->vertical_anchor, "VCENTER")  ? -size.y / 2
                     : named(box->vertical_anchor, "BOTTOM") ? -size.y
                                                             : 0;
    /* Keep the original unrounded sums until each individual coordinate call. */
    AbVec2 offset = {offset_x, offset_y};
    float top = coordinate(box, (position.y - pieces[AB_UI_BOX_TOP].y) + offset_y);
    float bottom = coordinate(box, ((position.y + size.y) + offset_y) +
                                       pieces[AB_UI_BOX_BOTTOM].y);
    if (top > (float)box->widget.context->render.viewport_height || bottom < 0)
        return true;
    bool okay = draw_sides(box, position, offset, size, pieces, error) &&
                draw_corners(box, position, offset, size, pieces, error) &&
                draw_center(box, position, offset, size, color, solid_texture, error);
    return finish(box, okay);
}

bool ab_ui_bg_box_draw_self(AbUiBgBox *box, const AbUiDrawInput *input,
                            AbError *error) {
    if (!valid(box, error) || !box->widget.context->render_state)
        return ab_fail(error, "Missing BGBox render state");
    AbSpriteRenderState *state = &box->widget.context->render_state->geometry;
    state->translation_x = state->translation_y = state->angle_radians = 0;
    state->rotation_origin_x = state->rotation_origin_y = 0;
    state->scale_x = state->scale_y = 1;
    AbUiDrawTransform parent;
    float x, y;
    if (!ab_ui_draw_resolve(input, &parent) ||
        !numeric(box->widget.state.image.frame.fields.x, &x, error) ||
        !numeric(box->widget.state.image.frame.fields.y, &y, error))
        return finish(box, false);
    return ab_ui_bg_box_draw_box(
        box, (AbVec2){floorf(x + parent.position.x), floorf(y + parent.position.y)},
        (AbVec2){floorf(box->width), floorf(box->height)}, NULL, 0, error);
}

bool ab_ui_bg_box_draw_base(AbUiBgBox *box, const AbUiDrawInput *input,
                            AbError *error) {
    return ab_ui_bg_box_draw_self(box, input, error) &&
           ab_ui_widget_draw_base(&box->widget, input, error);
}

static bool draw(void *context, AbUiWidget *widget, const AbUiDrawInput *input,
                 AbError *error) {
    AbUiBgBox *box = context;
    return widget == &box->widget && ab_ui_bg_box_draw_base(box, input, error);
}

bool ab_ui_bg_box_init(AbUiBgBox *box, AbUiWidgetContext *context,
                       AbUiWidgetChildren children, const AbUiFrameFields *arguments,
                       AbError *error) {
    if (!box || box->widget.initialized)
        return ab_fail(error, "BGBox requires fresh caller storage");
    if (!ab_ui_widget_init(&box->widget, context, AB_UI_WIDGET_FRAME, children,
                           arguments, NULL, NULL, error))
        return false;
    box->widget.state.image.frame.fields.x = number(0);
    box->widget.state.image.frame.fields.y = number(0);
    box->width = box->height = 500;
    box->scale_it = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    static const char *const names[AB_UI_BOX_COMPONENT_COUNT] = {
        "POPUP_TOP_LEFT",     "POPUP_TOP_RIGHT", "POPUP_BOTTOM_LEFT",
        "POPUP_BOTTOM_RIGHT", "POPUP_CENTER",    "POPUP_TOP_MIDDLE",
        "POPUP_LEFT",         "POPUP_RIGHT",     "POPUP_BOTTOM_MIDDLE"};
    for (size_t index = 0; index < AB_UI_BOX_COMPONENT_COUNT; ++index)
        box->components[index] = text(names[index]);
    box->widget.methods = (AbUiWidgetMethods){.context = box, .draw = draw};
    return true;
}
