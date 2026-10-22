#include "ab/ui_scroll_frame.h"

#include <math.h>
#include <string.h>

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static bool named(AbSpan value, const char *name) {
    return value.size == strlen(name) &&
           (!value.size || (value.data && !memcmp(value.data, name, value.size)));
}

static AbUiPointerValue number(float value) {
    AbUiPointerValue result = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&result.number_bits, &value, sizeof(value));
    return result;
}

static bool numeric(AbUiPointerValue value, float *output, AbError *error) {
    float result;
    memcpy(&result, &value.number_bits, sizeof(result));
    if (value.kind != AB_UI_POINTER_NUMBER || !isfinite(result)) {
        ab_fail(error, "ScrollFrame needs a finite numeric field");
        return false;
    }
    *output = result;
    return true;
}

static bool valid(const AbUiScrollFrame *scroll, AbError *error) {
    return scroll && scroll->widget.initialized && scroll->widget.context &&
                   !scroll->widget.context->failed && scroll->anchors &&
                   scroll->anchor_count <= scroll->anchor_capacity
               ? true
               : ab_fail(error, "Invalid or failed ScrollFrame");
}

static bool finish(AbUiScrollFrame *scroll, bool okay) {
    if (!okay)
        scroll->widget.context->failed = true;
    return okay;
}

static bool position(AbUiScrollFrame *scroll, AbVec2 *output, AbError *error) {
    return numeric(scroll->widget.scroll[0], &output->x, error) &&
           numeric(scroll->widget.scroll[1], &output->y, error);
}

static void set_position(AbUiScrollFrame *scroll, AbVec2 point) {
    scroll->widget.scroll[0] = number(point.x);
    scroll->widget.scroll[1] = number(point.y);
}

static bool anchor_at(AbUiScrollFrame *scroll, size_t index, AbVec2 *output,
                      AbError *error) {
    if (!index || index > scroll->anchor_count ||
        !isfinite(scroll->anchors[index - 1].x) ||
        !isfinite(scroll->anchors[index - 1].y)) {
        ab_fail(error, "Missing or invalid ScrollFrame anchor");
        return false;
    }
    *output = scroll->anchors[index - 1];
    return true;
}

static float distance(AbVec2 first, AbVec2 second) {
    float x = first.x - second.x, y = first.y - second.y;
    float xx = x * x, yy = y * y;
    return sqrtf(xx + yy);
}

bool ab_ui_scroll_frame_reset(AbUiScrollFrame *scroll, AbError *error) {
    if (!valid(scroll, error))
        return false;
    set_position(scroll, (AbVec2){0});
    scroll->dragging = false;
    scroll->current_anchor = scroll->target_anchor = 1;
    scroll->drag_start = scroll->drag_last = scroll->drag_last_update = (AbVec2){0};
    memset(scroll->speed_x, 0, sizeof(scroll->speed_x));
    memset(scroll->speed_y, 0, sizeof(scroll->speed_y));
    scroll->drag_index = 1;
    scroll->drag_timer = 0;
    scroll->velocity = 8;
    return true;
}

bool ab_ui_scroll_frame_set_anchors(AbUiScrollFrame *scroll, const AbVec2 *anchors,
                                    size_t count, AbError *error) {
    if (!valid(scroll, error) || (count && !anchors) || count > scroll->anchor_capacity)
        return ab_fail(error, "ScrollFrame anchor storage is insufficient");
    if (count)
        memmove(scroll->anchors, anchors, count * sizeof(*anchors));
    scroll->anchor_count = count;
    AbVec2 first;
    if (!anchor_at(scroll, 1, &first, error))
        return finish(scroll, false);
    scroll->bounds = (AbAabb){first, first};
    for (size_t index = 0; index < count; ++index) {
        AbVec2 point;
        if (!anchor_at(scroll, index + 1, &point, error))
            return finish(scroll, false);
        if (point.x < scroll->bounds.lower.x)
            scroll->bounds.lower.x = point.x;
        if (point.y < scroll->bounds.lower.y)
            scroll->bounds.lower.y = point.y;
        if (scroll->bounds.upper.x < point.x)
            scroll->bounds.upper.x = point.x;
        if (scroll->bounds.upper.y < point.y)
            scroll->bounds.upper.y = point.y;
    }
    return true;
}

