#include "ab/ui_text_widget.h"

#include "ab/ui_text_clipping.h"
#include "ab/ui_value_text.h"

#include <math.h>
#include <string.h>

typedef struct {
    AbUiTextWidget *text;
    const AbUiFrameFields *frame;
    const AbUiTextArguments *arguments;
    AbUiInitBoundaries names, resources;
} TextInitializer;

static bool default_name(void *context, AbUiPointerValue *output, AbError *error) {
    TextInitializer *call = context;
    return call->names.default_name(call->names.context, output, error);
}

static bool set_font(void *context, AbUiPointerValue font, AbError *error) {
    TextInitializer *call = context;
    return call->resources.set_font(call->resources.context, font, error);
}

static bool localize(void *context, AbUiPointerValue group, AbUiPointerValue label,
                     AbUiPointerValue *output, AbError *error) {
    TextInitializer *call = context;
    return call->resources.localize(call->resources.context, group, label, output,
                                    error);
}

static bool measure(void *context, AbUiPointerValue label, AbUiPointerValue *output,
                    AbError *error) {
    TextInitializer *call = context;
    return call->resources.measure(call->resources.context, label, output, error);
}

static bool call_font(void *context, uintptr_t function, AbUiPointerValue *output,
                      AbError *error) {
    TextInitializer *call = context;
    return call->resources.call_font(call->resources.context, function, output, error);
}

static bool initialize(void *context, const AbUiInitBoundaries *names, AbError *error) {
    TextInitializer *call = context;
    call->names = *names;
    AbUiInitBoundaries resources = call->resources;
    resources.context = call;
    resources.default_name = default_name;
    resources.set_font = set_font;
    resources.localize = localize;
    resources.measure = measure;
    resources.call_font = call_font;
    return ab_ui_text_init(&call->text->state, call->frame, call->arguments, &resources,
                           error);
}

