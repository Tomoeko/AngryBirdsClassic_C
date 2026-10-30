#include "ab/ui_widget.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static bool truthy(AbUiPointerValue value) {
    return ab_ui_pointer_value_truthy(&value);
}

bool ab_ui_widget_set_screen_clip(void *context, AbScreenClipRect clip,
                                  AbError *error) {
    AbUiWidgetContext *widgets = context;
    if (!widgets || widgets->failed || !widgets->render_state)
        return ab_fail(error, "Missing concrete UI clip context");
    return ab_screen_clip(clip, widgets->screen_clip_offset,
                          &widgets->render_state->clip, error);
}

static AbUiPointerValue number(float value) {
    AbUiPointerValue result = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&result.number_bits, &value, sizeof(value));
    return result;
}

static bool numeric(AbUiPointerValue value, float *output, AbError *error) {
    float result;
    memcpy(&result, &value.number_bits, sizeof(result));
    if (value.kind != AB_UI_POINTER_NUMBER || !isfinite(result))
        return ab_fail(error, "Nonfinite or nonnumeric concrete UI field");
    *output = result;
    return true;
}

static bool valid(const AbUiWidget *widget, AbError *error) {
    return widget && widget->initialized && widget->context &&
                   !widget->context->failed && widget->frame &&
                   widget->tree.state == widget->frame &&
                   (widget->kind == AB_UI_WIDGET_FRAME ||
                    widget->frame == &widget->state.image.frame) &&
                   widget->tree.context == widget &&
                   widget->tree.children == widget->children.frames &&
                   widget->tree.capacity == widget->children.capacity &&
                   widget->tree.state->child_count <= widget->tree.capacity &&
                   widget->tree.state->lookup_count == widget->tree.state->child_count
               ? true
               : ab_fail(error, "Invalid, foreign or failed concrete UI widget");
}

static bool finish(AbUiWidget *widget, bool okay) {
    if (!okay)
        widget->context->failed = true;
    return okay;
}

static AbUiWidget *child_at(AbUiWidget *widget, size_t index, AbError *error) {
    AbUiFrameNode *frame = widget->children.frames[index];
    AbUiWidget *child = frame ? frame->context : NULL;
    if (!valid(child, error) || &child->tree != frame ||
        child->context != widget->context) {
        ab_fail(error, "Foreign concrete UI child");
        return NULL;
    }
    return child;
}

static bool enter_call(AbUiWidget *widget, bool *running, AbError *error) {
    if (!valid(widget, error) || *running ||
        widget->context->depth >= widget->context->maximum_depth)
        return ab_fail(error, "Reentrant or excessively deep concrete UI call");
    *running = true;
    ++widget->context->depth;
    return true;
}

static bool leave_call(AbUiWidget *widget, bool *running, bool okay) {
    --widget->context->depth;
    *running = false;
    return finish(widget, okay);
}

static bool default_name(void *context, AbUiPointerValue *output, AbError *error) {
    (void)error;
    AbUiWidget *widget = context;
    *output = (AbUiPointerValue){
        .kind = AB_UI_POINTER_TEXT,
        .text = {(const uint8_t *)widget->default_name, strlen(widget->default_name)}};
    return true;
}

static bool lifecycle(void *context, AbUiFrameNode *node, AbUiFrameMethod method,
                      AbError *error) {
    AbUiWidget *widget = context;
    if (!valid(widget, error) || node != &widget->tree)
        return false;
    if (widget->context->depth >= widget->context->maximum_depth)
        return ab_fail(error, "Excessively deep concrete UI lifecycle");
    ++widget->context->depth;
    bool okay =
        widget->methods.lifecycle
            ? widget->methods.lifecycle(widget->methods.context, widget, method, error)
            : ab_ui_widget_lifecycle_base(widget, method, error);
    --widget->context->depth;
    return finish(widget, okay);
}