bool ab_ui_scroll_frame_set_current(AbUiScrollFrame *scroll, size_t anchor,
                                    AbError *error) {
    if (!valid(scroll, error))
        return false;
    if (!anchor)
        return true;
    scroll->current_anchor = scroll->target_anchor = anchor;
    AbVec2 point;
    if (!anchor_at(scroll, anchor, &point, error))
        return finish(scroll, false);
    set_position(scroll, point);
    return true;
}

bool ab_ui_scroll_frame_set_target(AbUiScrollFrame *scroll, size_t anchor,
                                   AbError *error) {
    if (!valid(scroll, error))
        return false;
    scroll->target_anchor = anchor;
    return true;
}

bool ab_ui_scroll_frame_next(AbUiScrollFrame *scroll, AbError *error) {
    if (!valid(scroll, error))
        return false;
    scroll->target_anchor = scroll->current_anchor < scroll->anchor_count
                                ? scroll->current_anchor + 1
                                : scroll->anchor_count;
    scroll->velocity = 8;
    return true;
}

bool ab_ui_scroll_frame_previous(AbUiScrollFrame *scroll, AbError *error) {
    if (!valid(scroll, error))
        return false;
    scroll->target_anchor = scroll->current_anchor > 1 ? scroll->current_anchor - 1 : 1;
    scroll->velocity = 8;
    return true;
}

bool ab_ui_scroll_frame_nearest(AbUiScrollFrame *scroll, AbError *error) {
    if (!valid(scroll, error))
        return false;
    AbVec2 point;
    if (!position(scroll, &point, error))
        return finish(scroll, false);
    float closest = INFINITY;
    size_t selected = 1;
    for (size_t index = 0; index < scroll->anchor_count; ++index) {
        AbVec2 anchor;
        if (!anchor_at(scroll, index + 1, &anchor, error))
            return finish(scroll, false);
        float length = distance(point, anchor);
        if (length < closest) {
            closest = length;
            selected = index + 1;
        }
    }
    scroll->target_anchor = selected;
    scroll->velocity = 8;
    return true;
}

bool ab_ui_scroll_frame_flick(AbUiScrollFrame *scroll, float speed, AbUiScrollAxis axis,
                              AbError *error) {
    if (!valid(scroll, error) || !isfinite(speed) || (unsigned)axis > AB_UI_SCROLL_Y)
        return ab_fail(error, "Invalid ScrollFrame flick");
    AbVec2 current;
    if (!position(scroll, &current, error))
        return finish(scroll, false);
    float value = axis == AB_UI_SCROLL_X ? current.x : current.y;
    float low =
        axis == AB_UI_SCROLL_X ? scroll->bounds.lower.x : scroll->bounds.lower.y;
    float high =
        axis == AB_UI_SCROLL_X ? scroll->bounds.upper.x : scroll->bounds.upper.y;
    if ((speed < 0 && value < low) || (speed > 0 && high < value))
        return ab_ui_scroll_frame_nearest(scroll, error);
    float closest = INFINITY;
    size_t selected = 1;
    for (size_t index = 0; index < scroll->anchor_count; ++index) {
        AbVec2 anchor;
        if (!anchor_at(scroll, index + 1, &anchor, error))
            return finish(scroll, false);
        float delta = (axis == AB_UI_SCROLL_X ? anchor.x : anchor.y) - value;
        if (fabsf(delta) < closest &&
            ((speed > 0 && delta > 0) || (speed < 0 && delta < 0))) {
            closest = fabsf(delta);
            selected = index + 1;
        }
    }
    scroll->target_anchor = selected;
    float velocity = fabsf(speed) * 0.2f;
    velocity = velocity * 0.0167f;
    scroll->velocity = velocity > 8 ? velocity : 8;
    return true;
}

bool ab_ui_scroll_frame_begin_drag(AbUiScrollFrame *scroll, AbVec2 point,
                                   AbError *error) {
    if (!valid(scroll, error) || !isfinite(point.x) || !isfinite(point.y))
        return ab_fail(error, "Invalid ScrollFrame drag start");
    scroll->drag_start = scroll->drag_last = scroll->drag_last_update = point;
    memset(scroll->speed_x, 0, sizeof(scroll->speed_x));
    memset(scroll->speed_y, 0, sizeof(scroll->speed_y));
    scroll->dragging = true;
    scroll->drag_has_moved = false;
    return true;
}

static float resistance(float value, float delta, float low, float high, float extent) {
    float factor = 1;
    if (value + delta < low)
        factor = 1 + fabsf(low - (value + delta)) / (extent * 0.2f);
    if (high < value + delta)
        factor = 1 + fabsf(high - (value + delta)) / (extent * 0.2f);
    return factor;
}

