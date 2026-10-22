#ifndef AB_MAIN_MENU_THEME_H
#define AB_MAIN_MENU_THEME_H

#include "ab/ui_pointer.h"

typedef enum {
    AB_MAIN_MENU_SET_ZOOM,
    AB_MAIN_MENU_GET_ZOOM,
    AB_MAIN_MENU_GET_THEME,
} AbMainMenuSetting;

typedef enum {
    AB_MAIN_MENU_TOP_OFFSET,
    AB_MAIN_MENU_HAS_WATER,
    AB_MAIN_MENU_WATER_LEVEL,
} AbMainMenuThemeField;

typedef enum {
    AB_MAIN_MENU_CAMERA_ZOOM,
    AB_MAIN_MENU_SELECT_THEME,
    AB_MAIN_MENU_SELECT_EFFECTS,
    AB_MAIN_MENU_CAMERA_TOP_LEFT,
} AbMainMenuThemeCall;

typedef struct {
    AbUiPointerValue water_level;
} AbMainMenuThemeState;

typedef struct {
    void *context;
    float screen_height, scroll;
    /* SET has no return; output is NULL for that operation. GET returns the
     * supported raw numeric zoom or text theme identifier. */
    bool (*settings)(void *context, AbMainMenuSetting operation,
                     AbUiPointerValue *output, AbError *error);
    bool (*sprite_bounds)(void *context, AbSpan image, AbVec2 *output, AbError *error);
    bool (*theme_field)(void *context, AbSpan theme, AbMainMenuThemeField field,
                        AbUiPointerValue *output, AbError *error);
    bool (*call)(void *context, AbMainMenuThemeCall operation, AbSpan theme,
                 const float *numbers, size_t count, AbError *error);
} AbMainMenuThemeBoundaries;

/* Original main-menu theme layout with explicit settings/resource/theme and
 * camera/effect boundaries. Keep binary32 operation order, the >320 extra top
 * offset and absent/false water defaults. Water state changes after theme/
 * effect selection and before the final camera call. Output changes only on
 * success; earlier callbacks/water writes survive failure. Positive finite
 * screen height/zoom, finite used sprite/offset values and stable borrowed text
 * are required. Truthy water levels retain their complete supported raw values.
 * No allocation, original script execution or GPU calls. */
bool ab_main_menu_theme_layout(AbMainMenuThemeState *state,
                               const AbMainMenuThemeBoundaries *boundaries,
                               float *camera_top, AbError *error);

#endif