static bool
initialize_widget(AbUiWidget *widget, AbUiWidgetContext *context, AbUiWidgetKind kind,
                  AbUiWidgetChildren children, const AbUiFrameFields *frame,
                  const AbUiImageArguments *image,
                  const AbUiImageButtonArguments *button, AbUiImageInitSetter setter,
                  AbUiFrameState *external_frame, AbUiWidgetFrameInitializer initialize,
                  void *initialize_context, AbError *error) {
    if (!widget || widget->initialized || !context || context->failed ||
        !context->maximum_depth || (unsigned)kind > AB_UI_WIDGET_IMAGE_BUTTON ||
        children.capacity > UINT32_C(16777216) ||
        (children.capacity &&
         (!children.frames || !children.draw || !children.pointer)))
        return ab_fail(error, "Invalid concrete UI constructor/storage");
    *widget = (AbUiWidget){.context = context, .kind = kind, .children = children};
    widget->frame = external_frame ? external_frame : &widget->state.image.frame;
    int count = snprintf(widget->default_name, sizeof(widget->default_name),
                         "table: %p", (void *)widget);
    if (count < 0 || (size_t)count >= sizeof(widget->default_name))
        return finish(widget, ab_fail(error, "Concrete UI name exceeds storage"));
    AbUiImageInitBoundaries boundaries = {
        .frame = {.context = widget, .default_name = default_name},
        .resources = context->images,
        .empty_padding = {.kind = AB_UI_POINTER_OBJECT,
                          .object = (uintptr_t)widget->padding}};
    bool okay = initialize ? initialize(initialize_context, &boundaries.frame, error)
                : kind == AB_UI_WIDGET_FRAME
                    ? ab_ui_frame_init(&widget->state.image.frame, frame,
                                       &boundaries.frame, error)
                : kind == AB_UI_WIDGET_IMAGE
                    ? ab_ui_image_state_init_derived(&widget->state.image, image,
                                                     &boundaries, setter, error)
                    : ab_ui_image_button_state_init_derived(&widget->state, button,
                                                            &boundaries, setter, error);
    widget->tree = (AbUiFrameNode){.state = widget->frame,
                                   .children = children.frames,
                                   .capacity = children.capacity,
                                   .context = widget,
                                   .method = lifecycle};
    widget->initialized = okay;
    return finish(widget, okay);
}

bool ab_ui_widget_init_derived(AbUiWidget *widget, AbUiWidgetContext *context,
                               AbUiWidgetKind kind, AbUiWidgetChildren children,
                               const AbUiFrameFields *frame,
                               const AbUiImageArguments *image,
                               const AbUiImageButtonArguments *button,
                               AbUiImageInitSetter setter, AbError *error) {
    return initialize_widget(widget, context, kind, children, frame, image, button,
                             setter, NULL, NULL, NULL, error);
}

bool ab_ui_widget_init_frame(AbUiWidget *widget, AbUiWidgetContext *context,
                             AbUiWidgetChildren children, AbUiFrameState *frame,
                             AbUiWidgetFrameInitializer initialize,
                             void *initialize_context, AbError *error) {
    if (!frame || !initialize)
        return ab_fail(error, "Missing derived UI Frame state/initializer");
    return initialize_widget(widget, context, AB_UI_WIDGET_FRAME, children, NULL, NULL,
                             NULL, (AbUiImageInitSetter){0}, frame, initialize,
                             initialize_context, error);
}

bool ab_ui_widget_init(AbUiWidget *widget, AbUiWidgetContext *context,
                       AbUiWidgetKind kind, AbUiWidgetChildren children,
                       const AbUiFrameFields *frame, const AbUiImageArguments *image,
                       const AbUiImageButtonArguments *button, AbError *error) {
    return ab_ui_widget_init_derived(widget, context, kind, children, frame, image,
                                     button, (AbUiImageInitSetter){0}, error);
}

bool ab_ui_widget_add(AbUiWidget *parent, AbUiWidget *child, AbError *error) {
    if (!valid(parent, error) || !valid(child, error) ||
        parent->context != child->context)
        return ab_fail(error, "Invalid concrete UI parent/child");
    return finish(parent,
                  ab_ui_frame_add_child(&parent->tree, &child->tree, 0, false, error));
}

