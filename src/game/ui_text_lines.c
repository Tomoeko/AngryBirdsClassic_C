#include "ab/ui_text_lines.h"

#include <math.h>
#include <string.h>

static bool numeric(AbUiPointerValue value, float *output) {
    if (value.kind != AB_UI_POINTER_NUMBER) {
        return false;
    }
    memcpy(output, &value.number_bits, sizeof(*output));
    return isfinite(*output);
}

static AbUiPointerValue number(float value) {
    AbUiPointerValue output = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&output.number_bits, &value, sizeof(value));
    return output;
}

static bool named(AbUiPointerValue value, const char *bytes, size_t count) {
    return value.kind == AB_UI_POINTER_TEXT && value.text.size == count &&
           memcmp(value.text.data, bytes, count) == 0;
}

static bool selected_scale(const AbUiTextLines *state, AbUiPointerValue own_scale,
                           const AbUiTextLineBoundaries *boundaries, AbVec2 *output,
                           AbError *error) {
    if (!boundaries->cumulative_scale) {
        return ab_fail(error, "Missing UI text cumulative scale boundary");
    }
    if (!boundaries->cumulative_scale(boundaries->init.context, output, error)) {
        return false;
    }
    if (ab_ui_pointer_value_truthy(&own_scale) &&
        (!numeric(state->text.frame.fields.scale_x, &output->x) ||
         !numeric(state->text.frame.fields.scale_y, &output->y))) {
        return ab_fail(error, "Invalid UI text own scales");
    }
    if (!isfinite(output->x) || !isfinite(output->y) || output->x == 0) {
        return ab_fail(error, "Invalid UI text wrapping scales");
    }
    return true;
}

static bool create_lines(AbUiTextLines *state, const AbUiWrappedText *wrapped,
                         float line_height, const AbUiInitBoundaries *boundaries,
                         AbError *error) {
    float y = 0;
    if (named(state->text.fields.vertical_anchor, "VCENTER", 7)) {
        y = -state->text_block_height / 2.0f + line_height / 2.0f;
    } else if (named(state->text.fields.vertical_anchor, "BOTTOM", 6)) {
        y = -state->text_block_height + line_height;
    }
    for (size_t index = 0; index < wrapped->count; ++index) {
        AbUiFrameFields frame = {.y = number(y),
                                 .scale_x = state->text.frame.fields.scale_x,
                                 .scale_y = state->text.frame.fields.scale_y};
        AbUiTextArguments arguments = {
            .font = state->text.fields.font,
            .text = {.kind = AB_UI_POINTER_TEXT, .text = wrapped->lines[index]},
            .horizontal_anchor = state->text.fields.horizontal_anchor,
            .vertical_anchor = state->text.fields.vertical_anchor};
        AbUiTextState line = {0};
        if (!ab_ui_text_init(&line, &frame, &arguments, boundaries, error)) {
            return false;
        }
        state->lines[state->count++] = line;
        y = y + line_height;
        if (!isfinite(y)) {
            return ab_fail(error, "UI text line position exceeds finite range");
        }
    }
    return true;
}

bool ab_ui_text_lines_build(AbUiTextLines *state, AbUiPointerValue own_scale,
                            AbUiPointerValue spacing,
                            const AbUiTextLineBoundaries *boundaries, AbError *error) {
    float factor = 1, box_size, leading_scale;
    if (!state || !boundaries || !boundaries->init.font_setting_available ||
        !ab_ui_pointer_value_valid(&own_scale) ||
        !ab_ui_pointer_value_valid(&spacing) ||
        (ab_ui_pointer_value_truthy(&spacing) && !numeric(spacing, &factor)) ||
        !numeric(state->text.fields.box_size, &box_size) ||
        !numeric(state->text.leading_scale, &leading_scale) ||
        !ab_ui_pointer_value_valid(&state->text.fields.vertical_anchor) ||
        !ab_ui_pointer_value_valid(&state->text.fields.horizontal_anchor) ||
        (state->capacity && !state->lines)) {
        return ab_fail(error, "Invalid UI text line construction input");
    }
    AbVec2 scale;
    if (!selected_scale(state, own_scale, boundaries, &scale, error)) {
        return false;
    }
    if (!boundaries->init.set_font || !boundaries->wrap || !boundaries->font_leading) {
        return ab_fail(error, "Missing UI text line resource boundary");
    }
    if (!boundaries->init.set_font(boundaries->init.context, state->text.fields.font,
                                   error)) {
        return false;
    }
    float width = box_size / scale.x;
    if (!isfinite(width)) {
        return ab_fail(error, "UI text wrapping width exceeds finite range");
    }
    AbUiWrappedText wrapped;
    if (!boundaries->wrap(boundaries->init.context, state->text.fields.group,
                          state->text.fields.text, width, &wrapped, error)) {
        return false;
    }
    float leading;
    if (!boundaries->font_leading(boundaries->init.context, &leading, error)) {
        return false;
    }
    float line_height = ((leading * scale.y) * leading_scale) * factor;
    float height = (float)wrapped.count * line_height;
    if (!isfinite(line_height) || !isfinite(height) || !isfinite(wrapped.widest_line) ||
        wrapped.count > UINT32_C(0x1000000) || wrapped.count > state->capacity ||
        (wrapped.count && !wrapped.lines)) {
        return ab_fail(error, "Invalid wrapped UI text result or line capacity");
    }
    for (size_t index = 0; index < wrapped.count; ++index) {
        if (wrapped.lines[index].size && !wrapped.lines[index].data) {
            return ab_fail(error, "Invalid wrapped UI text line storage");
        }
    }
    state->text_block_height = height;
    state->widest_line = wrapped.widest_line;
    state->count = 0;
    if (!create_lines(state, &wrapped, line_height, &boundaries->init, error)) {
        return false;
    }
    state->clipped = true;
    return true;
}
