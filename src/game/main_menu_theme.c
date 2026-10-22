#include "ab/main_menu_theme.h"

#include <math.h>
#include <string.h>

static bool numeric(AbUiPointerValue value, float *output) {
    if (value.kind != AB_UI_POINTER_NUMBER) {
        return false;
    }
    memcpy(output, &value.number_bits, sizeof(*output));
    return isfinite(*output);
}

static bool default_number(AbUiPointerValue value, float fallback, float *output) {
    if (!ab_ui_pointer_value_valid(&value)) {
        return false;
    }
    if (!ab_ui_pointer_value_truthy(&value)) {
        *output = fallback;
        return true;
    }
    return numeric(value, output);
}

static float menu_top(float height, float zoom, float button_height, float offset) {
    float extent = 450 * zoom;
    extent *= 0.605f;
    float top = (-2 * height) / (height / extent);
    extent = 450 * zoom;
    extent *= 0.5f;
    float button = (button_height * 2.15f) / (height / extent);
    top += button;
    float extra = height > 320 ? (height / 320) * 30 : 0;
    top += offset;
    top += extra;
    return top;
}

bool ab_main_menu_theme_layout(AbMainMenuThemeState *state,
                               const AbMainMenuThemeBoundaries *boundaries,
                               float *camera_top, AbError *error) {
    if (!state || !boundaries || !camera_top || !boundaries->settings ||
        !boundaries->sprite_bounds || !boundaries->theme_field || !boundaries->call ||
        !isfinite(boundaries->screen_height) || boundaries->screen_height <= 0 ||
        !isfinite(boundaries->scroll)) {
        return ab_fail(error, "Invalid main-menu theme layout boundaries");
    }
    void *context = boundaries->context;
    AbUiPointerValue zoom_value, theme, offset_value;
    AbVec2 button;
    AbSpan image = {(const uint8_t *)"BUTTON_EMPTY", 12};
    if (!boundaries->settings(context, AB_MAIN_MENU_SET_ZOOM, NULL, error) ||
        !boundaries->settings(context, AB_MAIN_MENU_GET_ZOOM, &zoom_value, error) ||
        !boundaries->settings(context, AB_MAIN_MENU_GET_THEME, &theme, error) ||
        !boundaries->sprite_bounds(context, image, &button, error)) {
        return false;
    }
    float zoom;
    if (!numeric(zoom_value, &zoom) || zoom <= 0 || theme.kind != AB_UI_POINTER_TEXT ||
        !ab_ui_pointer_value_valid(&theme) || !isfinite(button.y)) {
        return ab_fail(error, "Invalid main-menu zoom/theme/button height");
    }
    if (!boundaries->theme_field(context, theme.text, AB_MAIN_MENU_TOP_OFFSET,
                                 &offset_value, error)) {
        return false;
    }
    float offset;
    if (!default_number(offset_value, 0, &offset)) {
        return ab_fail(error, "Invalid main-menu theme top offset");
    }
    float top = menu_top(boundaries->screen_height, zoom, button.y, offset);
    float scale = (0.5f * boundaries->screen_height) / 400;
    scale /= zoom * 0.66f;
    if (!isfinite(top) || !isfinite(scale)) {
        return ab_fail(error, "Nonfinite main-menu camera layout");
    }
    const AbSpan no_theme = {0};
    if (!boundaries->call(context, AB_MAIN_MENU_CAMERA_ZOOM, no_theme, &scale, 1,
                          error) ||
        !boundaries->call(context, AB_MAIN_MENU_SELECT_THEME, theme.text, NULL, 0,
                          error) ||
        !boundaries->call(context, AB_MAIN_MENU_SELECT_EFFECTS, theme.text, NULL, 0,
                          error)) {
        return false;
    }
    AbUiPointerValue has_water, water = {0};
    if (!boundaries->theme_field(context, theme.text, AB_MAIN_MENU_HAS_WATER,
                                 &has_water, error)) {
        return false;
    }
    if (!ab_ui_pointer_value_valid(&has_water)) {
        return ab_fail(error, "Invalid main-menu water flag");
    }
    if (ab_ui_pointer_value_truthy(&has_water)) {
        AbUiPointerValue level;
        if (!boundaries->theme_field(context, theme.text, AB_MAIN_MENU_WATER_LEVEL,
                                     &level, error)) {
            return false;
        }
        if (!ab_ui_pointer_value_valid(&level)) {
            return ab_fail(error, "Invalid main-menu water height");
        }
        water = ab_ui_pointer_value_truthy(&level)
                    ? level
                    : (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER,
                                         .number_bits = UINT32_C(0x41a00000)};
    }
    state->water_level = water;
    float position[] = {boundaries->scroll, top};
    if (!boundaries->call(context, AB_MAIN_MENU_CAMERA_TOP_LEFT, no_theme, position, 2,
                          error)) {
        return false;
    }
    *camera_top = top;
    return true;
}
