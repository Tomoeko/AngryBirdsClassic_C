#include "ab/ui_init.h"

static AbUiPointerValue number(uint32_t bits) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER, .number_bits = bits};
}

static AbUiPointerValue text(const char *bytes, size_t size) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                              .text = {(const uint8_t *)bytes, size}};
}

static AbUiPointerValue or_value(AbUiPointerValue value, AbUiPointerValue fallback) {
    return ab_ui_pointer_value_truthy(&value) ? value : fallback;
}

static bool valid_frame(const AbUiFrameFields *fields) {
    return ab_ui_pointer_value_valid(&fields->name) &&
           ab_ui_pointer_value_valid(&fields->x) &&
           ab_ui_pointer_value_valid(&fields->y) &&
           ab_ui_pointer_value_valid(&fields->scale_x) &&
           ab_ui_pointer_value_valid(&fields->scale_y) &&
           ab_ui_pointer_value_valid(&fields->angle) &&
           ab_ui_pointer_value_valid(&fields->w) &&
           ab_ui_pointer_value_valid(&fields->h) &&
           ab_ui_pointer_value_valid(&fields->visible) &&
           ab_ui_pointer_value_valid(&fields->active);
}

static bool valid_text(const AbUiTextArguments *fields) {
    return ab_ui_pointer_value_valid(&fields->text) &&
           ab_ui_pointer_value_valid(&fields->group) &&
           ab_ui_pointer_value_valid(&fields->box_size) &&
           ab_ui_pointer_value_valid(&fields->horizontal_anchor) &&
           ab_ui_pointer_value_valid(&fields->vertical_anchor) &&
           ab_ui_pointer_value_valid(&fields->font);
}

static bool call_font(AbUiTextState *state, const AbUiInitBoundaries *boundaries,
                      AbError *error) {
    if (state->font_getter.kind != AB_UI_POINTER_FUNCTION || !boundaries->call_font) {
        return ab_fail(error, "Missing or non-function UI font getter boundary");
    }
    AbUiPointerValue font;
    if (!boundaries->call_font(boundaries->context, state->font_getter.function, &font,
                               error)) {
        return false;
    }
    if (!ab_ui_pointer_value_valid(&font)) {
        return ab_fail(error, "Invalid UI font getter result");
    }
    state->fields.font = font;
    return true;
}

bool ab_ui_text_set_font(AbUiTextState *state, AbUiPointerValue font,
                         const AbUiInitBoundaries *boundaries, AbError *error) {
    if (!state || !boundaries || !ab_ui_pointer_value_valid(&font)) {
        return ab_fail(error, "Invalid UI font setter input");
    }
    if (font.kind != AB_UI_POINTER_FUNCTION) {
        state->fields.font = font;
        return true;
    }
    state->font_getter = font;
    return call_font(state, boundaries, error);
}

bool ab_ui_text_layout(AbUiTextState *state, const AbUiInitBoundaries *boundaries,
                       AbError *error) {
    if (!state || !boundaries || !ab_ui_pointer_value_valid(&state->font_getter)) {
        return ab_fail(error, "Invalid UI text layout input");
    }
    return state->font_getter.kind == AB_UI_POINTER_NONE ||
           call_font(state, boundaries, error);
}

