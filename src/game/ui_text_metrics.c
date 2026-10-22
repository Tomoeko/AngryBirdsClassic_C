#include "ab/ui_text_metrics.h"

#include "ab/ui_value_text.h"

#include <math.h>
#include <string.h>

static bool numeric(AbUiPointerValue value, float *output) {
    if (value.kind != AB_UI_POINTER_NUMBER) {
        return false;
    }
    memcpy(output, &value.number_bits, sizeof(*output));
    return isfinite(*output);
}

static bool scale(AbUiPointerValue value, float *output) {
    if (!ab_ui_pointer_value_valid(&value)) {
        return false;
    }
    if (!ab_ui_pointer_value_truthy(&value)) {
        *output = 1;
        return true;
    }
    return numeric(value, output);
}

static bool select_font(const AbUiTextState *state,
                        const AbUiTextMetricBoundaries *boundaries, AbError *error) {
    if (!ab_ui_pointer_value_valid(&state->fields.font) ||
        !boundaries->resources.font_setting_available ||
        !boundaries->resources.set_font) {
        return ab_fail(error, "Missing UI text metric font boundary");
    }
    return boundaries->resources.set_font(boundaries->resources.context,
                                          state->fields.font, error);
}

bool ab_ui_text_get_font_leading(const AbUiTextState *state,
                                 const AbUiTextMetricBoundaries *boundaries,
                                 float *output, AbError *error) {
    float factor;
    if (!state || !boundaries || !output || !numeric(state->leading_scale, &factor)) {
        return ab_fail(error, "Invalid UI text leading input");
    }
    if (!select_font(state, boundaries, error)) {
        return false;
    }
    if (!boundaries->leading) {
        return ab_fail(error, "Missing UI current font leading boundary");
    }
    float leading;
    if (!boundaries->leading(boundaries->resources.context, &leading, error)) {
        return false;
    }
    float result = leading * factor;
    if (!isfinite(leading) || !isfinite(result)) {
        return ab_fail(error, "UI text leading exceeds finite range");
    }
    *output = result;
    return true;
}

bool ab_ui_text_get_height(const AbUiTextState *state, AbUiPointerValue clipped,
                           size_t line_count,
                           const AbUiTextMetricBoundaries *boundaries, float *output,
                           AbError *error) {
    float scale_y;
    if (!state || !output || !ab_ui_pointer_value_valid(&clipped) ||
        !scale(state->frame.fields.scale_y, &scale_y) ||
        (ab_ui_pointer_value_truthy(&clipped) && line_count > UINT32_C(0x1000000))) {
        return ab_fail(error, "Invalid UI text height input");
    }
    float leading;
    if (!ab_ui_text_get_font_leading(state, boundaries, &leading, error)) {
        return false;
    }
    float height =
        ab_ui_pointer_value_truthy(&clipped) ? (float)line_count * leading : leading;
    height = height * scale_y;
    if (!isfinite(height)) {
        return ab_fail(error, "UI text height exceeds finite range");
    }
    *output = height;
    return true;
}

bool ab_ui_text_get_width(const AbUiTextState *state,
                          const AbUiTextMetricBoundaries *boundaries, float *output,
                          AbError *error) {
    float scale_x;
    if (!state || !boundaries || !output ||
        !scale(state->frame.fields.scale_x, &scale_x) ||
        !ab_ui_pointer_value_valid(&state->fields.text)) {
        return ab_fail(error, "Invalid UI text width input");
    }
    if (!select_font(state, boundaries, error)) {
        return false;
    }
    if (state->fields.text.kind == AB_UI_POINTER_TEXT &&
        !state->fields.text.text.size) {
        *output = 0;
        return true;
    }
    AbSpan label;
    uint8_t numeric_bytes[32];
    if (boundaries->stringify) {
        if (!boundaries->stringify(boundaries->resources.context, state->fields.text,
                                   &label, error)) {
            return false;
        }
    } else if (!ab_ui_value_text(state->fields.text, numeric_bytes,
                                 sizeof(numeric_bytes), &label)) {
        return ab_fail(error, "Unsupported UI text tostring value");
    }
    if ((label.size && !label.data) || !boundaries->resources.localize ||
        !boundaries->resources.measure ||
        !ab_ui_pointer_value_valid(&state->fields.group)) {
        return ab_fail(error, "Missing or invalid UI text width resource boundary");
    }
    AbUiPointerValue localized, width;
    if (!boundaries->resources.localize(
            boundaries->resources.context, state->fields.group,
            (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = label}, &localized,
            error)) {
        return false;
    }
    if (!ab_ui_pointer_value_valid(&localized)) {
        return ab_fail(error, "Invalid UI width localized value");
    }
    if (!boundaries->resources.measure(boundaries->resources.context, localized, &width,
                                       error)) {
        return false;
    }
    float measured;
    if (!numeric(width, &measured)) {
        return ab_fail(error, "Invalid UI measured width");
    }
    float result = measured * scale_x;
    if (!isfinite(result)) {
        return ab_fail(error, "UI text width exceeds finite range");
    }
    *output = result;
    return true;
}