bool ab_ui_scroll_frame_update_drag(AbUiScrollFrame *scroll, AbVec2 point,
                                    AbError *error) {
    if (!valid(scroll, error) || !isfinite(point.x) || !isfinite(point.y) ||
        !scroll->host.input_seconds || !isfinite(*scroll->host.input_seconds))
        return ab_fail(error, "Missing current ScrollFrame input delta");
    scroll->drag_timer += *scroll->host.input_seconds;
    if (scroll->drag_timer >= 0x1.111112p-6f) {
        float x = scroll->drag_last.x - point.x;
        float y = scroll->drag_last.y - point.y;
        ++scroll->drag_index;
        if (scroll->drag_index > 5)
            scroll->drag_index = 1;
        scroll->speed_x[scroll->drag_index - 1] = x / scroll->drag_timer;
        scroll->speed_y[scroll->drag_index - 1] = y / scroll->drag_timer;
        scroll->drag_timer = 0;
        scroll->drag_last = point;
    }
    AbVec2 current;
    if (!position(scroll, &current, error))
        return finish(scroll, false);
    float x = scroll->drag_last_update.x - point.x;
    float y = scroll->drag_last_update.y - point.y;
    float rx = resistance(current.x, x, scroll->bounds.lower.x, scroll->bounds.upper.x,
                          (float)scroll->widget.context->render.viewport_width);
    float ry = resistance(current.y, y, scroll->bounds.lower.y, scroll->bounds.upper.y,
                          (float)scroll->widget.context->render.viewport_height);
    if (scroll->axis == AB_UI_SCROLL_X)
        scroll->widget.scroll[0] = number(current.x + x / rx);
    if (scroll->axis == AB_UI_SCROLL_Y)
        scroll->widget.scroll[1] = number(current.y + y / ry);
    scroll->drag_last_update = point;
    return true;
}

bool ab_ui_scroll_frame_release(AbUiScrollFrame *scroll, AbError *error) {
    if (!valid(scroll, error))
        return false;
    scroll->dragging = false;
    float x = 0, y = 0;
    for (size_t index = 0; index < 5; ++index)
        x += scroll->speed_x[index];
    x = x * 0.2f;
    for (size_t index = 0; index < 5; ++index)
        y += scroll->speed_y[index];
    y = y * 0.2f;
    bool okay = true;
    if (scroll->axis == AB_UI_SCROLL_X)
        okay = fabsf(x) < 200
                   ? ab_ui_scroll_frame_nearest(scroll, error)
                   : ab_ui_scroll_frame_flick(scroll, x, AB_UI_SCROLL_X, error);
    else if (scroll->axis == AB_UI_SCROLL_Y)
        okay = fabsf(y) < 200
                   ? ab_ui_scroll_frame_nearest(scroll, error)
                   : ab_ui_scroll_frame_flick(scroll, y, AB_UI_SCROLL_Y, error);
    if (!okay)
        return finish(scroll, false);
    scroll->drag_has_moved = false;
    return true;
}

bool ab_ui_scroll_frame_set_clip(AbUiScrollFrame *scroll, const AbUiScrollClip *clip,
                                 AbError *error) {
    if (!valid(scroll, error))
        return false;
    if (clip && (!isfinite(clip->x) || !isfinite(clip->y) || !isfinite(clip->width) ||
                 !isfinite(clip->height)))
        return ab_fail(error, "Nonfinite ScrollFrame clip");
    if (clip)
        scroll->clip = *clip;
    scroll->clip_present = clip != NULL;
    return true;
}

static bool clear_descendants(AbUiWidget *widget, size_t depth, AbError *error) {
    if (depth >= widget->context->maximum_depth)
        return ab_fail(error, "Excessively deep ScrollFrame flag traversal");
    for (size_t index = 0; index < widget->tree.state->child_count; ++index) {
        AbUiFrameNode *node = widget->children.frames[index];
        AbUiWidget *child = node ? node->context : NULL;
        if (!child || &child->tree != node || child->context != widget->context ||
            !ab_ui_widget_clear_pointer_flags(child, error) ||
            !clear_descendants(child, depth + 1, error))
            return false;
    }
    return true;
}