bool ab_ui_frame_init(AbUiFrameState *state, const AbUiFrameFields *arguments,
                      const AbUiInitBoundaries *boundaries, AbError *error) {
    const AbUiFrameFields empty = {0};
    const AbUiFrameFields *input = arguments ? arguments : &empty;
    if (!state || !boundaries || !valid_frame(input)) {
        return ab_fail(error, "Invalid UI frame initialization input");
    }
    state->child_count = state->lookup_count = 0;
    if (ab_ui_pointer_value_truthy(&input->name)) {
        state->fields.name = input->name;
    } else {
        AbUiPointerValue name;
        if (!boundaries->default_name) {
            return ab_fail(error, "Missing UI default name boundary");
        }
        if (!boundaries->default_name(boundaries->context, &name, error)) {
            return false;
        }
        if (!ab_ui_pointer_value_valid(&name)) {
            return ab_fail(error, "Invalid UI default name value");
        }
        state->fields.name = name;
    }
    state->fields.x = or_value(input->x, number(0));
    state->fields.y = or_value(input->y, number(0));
    state->fields.scale_x = or_value(input->scale_x, number(UINT32_C(0x3f800000)));
    state->fields.scale_y = or_value(input->scale_y, number(UINT32_C(0x3f800000)));
    state->fields.angle = or_value(input->angle, number(0));
    state->fields.w = or_value(input->w, number(0));
    state->fields.h = or_value(input->h, number(0));
    const AbUiPointerValue yes = {.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    state->fields.visible =
        input->visible.kind == AB_UI_POINTER_NONE ? yes : input->visible;
    state->fields.active =
        input->active.kind == AB_UI_POINTER_NONE ? yes : input->active;
    return true;
}

bool ab_ui_text_init(AbUiTextState *state, const AbUiFrameFields *frame_arguments,
                     const AbUiTextArguments *text_arguments,
                     const AbUiInitBoundaries *boundaries, AbError *error) {
    const AbUiTextArguments empty = {0};
    const AbUiTextArguments *input = text_arguments ? text_arguments : &empty;
    if (!state || !boundaries || !valid_text(input) ||
        !ab_ui_pointer_value_valid(&state->fields.font) ||
        !ab_ui_pointer_value_valid(&boundaries->screen_width) ||
        !ab_ui_pointer_value_valid(&boundaries->basic_font)) {
        return ab_fail(error, "Invalid UI text initialization input");
    }
    if (!ab_ui_frame_init(&state->frame, frame_arguments, boundaries, error)) {
        return false;
    }
    state->fields.text = or_value(input->text, text("", 0));
    state->fields.group = or_value(input->group, text("TEXTS_BASIC", 11));
    state->fields.box_size = or_value(input->box_size, boundaries->screen_width);
    state->fields.horizontal_anchor =
        or_value(input->horizontal_anchor, text("HCENTER", 7));
    state->fields.vertical_anchor =
        or_value(input->vertical_anchor, text("VCENTER", 7));
    if (input->font.kind == AB_UI_POINTER_FUNCTION &&
        !ab_ui_text_set_font(state, input->font, boundaries, error)) {
        return false;
    }
    state->fields.font =
        or_value(state->fields.font, or_value(input->font, boundaries->basic_font));
    state->frame.fields.scale_x = state->frame.fields.scale_y =
        number(UINT32_C(0x3f800000));
    state->frame.fields.visible = state->floor_coordinates =
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    state->leading_scale = number(UINT32_C(0x3f800000));
    if (!boundaries->font_setting_available) {
        state->width = number(0);
        return true;
    }
    if (!boundaries->set_font) {
        return ab_fail(error, "Missing UI initial font boundary");
    }
    if (!boundaries->set_font(boundaries->context, state->fields.font, error)) {
        return false;
    }
    AbUiPointerValue label = state->fields.text;
    if (state->fields.group.kind != AB_UI_POINTER_TEXT ||
        state->fields.group.text.size) {
        if (!boundaries->localize) {
            return ab_fail(error, "Missing UI initial text localization");
        }
        if (!boundaries->localize(boundaries->context, state->fields.group,
                                  state->fields.text, &label, error)) {
            return false;
        }
        if (!ab_ui_pointer_value_valid(&label)) {
            return ab_fail(error, "Invalid UI initial localized text value");
        }
    }
    AbUiPointerValue width;
    if (!boundaries->measure) {
        return ab_fail(error, "Missing UI initial text measurement");
    }
    if (!boundaries->measure(boundaries->context, label, &width, error)) {
        return false;
    }
    if (!ab_ui_pointer_value_valid(&width)) {
        return ab_fail(error, "Invalid UI initial text width value");
    }
    state->width = width;
    return true;
}