bool ab_ui_widget_insert(AbUiWidget *parent, AbUiWidget *child, size_t position,
                         AbError *error) {
    if (!valid(parent, error) || !valid(child, error) ||
        parent->context != child->context)
        return ab_fail(error, "Invalid concrete UI insertion parent/child");
    return finish(parent, ab_ui_frame_add_child(&parent->tree, &child->tree, position,
                                                true, error));
}

bool ab_ui_widget_lifecycle_base(AbUiWidget *widget, AbUiFrameMethod method,
                                 AbError *error) {
    if (!valid(widget, error))
        return false;
    switch (method) {
        case AB_UI_FRAME_ENTRY:
            return widget->kind == AB_UI_WIDGET_IMAGE_BUTTON
                       ? ab_ui_image_button_state_enter(&widget->state, &widget->tree,
                                                        &widget->context->lifecycle,
                                                        error)
                       : ab_ui_frame_enter(&widget->tree, &widget->context->lifecycle,
                                           error);
        case AB_UI_FRAME_EXIT:
            return ab_ui_frame_exit(&widget->tree, &widget->context->lifecycle, error);
        case AB_UI_FRAME_LAYOUT:
            return (widget->kind == AB_UI_WIDGET_FRAME ||
                    ab_ui_image_state_refresh(&widget->state.image,
                                              &widget->context->images, error)) &&
                   ab_ui_frame_layout_children(&widget->tree, error);
    }
    return ab_fail(error, "Unknown concrete UI lifecycle method");
}

bool ab_ui_widget_lifecycle(AbUiWidget *widget, AbUiFrameMethod method,
                            AbError *error) {
    if (!valid(widget, error))
        return false;
    return finish(widget, lifecycle(widget, &widget->tree, method, error));
}

static bool draw_child(void *context, AbSpan method, const AbUiDrawTransform *transform,
                       AbError *error) {
    if (method.size != 4 || memcmp(method.data, "draw", 4))
        return ab_fail(error, "Unsupported concrete UI draw method");
    AbUiDrawInput input = {.transform = *transform, .present = 63};
    return ab_ui_widget_draw(context, &input, error);
}

static bool draw_children(AbUiWidget *widget, AbError *error) {
    for (size_t index = 0; index < widget->tree.state->child_count; ++index) {
        AbUiWidget *child = child_at(widget, index, error);
        if (!child)
            return false;
        widget->children.draw[index] =
            (AbUiDrawChild){.callback = draw_child,
                            .context = child,
                            .tag = child->tag,
                            .visible = truthy(child->tree.state->fields.visible)};
    }
    return true;
}

static bool image_sink(void *context, const AbUiImageDrawPlan *plan, AbError *error) {
    AbUiWidgetContext *runtime = ((AbUiWidget *)context)->context;
    return ab_ui_image_render_plan(plan, runtime->render_state, &runtime->render,
                                   runtime->draw_list, error);
}

bool ab_ui_widget_draw_self(AbUiWidget *widget, const AbUiDrawInput *input,
                            AbError *error) {
    if (!valid(widget, error) || widget->kind == AB_UI_WIDGET_FRAME)
        return ab_fail(error, "Missing concrete UI image self");
    AbVec2 screen = {(float)widget->context->render.viewport_width,
                     (float)widget->context->render.viewport_height};
    if (widget->kind == AB_UI_WIDGET_IMAGE_BUTTON &&
        widget->context->animation_installed)
        return ab_ui_image_button_state_draw_self(&widget->state, input, screen,
                                                  image_sink, widget, error);
    AbUiImageDraw image;
    AbUiImageDrawPlan plan;
    if (!ab_ui_image_state_draw_view(&widget->state.image, &image) ||
        !ab_ui_image_draw_plan(&image, input, screen, &plan))
        return ab_fail(error, "Unsupported concrete UI image self draw");
    return image_sink(widget, &plan, error);
}

bool ab_ui_widget_draw_with_tag(AbUiWidget *widget, const AbUiDrawInput *input,
                                const AbUiPointerValue *tag, AbError *error) {
    if (!valid(widget, error) || !draw_children(widget, error))
        return false;
    AbUiDrawFrame frame;
    if (!ab_ui_frame_state_draw_view(widget->tree.state, &frame))
        return ab_fail(error, "Unsupported concrete UI tagged Frame geometry");
    return ab_ui_frame_draw(&frame, widget->children.draw,
                            widget->tree.state->child_count, input, tag, NULL, error);
}

