#include "ab/ui_main_menu_options.h"

#include <string.h>

static AbUiPointerValue text(const char *name) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                              .text = {(const uint8_t *)name, strlen(name)}};
}

static bool named(AbSpan value, const char *name) {
    return value.size == strlen(name) &&
           (!value.size || (value.data && !memcmp(value.data, name, value.size)));
}

static bool parent_layout(AbUiMainMenuOptions *options, AbError *error) {
    return !ab_ui_pointer_value_truthy(&options->main_menu->tree.entered) ||
           ab_ui_widget_lifecycle(options->main_menu, AB_UI_FRAME_LAYOUT, error);
}

static AbUiWidget *plain(AbUiMainMenuOptions *options, AbUiMainMenuOption role,
                         const char *name, const char *image, AbError *error) {
    static const AbUiMainMenuOption roles[] = {
        AB_UI_OPTION_KEYBOARD,   AB_UI_OPTION_RESTORE, AB_UI_OPTION_CHINA_ABOUT,
        AB_UI_OPTION_MORE_GAMES, AB_UI_OPTION_CREDITS, AB_UI_OPTION_FULLSCREEN};
    size_t slot = 0;
    while (slot < 6 && roles[slot] != role)
        ++slot;
    if (slot == 6) {
        ab_fail(error, "Invalid plain main-menu option role");
        return NULL;
    }
    AbUiWidget *widget = options->plain + slot;
    AbUiWidgetContext *context = options->main_menu->context;
    if (!ab_ui_widget_init(widget, context, AB_UI_WIDGET_IMAGE_BUTTON,
                           (AbUiWidgetChildren){0}, NULL, NULL, NULL, error))
        return NULL;
    options->controls[role] = widget;
    widget->state.image.frame.fields.name = text(name);
    return ab_ui_image_button_state_set_image(&widget->state, text(image),
                                              (AbUiPointerValue){0}, &context->images,
                                              error)
               ? widget
               : NULL;
}

static AbUiToggleButton *toggle(AbUiMainMenuOptions *options, AbUiMainMenuOption role,
                                const char *name, const char *first, const char *second,
                                AbError *error) {
    static const AbUiMainMenuOption roles[] = {AB_UI_OPTION_QUALITY, AB_UI_OPTION_PUSH,
                                               AB_UI_OPTION_CHILDLOCK,
                                               AB_UI_OPTION_SOUND};
    size_t slot = 0;
    while (slot < 4 && roles[slot] != role)
        ++slot;
    if (slot == 4) {
        ab_fail(error, "Invalid toggle main-menu option role");
        return NULL;
    }
    AbUiToggleButton *button = options->toggles + slot;
    if (!ab_ui_toggle_button_init(button, options->main_menu->context,
                                  (AbUiWidgetChildren){0}, NULL, error))
        return NULL;
    options->controls[role] = &button->widget;
    button->widget.state.image.frame.fields.name = text(name);
    const char *images[] = {first, second};
    for (size_t index = 0; index < 2; ++index) {
        float ordinal = (float)index + 1;
        AbDataValue key = {.kind = AB_DATA_NUMBER};
        memcpy(&key.bits, &ordinal, sizeof(ordinal));
        options->image_entries[slot][index] = (AbDataEntry){
            key, {.kind = AB_DATA_STRING, .string = text(images[index]).text}};
    }
    options->image_states[slot] =
        (AbDataTable){options->image_entries[slot], 2, 2, NULL};
    return ab_ui_toggle_button_set_images(button, options->image_states + slot,
                                          (AbUiPointerValue){0}, error)
               ? button
               : NULL;
}

static bool add_control(AbUiMainMenuOptions *options, AbUiWidget *widget,
                        AbError *error) {
    widget->state.activate_on_release =
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    return ab_ui_widget_add(&options->slider->box.widget, widget, error);
}

static bool attach(AbUiMainMenuOptions *options, AbUiWidget *widget, const char *action,
                   bool explicit_sound, AbError *error) {
    widget->state.return_value = text(action);
    if (explicit_sound)
        widget->state.click_sound = text("menu_confirm");
    return add_control(options, widget, error);
}

static bool keyboard(AbUiMainMenuOptions *options,
                     const AbUiMainMenuOptionsSource *source, AbError *error) {
    bool apple =
        named(source->device_model, "ipad") || named(source->device_model, "iphone");
    bool enabled = !apple;
    if (source->remote_config_present &&
        (apple || named(source->device_model, "android"))) {
        const AbUiPointerValue *value =
            apple ? &source->remote_keyboard_ios : &source->remote_keyboard_android;
        if (!ab_ui_pointer_value_valid(value))
            return ab_fail(error, "Invalid main-menu remote keyboard flag");
        if (!apple)
            enabled = true;
        if (value->kind != AB_UI_POINTER_NONE)
            enabled = ab_ui_pointer_value_truthy(value);
    }
    if (!enabled)
        return true;
    AbUiWidget *button = plain(options, AB_UI_OPTION_KEYBOARD, "showKeyboardButton",
                               "UI_CODE_UNLOCK_BUTTON_KEYBOARD", error);
    if (!button)
        return false;
    button->state.return_value = text("OPEN_KEYBOARD_FRAME");
    button->state.activate_on_release =
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    return ab_ui_widget_insert(&options->slider->box.widget, button, 1, error) &&
           parent_layout(options, error);
}

