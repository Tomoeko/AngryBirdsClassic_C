#ifndef AB_UI_HUD_BUTTONS_H
#define AB_UI_HUD_BUTTONS_H

#include "ab/ui_widget.h"

typedef struct {
    AbUiWidget widget;
    const AbUiPointerValue *using_gesture_controls;
} AbUiHudPauseButton;

typedef enum {
    AB_UI_SKIP_HIDDEN,
    AB_UI_SKIP_APPEAR,
    AB_UI_SKIP_ONSCREEN,
    AB_UI_SKIP_DISAPPEAR,
    AB_UI_SKIP_GONE,
    AB_UI_SKIP_CLICKED,
} AbUiHudSkipState;

typedef struct {
    AbUiWidget widget;
    AbUiHudSkipState state;
    float scale_multiplier;
} AbUiHudSkipButton;

/* Concrete GameHud.lua button subclasses. Caller owns fresh stable storage,
 * child arrays and borrowed dependencies. PauseButton forwards constructor
 * arguments to ImageButton. Only the osx class installs its gesture-aware draw:
 * a truthy live gesture flag suppresses drawing/sets enabled=false; otherwise
 * draw with parent x/y only, then set enabled=true without a resource refresh.
 * Other platforms keep the ordinary ImageButton draw method.
 *
 * SkipButton.init ignores constructor arguments, initializes ImageButton first,
 * then queries actual isLevelEnding. Its update calls the real ImageButton base
 * before the source appearance/disappearance state changes. Drawing multiplies
 * the incoming scales by the live multiplier. Frame visibility, return values,
 * exact clamp boundaries and mutation prefixes remain shared canonical fields.
 * GameHud's later image/name/visibility/action assignments are separate.
 * No allocation, resource loading, session defaults or VM. Reached failures stop
 * the shared context; custom/malformed state strings remain unsupported. */
bool ab_ui_hud_pause_button_init(AbUiHudPauseButton *button, AbUiWidgetContext *context,
                                 AbUiWidgetChildren children,
                                 const AbUiImageButtonArguments *arguments, bool osx,
                                 const AbUiPointerValue *using_gesture_controls,
                                 AbError *error);
bool ab_ui_hud_skip_button_init(AbUiHudSkipButton *button, AbUiWidgetContext *context,
                                AbUiWidgetChildren children, void *query_context,
                                bool (*is_level_ending)(void *context,
                                                        AbUiPointerValue *output,
                                                        AbError *error),
                                AbError *error);

#endif