bool ab_ui_widget_draw_base(AbUiWidget *widget, const AbUiDrawInput *input,
                            AbError *error) {
    if (!valid(widget, error) || !draw_children(widget, error))
        return false;
    AbVec2 screen = {(float)widget->context->render.viewport_width,
                     (float)widget->context->render.viewport_height};
    if (widget->kind == AB_UI_WIDGET_IMAGE_BUTTON &&
        widget->context->animation_installed)
        return ab_ui_image_button_state_draw(&widget->state, widget->children.draw,
                                             widget->tree.state->child_count, input,
                                             screen, image_sink, widget, error);
    AbUiDrawFrame frame;
    if (!ab_ui_frame_state_draw_view(widget->tree.state, &frame))
        return ab_fail(error, "Unsupported concrete UI frame geometry");
    if (widget->kind != AB_UI_WIDGET_FRAME) {
        AbUiImageDraw image;
        AbUiImageDrawPlan plan;
        if (!ab_ui_image_state_draw_view(&widget->state.image, &image) ||
            !ab_ui_image_draw_plan(&image, input, screen, &plan))
            return ab_fail(error, "Unsupported concrete UI image draw");
        if (!image_sink(widget, &plan, error))
            return false;
    }
    return ab_ui_frame_draw(&frame, widget->children.draw,
                            widget->tree.state->child_count, input, NULL, NULL, error);
}

bool ab_ui_widget_draw(AbUiWidget *widget, const AbUiDrawInput *input, AbError *error) {
    if (!widget || !enter_call(widget, &widget->drawing, error))
        return false;
    bool okay = widget->methods.draw ? widget->methods.draw(widget->methods.context,
                                                            widget, input, error)
                                     : ab_ui_widget_draw_base(widget, input, error);
    return leave_call(widget, &widget->drawing, okay);
}

bool ab_ui_widget_draw_named(AbUiWidget *widget, AbSpan method,
                             const AbUiDrawInput *input, AbError *error) {
    if (method.size == 4 && method.data && !memcmp(method.data, "draw", 4))
        return ab_ui_widget_draw(widget, input, error);
    if (!widget || !enter_call(widget, &widget->drawing, error))
        return false;
    bool okay = widget->methods.named_draw && (!method.size || method.data)
                    ? widget->methods.named_draw(widget->methods.context, widget,
                                                 method, input, error)
                    : ab_fail(error, "Missing concrete UI named draw method");
    return leave_call(widget, &widget->drawing, okay);
}

static bool geometry(AbUiWidget *widget, size_t depth, AbError *error) {
    if (!valid(widget, error) || depth >= widget->context->maximum_depth)
        return ab_fail(error, "Invalid concrete UI geometry chain");
    AbUiDrawFrame frame;
    if (!ab_ui_frame_state_draw_view(widget->tree.state, &frame))
        return ab_fail(error, "Unsupported concrete UI hit geometry");
    widget->geometry =
        (AbUiFrameGeometry){.position = frame.position, .scale = frame.scale};
    if (truthy(widget->scroll[0]) && truthy(widget->scroll[1])) {
        if (!numeric(widget->scroll[0], &widget->geometry.scroll.x, error) ||
            !numeric(widget->scroll[1], &widget->geometry.scroll.y, error))
            return false;
        widget->geometry.scroll_present = 3;
    }
    if (widget->tree.parent) {
        AbUiWidget *parent = widget->tree.parent->context;
        if (!parent || &parent->tree != widget->tree.parent ||
            parent->context != widget->context || !geometry(parent, depth + 1, error))
            return false;
        widget->geometry.parent = &parent->geometry;
    }
    return true;
}

static bool query_geometry(AbUiWidget *widget, size_t depth, bool positions,
                           AbError *error);

