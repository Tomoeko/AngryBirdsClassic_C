#include "ab/ui_slider.h"
#include "ab/camera_mover.h"
#include "ab/ui_image_button_animation.h"

#include <math.h>
#include <string.h>

static AbUiPointerValue number(float value) {
    AbUiPointerValue result = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&result.number_bits, &value, sizeof(value));
    return result;
}

static AbUiPointerValue boolean(bool value) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = value};
}

static AbUiPointerValue text(const char *value) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                              .text = {(const uint8_t *)value, strlen(value)}};
}

static bool numeric(AbUiPointerValue value, float *output, AbError *error) {
    if (value.kind != AB_UI_POINTER_NUMBER)
        return ab_fail(error, "Nonnumeric reached Slider field");
    memcpy(output, &value.number_bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite reached Slider field");
}

static bool optional(AbUiPointerValue value, float fallback, float *output,
                     AbError *error) {
    *output = fallback;
    return !ab_ui_pointer_value_truthy(&value) || numeric(value, output, error);
}

static bool valid(AbUiSlider *slider, AbError *error) {
    return (slider && slider->box.widget.initialized && slider->box.widget.context &&
            !slider->box.widget.context->failed) ||
           ab_fail(error, "Invalid or stopped Slider");
}

static bool finish(AbUiSlider *slider, bool okay) {
    if (!okay)
        slider->box.widget.context->failed = true;
    return okay;
}

static bool refresh(AbUiSlider *slider, AbError *error) {
    bool closed = slider->state == AB_UI_SLIDER_CLOSED;
    AbUiImageResourceBoundaries *images = &slider->box.widget.context->images;
    return ab_ui_image_button_state_set_image(&slider->toggle.state,
                                              closed ? slider->closed_image
                                                     : slider->open_image,
                                              (AbUiPointerValue){0}, images, error) &&
           ab_ui_image_state_set_image(&slider->indicator.state.image,
                                       closed ? slider->closed_indicator
                                              : slider->open_indicator,
                                       images, error);
}

bool ab_ui_slider_images(AbUiSlider *slider, AbUiPointerValue open,
                         AbUiPointerValue closed, bool indicator, AbError *error) {
    if (!valid(slider, error))
        return false;
    if (indicator) {
        slider->open_indicator = open;
        slider->closed_indicator = closed;
    } else {
        slider->closed_image = closed;
        slider->open_image = open;
    }
    return finish(slider, refresh(slider, error));
}

bool ab_ui_slider_enabled(AbUiSlider *slider, AbUiPointerValue enabled,
                          AbError *error) {
    if (!valid(slider, error) || !ab_ui_pointer_value_valid(&enabled))
        return false;
    slider->enabled = enabled;
    if (slider->state != AB_UI_SLIDER_CLOSED)
        slider->state = AB_UI_SLIDER_CLOSING;
    return finish(slider, ab_ui_image_button_state_enable(
                              &slider->toggle.state, enabled,
                              &slider->box.widget.context->images, error));
}

bool ab_ui_slider_direction(AbUiSlider *slider, AbSpan direction, AbError *error) {
    if (!valid(slider, error) || (direction.size && !direction.data))
        return false;
    const char *const names[] = {"UP", "DOWN", "LEFT", "RIGHT"};
    const AbVec2 vectors[] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
    const char *const horizontal[] = {"HCENTER", "HCENTER", "BOTTOM", "TOP"};
    const char *const vertical[] = {"RIGHT", "LEFT", "VCENTER", "VCENTER"};
    for (size_t index = 0; index < 4; ++index) {
        if (direction.size != strlen(names[index]) ||
            memcmp(direction.data, names[index], direction.size))
            continue;
        slider->direction = vectors[index];
        slider->box.horizontal_anchor = text(horizontal[index]);
        slider->box.vertical_anchor = text(vertical[index]);
        break;
    }
    return true;
}

bool ab_ui_slider_curve(AbUiSlider *slider, AbUiSliderCurve curve, AbError *error) {
    if (!valid(slider, error))
        return false;
    if ((unsigned)curve > AB_UI_SLIDER_CUBIC_IN)
        return finish(slider, ab_fail(error, "Unsupported Slider opening curve"));
    slider->open_curve = curve;
    return true;
}

bool ab_ui_slider_is_open(const AbUiSlider *slider) {
    return slider && (slider->state == AB_UI_SLIDER_OPEN ||
                      slider->state == AB_UI_SLIDER_OPENING);
}

static bool transition(AbUiSlider *slider, bool open, AbError *error) {
    if (!valid(slider, error))
        return false;
    if (open) {
        if (slider->state == AB_UI_SLIDER_CLOSED ||
            slider->state == AB_UI_SLIDER_CLOSING)
            slider->state = AB_UI_SLIDER_OPENING;
    } else if (ab_ui_slider_is_open(slider))
        slider->state = AB_UI_SLIDER_CLOSING;
    return finish(slider, refresh(slider, error));
}

bool ab_ui_slider_open(AbUiSlider *slider, AbError *error) {
    return transition(slider, true, error);
}

bool ab_ui_slider_close(AbUiSlider *slider, AbError *error) {
    return transition(slider, false, error);
}

bool ab_ui_slider_toggle(AbUiSlider *slider, AbError *error) {
    return transition(slider, !ab_ui_slider_is_open(slider), error);
}

bool ab_ui_slider_disappear(AbUiSlider *slider, AbError *error) {
    if (!valid(slider, error))
        return false;
    slider->state = AB_UI_SLIDER_DISAPPEAR;
    return true;
}

bool ab_ui_slider_total(AbUiSlider *slider, float *output, AbError *error) {
    if (!valid(slider, error) || !output)
        return false;
    float total = slider->padding.x * fabsf(slider->direction.x);
    total += slider->padding.y * fabsf(slider->direction.y);
    for (size_t index = 0; index < slider->count; ++index) {
        float size;
        if (!slider->items[index] || !optional(slider->items[index]->override_size,
                                               slider->child_size, &size, error))
            return finish(slider, false);
        total += size;
        total += slider->gap;
    }
    total = ceilf(total);
    if (!isfinite(total))
        return finish(slider, ab_fail(error, "Slider total exceeds finite range"));
    *output = total;
    return true;
}

bool ab_ui_slider_duration(AbUiSlider *slider, float *output, AbError *error) {
    float total;
    if (!output || !ab_ui_slider_total(slider, &total, error))
        return false;
    float duration = total / (slider->host.screen->y * 0.8f);
    if (!isfinite(duration))
        return finish(slider, ab_fail(error, "Slider duration exceeds finite range"));
    *output = duration;
    return true;
}

bool ab_ui_slider_add(AbUiSlider *slider, AbUiSliderChild *child, AbError *error) {
    if (!valid(slider, error) || !child || !child->widget ||
        slider->count == slider->capacity)
        return ab_fail(error, "Invalid Slider content or exhausted storage");
    if (!ab_ui_widget_insert(&slider->box.widget, child->widget, slider->count + 1,
                             error))
        return finish(slider, false);
    slider->items[slider->count++] = child;
    return true;
}

bool ab_ui_slider_button_bounds(AbUiSlider *slider, float output[4], AbError *error) {
    if (!valid(slider, error) || !output)
        return false;
    float x, y, pivot_x, pivot_y, width, height;
    AbUiFrameFields *frame = &slider->box.widget.frame->fields;
    AbUiFrameFields *button = &slider->toggle.frame->fields;
    if (!numeric(frame->x, &x, error) || !numeric(frame->y, &y, error) ||
        !numeric(slider->toggle.state.image.resource_pivot_x, &pivot_x, error) ||
        !numeric(slider->toggle.state.image.resource_pivot_y, &pivot_y, error) ||
        !numeric(button->w, &width, error) || !numeric(button->h, &height, error))
        return finish(slider, false);
    float bounds[] = {x - pivot_x, y - pivot_y, (x - pivot_x) + width,
                      (y - pivot_y) + height};
    for (size_t index = 0; index < 4; ++index)
        if (!isfinite(bounds[index]))
            return finish(slider,
                          ab_fail(error, "Slider button bounds exceed finite range"));
    memcpy(output, bounds, sizeof(bounds));
    return true;
}

static bool curve(AbUiSlider *slider, float change, float duration, float *output,
                  AbError *error) {
    if (slider->open_curve == AB_UI_SLIDER_CUBIC_IN) {
        float progress = slider->extend_state / duration;
        float value = change * progress;
        value *= progress;
        value *= progress;
        value += 0;
        if (!isfinite(value))
            return ab_fail(error, "Slider cubic-in curve exceeds finite domain");
        *output = value;
        return true;
    }
    if (slider->open_curve == AB_UI_SLIDER_LINEAR) {
        float value = change * slider->extend_state;
        value /= duration;
        value += 0;
        if (!isfinite(value))
            return ab_fail(error, "Slider linear curve exceeds finite domain");
        *output = value;
        return true;
    }
    return ab_camera_tween(slider->open_curve == AB_UI_SLIDER_CUBIC_OUT
                               ? AB_CAMERA_TWEEN_CUBIC_OUT
                               : AB_CAMERA_TWEEN_CUBIC_IN_OUT,
                           slider->extend_state, 0, change, duration, output) ||
           ab_fail(error, "Slider opening curve exceeds numeric domain");
}

static bool animate(AbUiSlider *slider, float seconds, float total, AbError *error) {
    bool opening = slider->state == AB_UI_SLIDER_OPENING;
    bool closing =
        slider->state == AB_UI_SLIDER_CLOSING ||
        (slider->state == AB_UI_SLIDER_DISAPPEAR && slider->extend_state > 0);
    if (opening || closing) {
        float step = seconds * slider->host.screen->y;
        step *= 0.8f;
        slider->extend_state =
            opening ? slider->extend_state + step : slider->extend_state - step;
        if (opening && total <= slider->extend_state) {
            slider->extend_state = total;
            slider->state = AB_UI_SLIDER_OPEN;
            if (!refresh(slider, error))
                return false;
        } else if (!opening && slider->extend_state < 0) {
            slider->extend_state = 0;
            if (slider->state == AB_UI_SLIDER_CLOSING) {
                slider->state = AB_UI_SLIDER_CLOSED;
                if (!refresh(slider, error))
                    return false;
            }
        }
        if (!curve(slider, total, total, &slider->extend_adjusted, error))
            return false;
    }
    if (slider->state == AB_UI_SLIDER_DISAPPEAR && slider->extend_state == 0) {
        slider->disappear_scale -= seconds * 3;
        if (slider->disappear_scale <= 0) {
            slider->disappear_scale = 0;
            slider->state = AB_UI_SLIDER_DISAPPEARED;
            slider->box.widget.frame->fields.visible = boolean(false);
        }
        slider->toggle.frame->fields.scale_x = number(slider->disappear_scale);
        slider->toggle.frame->fields.scale_y = number(slider->disappear_scale);
        slider->indicator.frame->fields.scale_x = number(slider->disappear_scale);
        slider->indicator.frame->fields.scale_y = number(slider->disappear_scale);
    }
    if (slider->rotate_indicator) {
        float angle, pi;
        uint32_t pi_bits = UINT32_C(0x40490fdb);
        memcpy(&pi, &pi_bits, sizeof(pi));
        if (!curve(slider, pi * 2, total, &angle, error))
            return false;
        slider->indicator.frame->fields.angle = number(angle);
    }
    return true;
}

static bool arrange(AbUiSlider *slider, float total, AbError *error) {
    float scale_x, scale_y;
    if (!numeric(slider->box.widget.frame->fields.scale_x, &scale_x, error) ||
        !numeric(slider->box.widget.frame->fields.scale_y, &scale_y, error))
        return false;
    for (size_t index = 0; index < slider->count; ++index) {
        AbUiSliderChild *child = slider->items[index];
        float ordinal = (float)index + 1;
        float spacing = slider->child_size + slider->gap;
        float x = slider->direction.x * (ordinal * spacing);
        float y = slider->direction.y * (ordinal * spacing);
        float offset_x, offset_y, override;
        if (!optional(child->offset_x, 0, &offset_x, error) ||
            !optional(child->offset_y, 0, &offset_y, error))
            return false;
        x = ((x + slider->extend_adjusted) - total) + offset_x;
        x *= slider->direction.x;
        x *= scale_x;
        if (!isfinite(x))
            return ab_fail(error, "Slider child X exceeds finite range");
        child->widget->frame->fields.x = number(x);
        y = ((y + slider->extend_adjusted) - total) + offset_y;
        y *= slider->direction.y;
        y *= scale_y;
        if (!isfinite(y))
            return ab_fail(error, "Slider child Y exceeds finite range");
        child->widget->frame->fields.y = number(y);
        if (!optional(child->override_size, 0, &override, error))
            return false;
        x = (x - offset_x) - override;
        y = (y - offset_y) - override;
        bool visible = slider->direction.x < 0   ? x < 0
                       : slider->direction.x > 0 ? x > 0
                       : slider->direction.y < 0 ? y < 0
                                                 : y > 0;
        child->widget->frame->fields.visible = boolean(visible);
    }
    float scale;
    if (!ab_ui_scaling_get(slider->host.scaling, &scale, error))
        return false;
    float width = slider->direction.x != 0
                      ? slider->extend_adjusted * scale
                      : (slider->padding.x * 2 + slider->child_size) * scale;
    float height = slider->direction.x != 0
                       ? (slider->padding.y * 2 + slider->child_size) * scale
                       : slider->extend_adjusted * scale;
    if (!isfinite(width) || !isfinite(height))
        return ab_fail(error, "Slider content bounds exceed finite range");
    if (slider->direction.x != 0)
        ab_ui_bg_box_set_size(&slider->box, width, height);
    else if (slider->direction.y != 0)
        ab_ui_bg_box_set_size(&slider->box, width, height);
    return true;
}

static bool update(void *context, AbUiWidget *widget, float seconds, float time,
                   AbError *error) {
    AbUiSlider *slider = context;
    float total;
    if (widget != &slider->box.widget || !isfinite(seconds) ||
        !ab_ui_slider_total(slider, &total, error) ||
        !animate(slider, seconds, total, error) || !arrange(slider, total, error))
        return false;
    return ab_ui_widget_update_base(widget, seconds, time, error);
}

static bool pointer(void *context, AbUiWidget *widget, AbSpan event, AbVec2 point,
                    AbUiPointerReply *output, AbError *error) {
    AbUiSlider *slider = context;
    AbUiPointerReply result;
    if (widget != &slider->box.widget || !output ||
        !ab_ui_widget_pointer_base(widget, event, point, &result, error))
        return false;
    if (result.values[2].kind == AB_UI_POINTER_OBJECT &&
        result.values[2].object == (uintptr_t)&slider->toggle &&
        ab_ui_pointer_value_truthy(&slider->enabled)) {
        bool visible;
        if (!slider->host.notifications_visible)
            return ab_fail(error, "Slider requires its actual notification owner");
        if (!slider->host.notifications_visible(slider->host.context, &visible,
                                                error) ||
            (!visible && !ab_ui_slider_toggle(slider, error)))
            return false;
    }
    *output = result;
    return true;
}

static bool draw(void *context, AbUiWidget *widget, const AbUiDrawInput *input,
                 AbError *error) {
    AbUiSlider *slider = context;
    AbUiDrawInput parent = input ? *input : (AbUiDrawInput){0};
    parent.present &= 15;
    AbUiDrawTransform transform;
    AbUiDrawFrame frame;
    if (widget != &slider->box.widget || !ab_ui_draw_resolve(&parent, &transform) ||
        !ab_ui_frame_state_draw_view(widget->frame, &frame))
        return ab_fail(error, "Invalid Slider draw geometry");
    bool clipped = slider->state == AB_UI_SLIDER_OPENING ||
                   slider->state == AB_UI_SLIDER_CLOSING ||
                   slider->state == AB_UI_SLIDER_DISAPPEAR;
    float x = transform.position.x + frame.position.x;
    if (clipped && slider->direction.x != 0 &&
        !ab_ui_widget_set_screen_clip(widget->context,
                                      (AbScreenClipRect){x, 0,
                                                         slider->host.screen->x - x,
                                                         slider->host.screen->y},
                                      error))
        return false;
    if (slider->extend_state > 0 &&
        !ab_ui_bg_box_draw_self(&slider->box, &parent, error))
        return false;
    AbUiDrawInput children = {
        .transform = {.position = {x, transform.position.y + frame.position.y},
                      .scale = {transform.scale.x * frame.scale.x,
                                transform.scale.y * frame.scale.y},
                      .angle = frame.angle,
                      .local_factor = 1},
        .present = 63};
    for (size_t index = 0; index < slider->count; ++index) {
        AbUiWidget *child = slider->items[index]->widget;
        AbUiPointerValue visible = child->frame->fields.visible;
        if (visible.kind == AB_UI_POINTER_BOOLEAN && visible.boolean &&
            !ab_ui_widget_draw(child, &children, error))
            return false;
    }
    if (clipped &&
        !ab_ui_widget_set_screen_clip(
            widget->context,
            (AbScreenClipRect){0, 0, slider->host.screen->x, slider->host.screen->y},
            error))
        return false;
    children.present = 15;
    if (!ab_ui_widget_draw(&slider->toggle, &children, error))
        return false;
    AbUiImageButtonState *button = &slider->toggle.state;
    bool hover_enabled = ab_ui_pointer_value_truthy(&button->hover_enabled);
    float hover = 0;
    if (hover_enabled && !numeric(button->hover_scale, &hover, error))
        return false;
    AbUiImageButtonAnimation animation = {.hover_scale = hover,
                                          .hover_enabled = hover_enabled};
    if (!ab_ui_image_button_hover_scale(&animation, &children.transform.local_factor))
        return ab_fail(error, "Invalid Slider toggle hover scale");
    children.transform.angle = 0;
    children.present = 63;
    return ab_ui_widget_draw(&slider->indicator, &children, error);
}

bool ab_ui_slider_init(AbUiSlider *slider, AbUiWidgetContext *context,
                       AbUiWidgetChildren children, AbUiSliderChild **items,
                       size_t capacity, const AbUiFrameFields *arguments,
                       const AbUiPointerValue padding[4], AbUiPointerValue enabled,
                       AbUiSliderHost host, AbError *error) {
    if (!slider || slider->box.widget.initialized || !host.scaling || !host.screen ||
        (capacity && !items) || capacity > SIZE_MAX - 2 ||
        children.capacity < capacity + 2)
        return ab_fail(error, "Invalid Slider owner/storage/host");
    if (!ab_ui_bg_box_init(&slider->box, context, children, arguments, error))
        return false;
    memset(slider->box.components, 0, sizeof(slider->box.components));
    slider->items = items;
    slider->capacity = capacity;
    slider->host = host;
    slider->count = 0;
    slider->padding = (AbVec2){0};
    slider->gap = slider->child_size = slider->extend_state = 0;
    slider->state = AB_UI_SLIDER_CLOSED;
    slider->open_curve = AB_UI_SLIDER_LINEAR;
    slider->direction = (AbVec2){0, -1};
    slider->enabled = ab_ui_pointer_value_truthy(&enabled) ? enabled : boolean(true);
    slider->extend_adjusted = slider->disappear_scale = 1;
    slider->rotate_indicator = true;
    ab_ui_bg_box_use_scale(&slider->box, boolean(true));
    if (!ab_ui_widget_init(&slider->toggle, context, AB_UI_WIDGET_IMAGE_BUTTON,
                           (AbUiWidgetChildren){0}, NULL, NULL, NULL, error))
        return false;
    if (padding)
        memcpy(slider->toggle.padding, padding, sizeof(slider->toggle.padding));
    slider->toggle.state.activate_on_release = boolean(true);
    if (!ab_ui_widget_add(&slider->box.widget, &slider->toggle, error) ||
        !ab_ui_widget_init(&slider->indicator, context, AB_UI_WIDGET_IMAGE,
                           (AbUiWidgetChildren){0}, NULL, NULL, NULL, error))
        return false;
    slider->indicator.frame->fields.name = text("slider");
    if (!ab_ui_widget_add(&slider->box.widget, &slider->indicator, error))
        return false;
    slider->box.widget.methods = (AbUiWidgetMethods){
        .context = slider, .update = update, .draw = draw, .pointer = pointer};
    return true;
}