static bool update(void *context, AbUiWidget *widget, float seconds, float time,
                   AbError *error) {
    AbUiScrollFrame *scroll = context;
    if (widget != &scroll->widget ||
        !ab_ui_widget_update_base(widget, seconds, time, error))
        return false;
    AbVec2 target, current, point;
    if (!anchor_at(scroll, scroll->target_anchor, &target, error) ||
        !anchor_at(scroll, scroll->current_anchor, &current, error) ||
        !position(scroll, &point, error))
        return false;
    if (!scroll->dragging && (point.x != target.x || point.y != target.y)) {
        float dx = target.x - point.x, dy = target.y - point.y;
        float x = dx * seconds;
        x = x * scroll->velocity;
        float y = dy * seconds;
        y = y * scroll->velocity;
        x = dx > 0 ? (x < dx ? x : dx) : (dx < x ? x : dx);
        y = dy > 0 ? (y < dy ? y : dy) : (dy < y ? y : dy);
        scroll->widget.scroll[0] = number(point.x + x);
        scroll->widget.scroll[1] = number(point.y + y);
    }
    if (!scroll->dragging && scroll->target_anchor != scroll->current_anchor) {
        if (!position(scroll, &point, error))
            return false;
        if (distance(target, point) < distance(current, point)) {
            scroll->current_anchor = scroll->target_anchor;
            if (scroll->anchor_changed &&
                !scroll->anchor_changed(scroll->derived_context, scroll,
                                        scroll->current_anchor, error))
                return false;
        }
    }
    if (scroll->drag_has_moved && !clear_descendants(widget, 0, error))
        return false;
    return true;
}

static bool pointer(void *context, AbUiWidget *widget, AbSpan event, AbVec2 point,
                    AbUiPointerReply *output, AbError *error) {
    AbUiScrollFrame *scroll = context;
    if (!output || widget != &scroll->widget)
        return ab_fail(error, "Invalid ScrollFrame pointer destination");
    bool inside = true;
    if (scroll->clip_present) {
        AbVec2 cursor = widget->context->cursor;
        inside = !(cursor.x < scroll->clip.x || cursor.y < scroll->clip.y ||
                   scroll->clip.x + scroll->clip.width <= cursor.x ||
                   scroll->clip.y + scroll->clip.height <= cursor.y);
    }
    bool blocked = false;
    if (scroll->blocks_input && scroll->dragging) {
        float delta = scroll->axis == AB_UI_SCROLL_X
                          ? scroll->drag_start.x - scroll->drag_last.x
                          : scroll->drag_start.y - scroll->drag_last.y;
        if (scroll->axis == AB_UI_SCROLL_XY)
            return ab_fail(error, "Source ScrollFrame blocksInput has no xy distance");
        blocked = scroll->block_distance < fabsf(delta);
    }
    AbVec2 current;
    AbUiPointerReply discarded;
    if (!blocked && inside) {
        if (!position(scroll, &current, error) ||
            !ab_ui_widget_pointer_base(
                widget, event, (AbVec2){point.x + current.x, point.y + current.y},
                &discarded, error))
            return false;
    }
    float threshold = (float)widget->context->render.viewport_width * 0.03125f;
    if (scroll->dragging && threshold < distance(point, scroll->drag_start))
        scroll->drag_has_moved = true;
    AbUiPointerReply reply = {0};
    if (inside && !scroll->drag_has_moved) {
        if (!position(scroll, &current, error) ||
            !ab_ui_widget_pointer_base(
                widget, event, (AbVec2){point.x + current.x, point.y + current.y},
                &reply, error))
            return false;
    }
    if (named(event, "LPRESS") && !scroll->dragging && inside) {
        if (!ab_ui_scroll_frame_begin_drag(scroll, point, error))
            return false;
    } else if (named(event, "LHOLD") && scroll->dragging) {
        if (!ab_ui_scroll_frame_update_drag(scroll, point, error))
            return false;
    } else if (named(event, "LRELEASE") && scroll->dragging) {
        if (!ab_ui_scroll_frame_release(scroll, error))
            return false;
    } else if (named(event, "WHEEL")) {
        if (!scroll->host.wheel || !isfinite(*scroll->host.wheel))
            return ab_fail(error, "Missing ScrollFrame cursor wheel");
        if (*scroll->host.wheel < 0) {
            if (!ab_ui_scroll_frame_next(scroll, error))
                return false;
        } else if (*scroll->host.wheel > 0 &&
                   !ab_ui_scroll_frame_previous(scroll, error))
            return false;
    }
    *output = reply;
    return true;
}