static bool build(AbUiMainMenuOptions *options, const AbUiMainMenuOptionsSource *source,
                  AbError *error) {
    if (source->keyboard_on_main_menu && !source->keyboard_in_external_slider &&
        !keyboard(options, source, error))
        return false;
    if (source->allow_low_quality) {
        AbUiToggleButton *button =
            toggle(options, AB_UI_OPTION_QUALITY, "qualityButton", "BUTTON_LQ",
                   "BUTTON_LOW_QUALITY", error);
        if (!button)
            return false;
        button->widget.state.return_value = text("TOGGLE_QUALITY");
        if (source->low_quality && !ab_ui_toggle_button_set_state(button, 2, error))
            return false;
        if (!add_control(options, &button->widget, error))
            return false;
    }
    if (named(source->device_model, "android") && !source->disable_notifications) {
        AbUiToggleButton *button =
            toggle(options, AB_UI_OPTION_PUSH, "push_notification_button",
                   "BUTTON_MENU_PUSH", "BUTTON_PUSH_OFF", error);
        if (!button)
            return false;
        button->widget.state.return_value = text("TOGGLE_PUSH_NOTIFICATIONS");
        if (!ab_ui_toggle_button_set_state(
                button, source->notifications_enabled ? 1 : 2, error) ||
            !add_control(options, &button->widget, error))
            return false;
    }
    if (source->mighty_eagle_buttons && source->ios && source->iap_enabled &&
        !source->mighty_eagle_enabled) {
        AbUiWidget *button = plain(options, AB_UI_OPTION_RESTORE, "iap_restore_button",
                                   "BUTTON_EAGLE_RESTORE", error);
        if (!button || !attach(options, button, "IAP_RESTORE_CLICKED", true, error))
            return false;
    }
    if (source->powerups_enabled && !source->freemium_china && !source->sugotoku) {
        AbUiToggleButton *button =
            toggle(options, AB_UI_OPTION_CHILDLOCK, "childlockButton",
                   "BUTTON_MENU_POWERUP", "BUTTON_CHILDLOCK_ON", error);
        if (!button)
            return false;
        button->widget.state.return_value = text("TOGGLE_CHILDLOCK");
        if (source->childlock_on && !ab_ui_toggle_button_set_state(button, 2, error))
            return false;
        if (!add_control(options, &button->widget, error))
            return false;
    }
    if (source->freemium_china) {
        AbUiWidget *button = plain(options, AB_UI_OPTION_CHINA_ABOUT,
                                   "talkwebAboutButton", "BUTTON_ABOUT_CHINA", error);
        if (!button || !attach(options, button, "TALKWEB_POPUP", true, error))
            return false;
        if (named(source->customer, "gamehall") ||
            named(source->customer, "chinatelecom")) {
            button = plain(options, AB_UI_OPTION_MORE_GAMES, "moreGamesButton",
                           "BUTTON_MOREGAMES", error);
            if (!button || !attach(options, button, "GOTO_MOREGAMES", true, error))
                return false;
        }
    }
    AbUiWidget *credits = plain(options, AB_UI_OPTION_CREDITS, "creditsButton",
                                "BUTTON_ABOUT_SMALL", error);
    if (!credits || !attach(options, credits, "GOTO_CREDITS", true, error))
        return false;
    AbUiToggleButton *sound =
        toggle(options, AB_UI_OPTION_SOUND, "soundButton", "BUTTON_SOUNDS_SMALL",
               "BUTTON_MAINMENU_SFX_OFF", error);
    if (!sound || !attach(options, &sound->widget, "TOGGLE_SOUND", false, error))
        return false;
    if (named(source->device_model, "windows") || named(source->device_model, "osx")) {
        AbUiWidget *button = plain(options, AB_UI_OPTION_FULLSCREEN,
                                   "fullscreen_button", "BUTTON_FULLSCREEN", error);
        if (!button || !attach(options, button, "TOGGLE_FULLSCREEN", true, error))
            return false;
    }
    return parent_layout(options, error);
}

bool ab_ui_main_menu_options_init(AbUiMainMenuOptions *options, AbUiWidget *main_menu,
                                  AbUiMainMenuSlider *slider,
                                  const AbUiMainMenuOptionsSource *source,
                                  AbError *error) {
    if (!options || options->initialized || !main_menu || !main_menu->initialized ||
        !slider || !slider->box.widget.initialized || !source ||
        main_menu->context != slider->box.widget.context || !main_menu->context ||
        main_menu->context->failed ||
        (source->device_model.size && !source->device_model.data) ||
        (source->customer.size && !source->customer.data))
        return ab_fail(error, "Invalid main-menu options owner/source");
    options->main_menu = main_menu;
    options->slider = slider;
    options->initialized = true;
    bool okay = build(options, source, error);
    if (!okay)
        main_menu->context->failed = true;
    return okay;
}
