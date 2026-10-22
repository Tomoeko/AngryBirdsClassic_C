#include "ab/ui_text_bounds.h"

#include <math.h>
#include <string.h>

static bool numeric(AbUiPointerValue value, float *output) {
    if (value.kind != AB_UI_POINTER_NUMBER) {
        return false;
    }
    memcpy(output, &value.number_bits, sizeof(*output));
    return isfinite(*output);
}

static bool named(AbUiPointerValue value, const char *bytes, size_t length) {
    return value.kind == AB_UI_POINTER_TEXT && value.text.size == length &&
           memcmp(value.text.data, bytes, length) == 0;
}

static bool leading(const AbUiTextMetricBoundaries *boundaries, float *output,
                    AbError *error) {
    if (!boundaries || !boundaries->leading) {
        return ab_fail(error, "Missing UI text bounds current-font boundary");
    }
    if (!boundaries->leading(boundaries->resources.context, output, error)) {
        return false;
    }
    return isfinite(*output) || ab_fail(error, "Nonfinite UI text bounds leading");
}

bool ab_ui_text_correction(const AbUiTextState *state,
                           const AbUiTextMetricBoundaries *boundaries, AbVec2 *output,
                           AbError *error) {
    if (!state || !output ||
        !ab_ui_pointer_value_valid(&state->fields.horizontal_anchor) ||
        !ab_ui_pointer_value_valid(&state->fields.vertical_anchor)) {
        return ab_fail(error, "Invalid UI text correction input");
    }
    float current_leading;
    if (!leading(boundaries, &current_leading, error)) {
        return false;
    }
    AbVec2 result = {0};
    bool center = named(state->fields.horizontal_anchor, "HCENTER", 7);
    if (center || named(state->fields.horizontal_anchor, "RIGHT", 5)) {
        float width;
        if (!numeric(state->width, &width)) {
            return ab_fail(error, "Invalid UI text correction width");
        }
        result.x = center ? -width / 2.0f : -width;
    }
    if (named(state->fields.vertical_anchor, "VCENTER", 7)) {
        result.y = -current_leading / 2.0f;
    } else if (named(state->fields.vertical_anchor, "BOTTOM", 6)) {
        result.y = -current_leading;
    }
    *output = result;
    return true;
}

static bool refresh_width(AbUiTextState *state,
                          const AbUiTextMetricBoundaries *boundaries, AbError *error) {
    float previous;
    if (!numeric(state->width, &previous) || !boundaries->resources.localize ||
        !boundaries->resources.measure ||
        !ab_ui_pointer_value_valid(&state->fields.group) ||
        !ab_ui_pointer_value_valid(&state->fields.text)) {
        return ab_fail(error, "Invalid UI bounds text width boundary");
    }
    AbUiPointerValue label, width;
    if (!boundaries->resources.localize(boundaries->resources.context,
                                        state->fields.group, state->fields.text, &label,
                                        error)) {
        return false;
    }
    if (!ab_ui_pointer_value_valid(&label)) {
        return ab_fail(error, "Invalid UI bounds localized label");
    }
    if (!boundaries->resources.measure(boundaries->resources.context, label, &width,
                                       error)) {
        return false;
    }
    float measured;
    if (!numeric(width, &measured)) {
        return ab_fail(error, "Invalid UI bounds measured width");
    }
    if (previous < measured) {
        state->width = width;
    }
    return true;
}

static bool leaf_bounds(const AbUiTextState *state, AbVec2 point,
                        const AbUiTextMetricBoundaries *boundaries, bool *output,
                        AbError *error) {
    float height, x, y, width;
    if (!numeric(state->frame.fields.x, &x) || !numeric(state->frame.fields.y, &y) ||
        !numeric(state->width, &width)) {
        return ab_fail(error, "Invalid UI text bounds leaf geometry");
    }
    if (!leading(boundaries, &height, error)) {
        return false;
    }
    AbVec2 correction;
    if (!ab_ui_text_correction(state, boundaries, &correction, error)) {
        return false;
    }
    float top = y + correction.y;
    float bottom = top + height;
    float left = x + correction.x;
    float right = left + width;
    if (!isfinite(top) || !isfinite(bottom) || !isfinite(left) || !isfinite(right)) {
        return ab_fail(error, "UI text bounds exceed finite range");
    }
    *output =
        top <= point.y && point.y <= bottom && left <= point.x && point.x <= right;
    return true;
}

bool ab_ui_text_check_bounds(AbUiTextState *state, AbUiPointerValue clipped,
                             const AbUiTextBoundsChild *children, size_t count,
                             AbVec2 point, const AbUiTextMetricBoundaries *boundaries,
                             AbUiPointerValue *output, AbError *error) {
    if (!state || !boundaries || !output || !ab_ui_pointer_value_valid(&clipped) ||
        !isfinite(point.x) || !isfinite(point.y) ||
        (ab_ui_pointer_value_truthy(&clipped) &&
         (count > UINT32_C(0x1000000) || (count && !children)))) {
        return ab_fail(error, "Invalid UI text bounds input");
    }
    if (!refresh_width(state, boundaries, error)) {
        return false;
    }
    if (ab_ui_pointer_value_truthy(&clipped)) {
        for (size_t index = 0; index < count; ++index) {
            AbUiPointerValue reply;
            if (!children[index].query) {
                return ab_fail(error, "Missing UI text bounds child query");
            }
            if (!children[index].query(children[index].context, point, &reply, error)) {
                return false;
            }
            if (!ab_ui_pointer_value_valid(&reply)) {
                return ab_fail(error, "Invalid UI text bounds child reply");
            }
            if (ab_ui_pointer_value_truthy(&reply)) {
                *output =
                    (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
                return true;
            }
        }
        *output = (AbUiPointerValue){0};
        return true;
    }
    bool hit = false;
    if (!leaf_bounds(state, point, boundaries, &hit, error)) {
        return false;
    }
    *output = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = hit};
    return true;
}