static AbUiWidget *child_at(AbUiScrollFrame *scroll, size_t index, AbError *error) {
    AbUiFrameNode *node = scroll->widget.children.frames[index];
    AbUiWidget *child = node ? node->context : NULL;
    if (!child || &child->tree != node || child->context != scroll->widget.context ||
        !child->initialized) {
        ab_fail(error, "Foreign ScrollFrame child");
        return NULL;
    }
    return child;
}

static bool draw_children(AbUiScrollFrame *scroll, AbVec2 point, AbSpan method,
                          AbError *error) {
    AbUiWidget *widget = &scroll->widget;
    AbVec2 origin;
    if (!ab_ui_widget_screen_position(widget, &origin, error))
        return false;
    for (size_t index = 0; index < widget->tree.state->child_count; ++index) {
        AbUiWidget *child = child_at(scroll, index, error);
        AbVec2 current, scale;
        AbVec2 child_position, child_scale;
        float width, height;
        if (!child || !numeric(child->tree.state->fields.x, &child_position.x, error) ||
            !numeric(child->tree.state->fields.y, &child_position.y, error) ||
            !numeric(child->tree.state->fields.scale_x, &child_scale.x, error) ||
            !numeric(child->tree.state->fields.scale_y, &child_scale.y, error) ||
            !numeric(child->tree.state->fields.w, &width, error) ||
            !numeric(child->tree.state->fields.h, &height, error) ||
            !position(scroll, &current, error) ||
            !ab_ui_widget_cumulative_scale(widget, &scale, error))
            return false;
        float x = child_position.x - current.x;
        x += origin.x;
        float y = child_position.y - current.y;
        y += origin.y;
        float half_x = width / 2;
        half_x = half_x * scale.x;
        float half_y = height / 2;
        half_y = half_y * scale.y;
        float extent_x = half_x * child_scale.x;
        float extent_y = half_y * child_scale.y;
        bool visible = 0 <= x + extent_x &&
                       x - extent_x < (float)widget->context->render.viewport_width &&
                       0 <= y + extent_y &&
                       y - extent_y < (float)widget->context->render.viewport_height;
        if (scroll->ignore_culling)
            visible = true;
        else if (scroll->clip_present &&
                 (x + half_x < scroll->clip.x ||
                  scroll->clip.x + scroll->clip.width <= x - half_x ||
                  y + half_y < scroll->clip.y ||
                  scroll->clip.y + scroll->clip.height <= y - half_y))
            visible = false;
        if (!visible)
            continue;
        AbUiDrawInput input = {.transform = {.position = point, .scale = scale},
                               .present = 15};
        if (!ab_ui_widget_draw_named(child, method, &input, error))
            return false;
    }
    return true;
}

static bool draw(void *context, AbUiWidget *widget, const AbUiDrawInput *input,
                 AbError *error) {
    AbUiScrollFrame *scroll = context;
    if (widget != &scroll->widget || !input || (input->present & ~63u))
        return ab_fail(error, "Invalid ScrollFrame draw input");
    bool clipped = scroll->clip_present;
    if (clipped) {
        if (!scroll->host.set_clip)
            return ab_fail(error, "Missing ScrollFrame screen clip setter");
        if (!scroll->host.set_clip(scroll->host.context, scroll->clip, error))
            return false;
    }
    AbVec2 current, frame;
    if (!position(scroll, &current, error) ||
        !numeric(widget->tree.state->fields.x, &frame.x, error) ||
        !numeric(widget->tree.state->fields.y, &frame.y, error))
        return false;
    float x = input->present & 1 ? input->transform.position.x : 0;
    float y = input->present & 2 ? input->transform.position.y : 0;
    x = x - current.x;
    x = floorf(x + frame.x);
    y = y - current.y;
    y = floorf(y + frame.y);
    if (!isfinite(x) || !isfinite(y))
        return ab_fail(error, "Nonfinite ScrollFrame draw position");
    static const char *const methods[] = {"drawBackground", "drawMidground",
                                          "drawForeground"};
    for (size_t phase = 0; phase < (scroll->draw_in_three_phases ? 3u : 1u); ++phase)
        if (!draw_children(scroll, (AbVec2){x, y},
                           span(scroll->draw_in_three_phases ? methods[phase] : "draw"),
                           error))
            return false;
    return !clipped ||
           scroll->host.set_clip(
               scroll->host.context,
               (AbUiScrollClip){0, 0, (float)widget->context->render.viewport_width,
                                (float)widget->context->render.viewport_height},
               error);
}