static bool hit(void *context, AbVec2 point, bool *output, AbError *error) {
    AbUiWidget *widget = context;
    if (!query_geometry(widget, 0, false, error))
        return false;
    AbUiImageState *image = &widget->state.image;
    AbUiImageHit query = {.frame = &widget->geometry,
                          .image_present = truthy(image->image)};
    AbVec2 scale;
    if (!ab_ui_frame_cumulative_scale(&widget->geometry, widget->context->maximum_depth,
                                      &scale))
        return ab_fail(error, "Invalid image hit-test scale chain");
    if (!query.image_present || scale.x == 0 || scale.y == 0) {
        *output = false;
        return true;
    }
    if (!geometry(widget, 0, error))
        return false;
    float padding[4] = {0};
    if (truthy(widget->state.padding)) {
        if (widget->state.padding.kind != AB_UI_POINTER_OBJECT ||
            widget->state.padding.object != (uintptr_t)widget->padding)
            return ab_fail(error, "Foreign concrete UI padding object");
        for (size_t index = 0; index < 4; ++index)
            if (truthy(widget->padding[index]) &&
                !numeric(widget->padding[index], padding + index, error))
                return false;
    }
    query.padding_left = padding[0];
    query.padding_right = padding[1];
    query.padding_top = padding[2];
    query.padding_bottom = padding[3];
    AbUiImageResourceBoundaries *resources = &widget->context->images;
    if (image->image.kind != AB_UI_POINTER_TEXT || !resources->bounds ||
        !resources->pivot)
        return ab_fail(error, "Missing image hit-test resource boundaries");
    if (!resources->bounds(resources->context, image->image.text, &query.sprite_size,
                           error) ||
        !resources->pivot(resources->context, image->image.text, &query.sprite_pivot,
                          error) ||
        !numeric(image->frame.fields.angle, &query.angle_radians, error))
        return false;
    return ab_ui_image_button_hit(&query, widget->context->maximum_depth, point,
                                  output) ||
           ab_fail(error, "Unsupported concrete UI button hit test");
}

bool ab_ui_widget_image_hit(AbUiWidget *widget, AbVec2 point, bool *output,
                            AbError *error) {
    if (!widget || !output || widget->kind == AB_UI_WIDGET_FRAME)
        return ab_fail(error, "Image hit test requires an Image-derived receiver");
    return hit(widget, point, output, error);
}

static bool audio(void *context, AbUiPointerValue sound, float volume, bool loop,
                  AbError *error) {
    AbUiWidgetContext *runtime = ((AbUiWidget *)context)->context;
    return runtime->play_audio
               ? runtime->play_audio(runtime->audio_context, sound, volume, loop, error)
               : ab_fail(error, "Missing concrete UI audio service");
}

static bool pointer_child(void *context, AbSpan event, AbVec2 point,
                          AbUiPointerReply *output, AbError *error) {
    return ab_ui_widget_pointer(context, event, point, output, error);
}

bool ab_ui_widget_pointer_base(AbUiWidget *widget, AbSpan event, AbVec2 point,
                               AbUiPointerReply *output, AbError *error) {
    if (!valid(widget, error))
        return false;
    for (size_t index = 0; index < widget->tree.state->child_count; ++index) {
        AbUiWidget *child = child_at(widget, index, error);
        if (!child)
            return false;
        widget->children.pointer[index] = (AbUiPointerChild){
            pointer_child, child, truthy(child->tree.state->fields.visible),
            truthy(child->tree.state->fields.active)};
    }
    AbVec2 frame_position;
    if (!numeric(widget->tree.state->fields.x, &frame_position.x, error) ||
        !numeric(widget->tree.state->fields.y, &frame_position.y, error))
        return false;
    AbUiImageButtonBoundaries boundaries = {widget,
                                            hit,
                                            audio,
                                            widget->context->device_ipad,
                                            widget->context->touch_count,
                                            frame_position,
                                            widget->children.pointer,
                                            widget->tree.state->child_count};
    return widget->kind == AB_UI_WIDGET_IMAGE_BUTTON
               ? ab_ui_image_button_state_pointer(&widget->state, (uintptr_t)widget,
                                                  &boundaries, event, point, output,
                                                  error)
               : ab_ui_frame_pointer(frame_position, widget->children.pointer,
                                     widget->tree.state->child_count, event, point,
                                     output, error);
}

