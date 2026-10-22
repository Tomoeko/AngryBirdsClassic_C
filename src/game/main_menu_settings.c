#include "ab/main_menu_settings.h"

bool ab_main_menu_settings_set_zoom(AbMainMenuSettings *settings,
                                    AbUiPointerValue value) {
    if (!settings || !ab_ui_pointer_value_valid(&value)) {
        return false;
    }
    settings->zoom = value;
    return true;
}

bool ab_main_menu_settings_set_theme(AbMainMenuSettings *settings,
                                     AbUiPointerValue value) {
    if (!settings || !ab_ui_pointer_value_valid(&value)) {
        return false;
    }
    settings->theme = value;
    return true;
}

bool ab_main_menu_settings_zoom(const AbMainMenuSettings *settings,
                                AbUiPointerValue *output) {
    if (!settings || !output || !ab_ui_pointer_value_valid(&settings->zoom)) {
        return false;
    }
    *output = ab_ui_pointer_value_truthy(&settings->zoom)
                  ? settings->zoom
                  : (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER,
                                       .number_bits = UINT32_C(0x3fea3d71)};
    return true;
}

bool ab_main_menu_settings_quality(const AbMainMenuSettings *settings,
                                   AbUiPointerValue *output) {
    if (!settings || !output || !ab_ui_pointer_value_valid(&settings->low_quality)) {
        return false;
    }
    *output = ab_ui_pointer_value_truthy(&settings->low_quality)
                  ? settings->low_quality
                  : (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    return true;
}

bool ab_main_menu_settings_theme(const AbMainMenuSettings *settings,
                                 AbUiPointerValue default_theme,
                                 AbMainMenuThemeLookup lookup, void *context,
                                 AbUiPointerValue *output, AbError *error) {
    if (!settings || !output || !ab_ui_pointer_value_valid(&settings->theme)) {
        return ab_fail(error, "Invalid main-menu saved theme");
    }
    if (ab_ui_pointer_value_truthy(&settings->theme)) {
        bool present;
        if (!lookup) {
            return ab_fail(error, "Missing main-menu theme lookup");
        }
        if (!lookup(context, settings->theme, &present, error)) {
            return false;
        }
        if (present) {
            *output = settings->theme;
            return true;
        }
    }
    if (!ab_ui_pointer_value_valid(&default_theme)) {
        return ab_fail(error, "Invalid main-menu default theme");
    }
    *output = ab_ui_pointer_value_truthy(&default_theme)
                  ? default_theme
                  : (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                                       .text = {(const uint8_t *)"theme1", 6}};
    return true;
}