static bool pause_event(void *context, AbDataTable *event, AbError *error) {
    AbUiScrollFrame *scroll = context;
    if (!valid(scroll, error))
        return false;
    const AbDataValue *id =
        ab_data_get(event, (AbDataValue){.kind = AB_DATA_STRING, .string = span("id")});
    AbDataValue expected = scroll->host.pause_event;
    bool same = id && id->kind == expected.kind;
    if (same && id->kind == AB_DATA_STRING)
        same = id->string.size == expected.string.size &&
               (!id->string.size ||
                !memcmp(id->string.data, expected.string.data, id->string.size));
    else if (same && id->kind == AB_DATA_NUMBER) {
        float first, second;
        memcpy(&first, &id->bits, sizeof(first));
        memcpy(&second, &expected.bits, sizeof(second));
        same = first == second;
    } else if (same)
        same = id->bits == expected.bits;
    return !same || !scroll->dragging || ab_ui_scroll_frame_release(scroll, error);
}

static bool lifecycle(void *context, AbUiWidget *widget, AbUiFrameMethod method,
                      AbError *error) {
    AbUiScrollFrame *scroll = context;
    if (widget != &scroll->widget ||
        !ab_ui_widget_lifecycle_base(widget, method, error))
        return false;
    if (method == AB_UI_FRAME_ENTRY) {
        if (!scroll->host.events ||
            !ab_event_manager_add(scroll->host.events, scroll->host.pause_event,
                                  &scroll->listener, error))
            return false;
        scroll->subscribed = true;
        return ab_ui_scroll_frame_reset(scroll, error);
    }
    if (method == AB_UI_FRAME_EXIT) {
        if (!scroll->host.events ||
            !ab_event_manager_remove(scroll->host.events, scroll->host.pause_event,
                                     &scroll->listener, error))
            return false;
        scroll->subscribed = false;
    }
    return true;
}

bool ab_ui_scroll_frame_detach(AbUiScrollFrame *scroll, AbError *error) {
    if (!scroll || !scroll->subscribed)
        return true;
    if (!ab_event_manager_remove(scroll->host.events, scroll->host.pause_event,
                                 &scroll->listener, error))
        return false;
    scroll->subscribed = false;
    return true;
}

bool ab_ui_scroll_frame_init(AbUiScrollFrame *scroll, AbUiWidgetContext *context,
                             AbUiWidgetChildren children, AbVec2 *anchors,
                             size_t capacity, const AbUiScrollArguments *arguments,
                             AbUiScrollHost host, AbError *error) {
    if (!scroll || scroll->widget.initialized || !context || !anchors || !capacity ||
        capacity > UINT32_C(16777216) ||
        (arguments && (unsigned)arguments->axis > AB_UI_SCROLL_XY))
        return ab_fail(error, "Invalid ScrollFrame owner/anchor storage");
    if (!ab_ui_widget_init(&scroll->widget, context, AB_UI_WIDGET_FRAME, children,
                           arguments ? &arguments->frame : NULL, NULL, NULL, error))
        return false;
    scroll->host = host;
    if (!scroll->host.set_clip) {
        scroll->host.context = context;
        scroll->host.set_clip = ab_ui_widget_set_screen_clip;
    }
    scroll->anchors = anchors;
    scroll->anchor_capacity = capacity;
    scroll->anchor_count = 1;
    anchors[0] = (AbVec2){0};
    scroll->axis = arguments ? arguments->axis : AB_UI_SCROLL_X;
    scroll->bounds = (AbAabb){0};
    scroll->clip_present = false;
    scroll->blocks_input = arguments && arguments->blocks_input;
    scroll->block_distance = 80;
    if (arguments && ab_ui_pointer_value_truthy(&arguments->block_distance) &&
        !numeric(arguments->block_distance, &scroll->block_distance, error))
        return finish(scroll, false);
    scroll->ignore_culling = arguments && arguments->ignore_culling;
    if (!ab_ui_scroll_frame_reset(scroll, error))
        return false;
    scroll->listener =
        (AbEventListener){.context = scroll, .event_triggered = pause_event};
    scroll->widget.methods = (AbUiWidgetMethods){.context = scroll,
                                                 .lifecycle = lifecycle,
                                                 .draw = draw,
                                                 .pointer = pointer,
                                                 .update = update};
    return true;
}