static bool numeric(AbUiPointerValue value, float *output, AbError *error) {
    if (value.kind != AB_UI_POINTER_NUMBER) {
        ab_fail(error, "Nonnumeric concrete Text geometry");
        return false;
    }
    memcpy(output, &value.number_bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite concrete Text geometry");
}

static bool equal(AbUiPointerValue value, const char *name) {
    return value.kind == AB_UI_POINTER_TEXT && value.text.size == strlen(name) &&
           value.text.data && !memcmp(value.text.data, name, value.text.size);
}

static bool draw_view(AbUiTextWidget *text, uint8_t numeric_text[32],
                      AbUiTextDraw *output, AbError *error) {
    AbUiTextState *state = &text->state;
    AbUiTextDraw view = {.visible =
                             ab_ui_pointer_value_truthy(&state->frame.fields.visible)};
    if (!view.visible) {
        *output = view;
        return true;
    }
    view.clipped = text->clipped.kind == AB_UI_POINTER_BOOLEAN && text->clipped.boolean;
    if (!view.clipped && ab_ui_pointer_value_truthy(&text->color))
        return ab_fail(error, "Concrete colored Text is not integrated yet");
    if (!numeric(state->frame.fields.x, &view.position.x, error) ||
        !numeric(state->frame.fields.y, &view.position.y, error) ||
        !numeric(state->frame.fields.scale_x, &view.scale.x, error) ||
        !numeric(state->frame.fields.scale_y, &view.scale.y, error))
        return false;
    if (state->fields.group.kind != AB_UI_POINTER_TEXT ||
        (!view.clipped && state->fields.font.kind != AB_UI_POINTER_NONE &&
         state->fields.font.kind != AB_UI_POINTER_TEXT))
        return ab_fail(error, "Unsupported concrete Text draw resource value");
    view.group = state->fields.group.text;
    if (view.clipped) {
        *output = view;
        return true;
    }
    if (equal(state->fields.horizontal_anchor, "LEFT"))
        view.horizontal = AB_ANCHOR_LEFT;
    else if (equal(state->fields.horizontal_anchor, "HCENTER"))
        view.horizontal = AB_ANCHOR_HCENTER;
    else if (equal(state->fields.horizontal_anchor, "RIGHT"))
        view.horizontal = AB_ANCHOR_RIGHT;
    else
        return ab_fail(error, "Unsupported concrete Text horizontal anchor");
    if (equal(state->fields.vertical_anchor, "TOP"))
        view.vertical = AB_ANCHOR_TOP;
    else if (equal(state->fields.vertical_anchor, "VCENTER"))
        view.vertical = AB_ANCHOR_VCENTER;
    else if (equal(state->fields.vertical_anchor, "BOTTOM"))
        view.vertical = AB_ANCHOR_BOTTOM;
    else
        return ab_fail(error, "Unsupported concrete Text vertical anchor");
    if (!view.clipped &&
        !ab_ui_native_value_text(state->fields.text, numeric_text, 32, &view.text))
        return ab_fail(error, "Unsupported concrete native Text value conversion");
    view.font = state->fields.font.text;
    view.font_present = state->fields.font.kind == AB_UI_POINTER_TEXT;
    view.floor_coordinates = ab_ui_pointer_value_truthy(&state->floor_coordinates);
    view.alpha_present = text->alpha.kind == AB_UI_POINTER_NUMBER;
    if (view.alpha_present && !numeric(text->alpha, &view.alpha, error))
        return false;
    if (text->rotation_pivot_x.kind == AB_UI_POINTER_NUMBER &&
        text->rotation_pivot_y.kind == AB_UI_POINTER_NUMBER) {
        if (!numeric(text->rotation_pivot_x, &view.rotation_pivot.x, error) ||
            !numeric(text->rotation_pivot_y, &view.rotation_pivot.y, error))
            return false;
        view.rotation_pivot_present = 3;
    }
    *output = view;
    return true;
}

static bool draw_clipped(AbUiTextWidget *text, const AbUiTextDraw *view,
                         const float *arguments, size_t count, AbError *error) {
    AbUiTextClipping *clipping = text->clipping;
    if (!clipping || !clipping->initialized || clipping->busy || clipping->failed ||
        clipping->text != text || !clipping->generation.clipped)
        return ab_fail(error, "Missing actual clipped Text line owner");
    AbUiTextClipFrame frame;
    if (!ab_ui_text_clip_begin(view, arguments, count, &frame))
        return ab_fail(error, "Unsupported concrete clipped Text transform");
    AbUiWidgetContext *context = text->widget.context;
    if (context->render.viewport_height > INT32_MAX)
        return ab_fail(error, "Clipped Text viewport exceeds native signed range");
    for (size_t index = 0; index < clipping->generation.count; ++index) {
        /* Text.clip owns standalone builtin Text states in lines, independently
         * of Frame.children. Native recursion reads those states without calling
         * their methods or applying their visible fields. */
        AbUiTextWidget line = {.state = clipping->generation.lines[index]};
        line.state.frame.fields.visible =
            (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
        uint8_t numeric_text[32];
        AbUiTextDraw leaf;
        if (!draw_view(&line, numeric_text, &leaf, error))
            return false;
        if (!text->fonts->current || !text->fonts->current->font)
            return ab_fail(error, "Missing current clipped Text visibility font");
        AbUiTextDrawPlan plan;
        if (!ab_ui_text_clip_line_plan(&frame, &leaf, text->fonts->current->font,
                                       (int32_t)context->render.viewport_height,
                                       context->render_state->alpha, &plan))
            return ab_fail(error, "Unsupported concrete clipped Text line plan");
        if (!ab_ui_font_resources_draw(text->fonts, context, &plan, error))
            return false;
    }
    return true;
}

static bool draw(void *context, AbUiWidget *widget, const AbUiDrawInput *input,
                 AbError *error) {
    AbUiTextWidget *text = context;
    AbUiTextDraw view;
    uint8_t numeric_text[32];
    if (!draw_view(text, numeric_text, &view, error))
        return false;
    AbUiTextDrawPlan plan;
    float arguments[6] = {0};
    size_t count = 0;
    if (view.visible) {
        AbUiDrawTransform parent;
        if (!ab_ui_draw_resolve(input, &parent))
            return ab_fail(error, "Unsupported concrete Text parent transform");
        const float values[] = {parent.position.x, parent.position.y,
                                parent.scale.x,    parent.scale.y,
                                parent.angle,      parent.local_factor};
        memcpy(arguments, values, sizeof(arguments));
        count = 6;
    }
    if (view.visible && view.clipped) {
        if (!draw_clipped(text, &view, arguments, count, error))
            return false;
    } else {
        if (!ab_ui_text_draw_plan(&view, arguments, count,
                                  widget->context->render_state->alpha, &plan))
            return ab_fail(error, "Unsupported concrete Text draw plan");
        if (!ab_ui_font_resources_draw(text->fonts, widget->context, &plan, error))
            return false;
    }
    return ab_ui_widget_draw_with_tag(widget, input, NULL, error);
}

static bool lifecycle(void *context, AbUiWidget *widget, AbUiFrameMethod method,
                      AbError *error) {
    AbUiTextWidget *text = context;
    if (method != AB_UI_FRAME_LAYOUT)
        return ab_ui_widget_lifecycle_base(widget, method, error);
    AbUiTextMetricBoundaries boundaries = ab_ui_font_resources_boundaries(text->fonts);
    return ab_ui_text_layout(&text->state, &boundaries.resources, error);
}

bool ab_ui_text_widget_init(AbUiTextWidget *text, AbUiWidgetContext *context,
                            AbUiWidgetChildren children, AbUiFontResources *fonts,
                            const AbUiFrameFields *frame,
                            const AbUiTextArguments *arguments, AbError *error) {
    if (!text || !fonts || !context || !context->render_state)
        return ab_fail(error, "Missing concrete Text state/resources/context");
    text->fonts = fonts;
    TextInitializer call = {.text = text,
                            .frame = frame,
                            .arguments = arguments,
                            .resources =
                                ab_ui_font_resources_boundaries(fonts).resources};
    if (!ab_ui_widget_init_frame(&text->widget, context, children, &text->state.frame,
                                 initialize, &call, error))
        return false;
    text->widget.methods =
        (AbUiWidgetMethods){.context = text, .lifecycle = lifecycle, .draw = draw};
    return true;
}

bool ab_ui_text_widget_set_font(AbUiTextWidget *text, AbUiPointerValue font,
                                AbError *error) {
    if (!text || !text->widget.initialized || text->widget.context->failed)
        return ab_fail(error, "Invalid concrete Text font setter");
    AbUiTextMetricBoundaries boundaries = ab_ui_font_resources_boundaries(text->fonts);
    bool okay = ab_ui_text_set_font(&text->state, font, &boundaries.resources, error);
    if (!okay)
        text->widget.context->failed = true;
    return okay;
}

bool ab_ui_text_widget_get_width(AbUiTextWidget *text, float *output, AbError *error) {
    if (!text || !text->widget.initialized || text->widget.context->failed)
        return ab_fail(error, "Invalid concrete Text width query");
    AbUiTextMetricBoundaries boundaries = ab_ui_font_resources_boundaries(text->fonts);
    return ab_ui_text_get_width(&text->state, &boundaries, output, error);
}

bool ab_ui_text_widget_get_height(AbUiTextWidget *text, float *output, AbError *error) {
    if (!text || !text->widget.initialized || text->widget.context->failed)
        return ab_fail(error, "Invalid concrete Text height query");
    AbUiTextMetricBoundaries boundaries = ab_ui_font_resources_boundaries(text->fonts);
    size_t count = 0;
    if (ab_ui_pointer_value_truthy(&text->clipped)) {
        if (!text->clipping || text->clipping->failed || text->clipping->busy ||
            text->clipping->text != text)
            return ab_fail(error, "Missing actual Text lines for height query");
        count = text->clipping->generation.count;
    }
    return ab_ui_text_get_height(&text->state, text->clipped, count, &boundaries,
                                 output, error);
}