bool ab_ui_widget_pointer(AbUiWidget *widget, AbSpan event, AbVec2 point,
                          AbUiPointerReply *output, AbError *error) {
    if (!widget || !enter_call(widget, &widget->pointing, error))
        return false;
    bool okay = widget->methods.pointer
                    ? widget->methods.pointer(widget->methods.context, widget, event,
                                              point, output, error)
                    : ab_ui_widget_pointer_base(widget, event, point, output, error);
    return leave_call(widget, &widget->pointing, okay);
}

static bool update_children(void *context, float seconds, float time, AbError *error) {
    AbUiWidget *widget = context;
    for (size_t index = 0; index < widget->tree.state->child_count; ++index) {
        AbUiWidget *child = child_at(widget, index, error);
        if (!child)
            return false;
        if (truthy(child->tree.state->fields.active) &&
            !ab_ui_widget_update(child, seconds, time, error))
            return false;
    }
    return true;
}

static bool screen_position(void *context, AbVec2 *output, AbError *error) {
    AbUiWidget *widget = context;
    return geometry(widget, 0, error) &&
           (ab_ui_frame_screen_position(&widget->geometry,
                                        widget->context->maximum_depth, output) ||
            ab_fail(error, "Unsupported concrete UI screen position"));
}

static bool query_geometry(AbUiWidget *widget, size_t depth, bool positions,
                           AbError *error) {
    if (!valid(widget, error) || depth >= widget->context->maximum_depth)
        return ab_fail(error, "Invalid concrete UI geometry query chain");
    AbUiFrameFields *fields = &widget->tree.state->fields;
    widget->geometry = (AbUiFrameGeometry){.scale = {1, 1}};
    if (positions) {
        if (!numeric(fields->x, &widget->geometry.position.x, error) ||
            !numeric(fields->y, &widget->geometry.position.y, error))
            return false;
        if (depth && truthy(widget->scroll[0]) && truthy(widget->scroll[1])) {
            if (!numeric(widget->scroll[0], &widget->geometry.scroll.x, error) ||
                !numeric(widget->scroll[1], &widget->geometry.scroll.y, error))
                return false;
            widget->geometry.scroll_present = 3;
        }
    } else if (!numeric(fields->scale_x, &widget->geometry.scale.x, error) ||
               !numeric(fields->scale_y, &widget->geometry.scale.y, error))
        return false;
    if (widget->tree.parent) {
        AbUiWidget *parent = widget->tree.parent->context;
        if (!parent || &parent->tree != widget->tree.parent ||
            parent->context != widget->context ||
            !query_geometry(parent, depth + 1, positions, error))
            return false;
        widget->geometry.parent = &parent->geometry;
    }
    return true;
}

bool ab_ui_widget_screen_position(AbUiWidget *widget, AbVec2 *output, AbError *error) {
    return output && query_geometry(widget, 0, true, error) &&
           (ab_ui_frame_screen_position(&widget->geometry,
                                        widget->context->maximum_depth, output) ||
            ab_fail(error, "Unsupported concrete UI screen position"));
}

bool ab_ui_widget_cumulative_scale(AbUiWidget *widget, AbVec2 *output, AbError *error) {
    return output && query_geometry(widget, 0, false, error) &&
           (ab_ui_frame_cumulative_scale(&widget->geometry,
                                         widget->context->maximum_depth, output) ||
            ab_fail(error, "Unsupported concrete UI cumulative scale"));
}

static AbUiHoverState hover_state(AbUiPointerValue value) {
    static const char *const names[] = {"NONE", "ENTER", "HOVER", "LEAVE"};
    for (size_t index = 0; index < 4; ++index)
        if (value.kind == AB_UI_POINTER_TEXT &&
            value.text.size == strlen(names[index]) &&
            !memcmp(value.text.data, names[index], value.text.size))
            return (AbUiHoverState)index;
    return AB_UI_HOVER_OTHER;
}

typedef struct {
    AbUiWidget *widget;
    AbUiImageButtonAnimation *animation;
} AnimationCall;

