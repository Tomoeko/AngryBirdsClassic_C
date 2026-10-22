#ifndef AB_UI_MAIN_MENU_OPTIONS_H
#define AB_UI_MAIN_MENU_OPTIONS_H

#include "ab/ui_main_menu_slider.h"
#include "ab/ui_toggle_button.h"

typedef enum {
    AB_UI_OPTION_KEYBOARD,
    AB_UI_OPTION_QUALITY,
    AB_UI_OPTION_PUSH,
    AB_UI_OPTION_RESTORE,
    AB_UI_OPTION_CHILDLOCK,
    AB_UI_OPTION_CHINA_ABOUT,
    AB_UI_OPTION_MORE_GAMES,
    AB_UI_OPTION_CREDITS,
    AB_UI_OPTION_SOUND,
    AB_UI_OPTION_FULLSCREEN,
    AB_UI_OPTION_COUNT,
} AbUiMainMenuOption;

typedef struct {
    AbSpan device_model, customer;
    bool keyboard_on_main_menu, keyboard_in_external_slider, remote_config_present;
    AbUiPointerValue remote_keyboard_android, remote_keyboard_ios;
    bool allow_low_quality, low_quality, disable_notifications, notifications_enabled;
    bool mighty_eagle_buttons, ios, iap_enabled, mighty_eagle_enabled;
    bool powerups_enabled, freemium_china, sugotoku, childlock_on;
} AbUiMainMenuOptionsSource;

typedef struct {
    AbUiWidget *main_menu;
    AbUiMainMenuSlider *slider;
    AbUiWidget plain[6];
    AbUiToggleButton toggles[4];
    AbDataEntry image_entries[4][2];
    AbDataTable image_states[4];
    AbUiWidget *controls[AB_UI_OPTION_COUNT];
    bool initialized;
} AbUiMainMenuOptions;

/* Own the actual createOptionsSliderButtons/addKeyboardButtonToSlider graph.
 * Inputs are immutable resolved source feature/config/settings results; Boolean
 * fields represent Lua truthiness. Acquisition, persistence and action handlers
 * belong to the application owner. No feature or SDK response is synthesized.
 * Preserve source order and keyboard insertion, per-button resource/state writes,
 * entered-child lifecycle and final parent layout (including the keyboard
 * helper's separate layout). Sound starts at state1 here; entry synchronization
 * of the saved mute state is a separate MainMenu method.
 *
 * Fresh stable caller storage owns controls and mutable image arrays. The menu,
 * slider, resources and their child storage remain borrowed; reserve enough
 * slider capacity for the selected branches. Earlier controls/effects remain on
 * failure, and the shared context stops. No allocation, VM or navigation occurs. */
bool ab_ui_main_menu_options_init(AbUiMainMenuOptions *options, AbUiWidget *main_menu,
                                  AbUiMainMenuSlider *slider,
                                  const AbUiMainMenuOptionsSource *source,
                                  AbError *error);

#endif
