#ifndef AB_MAIN_MENU_SETTINGS_H
#define AB_MAIN_MENU_SETTINGS_H

#include "ab/ui_pointer.h"

typedef struct {
    AbUiPointerValue zoom, theme, low_quality;
} AbMainMenuSettings;

typedef bool (*AbMainMenuThemeLookup)(void *context, AbUiPointerValue identifier,
                                      bool *truthy_record, AbError *error);

/* Supported raw settings values remain borrowed. Setters preserve types; an
 * absent zoom models the original no-argument setter call. Getters apply Lua
 * truthiness, preserving a truthy raw value. Zoom defaults to original binary32
 * 0x3fea3d71 and quality to false. No allocation or persistence scheduling. */
bool ab_main_menu_settings_set_zoom(AbMainMenuSettings *settings,
                                    AbUiPointerValue value);
bool ab_main_menu_settings_set_theme(AbMainMenuSettings *settings,
                                     AbUiPointerValue value);
bool ab_main_menu_settings_zoom(const AbMainMenuSettings *settings,
                                AbUiPointerValue *output);
bool ab_main_menu_settings_quality(const AbMainMenuSettings *settings,
                                   AbUiPointerValue *output);

/* Keep a truthy saved theme only when its prepared theme record is truthy.
 * Otherwise return truthy default_theme or literal "theme1" without another
 * existence check. Lookup is invoked only for a truthy saved key. Its identity/
 * key semantics belong to the resource provider; output survives failure. */
bool ab_main_menu_settings_theme(const AbMainMenuSettings *settings,
                                 AbUiPointerValue default_theme,
                                 AbMainMenuThemeLookup lookup, void *context,
                                 AbUiPointerValue *output, AbError *error);

#endif