static bool animation_screen(void *context, AbVec2 *output, AbError *error) {
    return screen_position(((AnimationCall *)context)->widget, output, error);
}

static bool animation_hit(void *context, AbVec2 point, bool *output, AbError *error) {
    AnimationCall *call = context;
    AbUiFrameFields *fields = &call->widget->state.image.frame.fields;
    AbUiPointerValue old_x = fields->x, old_y = fields->y;
    fields->x = number(call->animation->position.x);
    fields->y = number(call->animation->position.y);
    bool okay = hit(call->widget, point, output, error);
    fields->x = old_x;
    fields->y = old_y;
    return okay;
}

bool ab_ui_widget_update_base(AbUiWidget *widget, float seconds, float time,
                              AbError *error) {
    if (!valid(widget, error))
        return false;
    if (!update_children(widget, seconds, time, error))
        return false;
    if (widget->kind != AB_UI_WIDGET_IMAGE_BUTTON ||
        !widget->context->animation_installed)
        return true;
    AbUiImageButtonState *state = &widget->state;
    if (truthy(state->clear_pointer_flags))
        ab_ui_image_button_state_clear_flags(state);
    AbUiImageButton button = {.click_started = truthy(state->click_started),
                              .should_clear_pointer_flags =
                                  truthy(state->clear_pointer_flags),
                              .enabled = state->enabled};
    AbUiImageButtonAnimation animation = {
        .hover_state = hover_state(state->hover_state),
        .hover_enabled = truthy(state->hover_enabled),
        .scale_on_touch = truthy(state->scale_on_touch)};
    if (!numeric(state->image.frame.fields.x, &animation.position.x, error) ||
        !numeric(state->image.frame.fields.y, &animation.position.y, error) ||
        !numeric(state->hover_scale, &animation.hover_scale, error))
        return false;
    AnimationCall call = {widget, &animation};
    AbUiImageButtonAnimationBoundaries boundaries = {
        .context = &call,
        .screen_position = animation_screen,
        .hit_test = animation_hit,
        .cursor = widget->context->cursor,
        .scale_buttons_on_touch = widget->context->scale_buttons_on_touch,
        .left_button_held = widget->context->left_button_held};
    bool okay = ab_ui_image_button_update_after_image(&button, &animation, &boundaries,
                                                      seconds, error);
    /* Publish only fields assigned by the selected source path. The input
     * hover count and unrecognized hover-state values retain their raw types. */
    state->hover_scale = number(animation.hover_scale);
    static const char *const names[] = {"NONE", "ENTER", "HOVER", "LEAVE"};
    if (animation.hover_state != hover_state(state->hover_state) &&
        (unsigned)animation.hover_state < 4)
        state->hover_state =
            (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                               .text = {(const uint8_t *)names[animation.hover_state],
                                        strlen(names[animation.hover_state])}};
    return okay;
}

bool ab_ui_widget_update(AbUiWidget *widget, float seconds, float time,
                         AbError *error) {
    if (!widget || !isfinite(seconds) || !isfinite(time) ||
        !enter_call(widget, &widget->updating, error))
        return false;
    bool okay = widget->methods.update
                    ? widget->methods.update(widget->methods.context, widget, seconds,
                                             time, error)
                    : ab_ui_widget_update_base(widget, seconds, time, error);
    return leave_call(widget, &widget->updating, okay);
}

bool ab_ui_widget_clear_pointer_flags(AbUiWidget *widget, AbError *error) {
    if (!valid(widget, error) ||
        widget->context->depth >= widget->context->maximum_depth)
        return false;
    ++widget->context->depth;
    if (widget->kind == AB_UI_WIDGET_IMAGE_BUTTON) {
        ab_ui_image_button_state_clear_flags(&widget->state);
        --widget->context->depth;
        return true;
    }
    bool okay = true;
    for (size_t index = 0; okay && index < widget->tree.state->child_count; ++index) {
        AbUiWidget *child = child_at(widget, index, error);
        okay = child && ab_ui_widget_clear_pointer_flags(child, error);
    }
    --widget->context->depth;
    return finish(widget, okay);
}
