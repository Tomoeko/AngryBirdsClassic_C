#include "ui_game_hud_internal.h"

#include <string.h>

static AbUiPointerValue boolean(bool value) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = value};
}

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static AbDataValue global(AbUiGameHudControls *controls, const char *name) {
    const AbDataValue *value = ab_data_get(controls->host.globals, text(name));
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool visible(AbUiWidget *widget, bool present, AbUiPointerValue value,
                    AbError *error) {
    if (!present || !widget->initialized || !widget->frame)
        return ab_fail(error, "Missing required live GameHud pause control");
    widget->frame->fields.visible = value;
    return true;
}

static bool eagle(AbUiGameHudControls *controls, bool show, bool returning,
                  AbError *error) {
    AbUiPointerValue available;
    if (!ab_ui_game_hud_eagle_query(controls, AB_UI_EAGLE_SHOW_ELEMENTS, &available,
                                    error))
        return false;
    if (!ab_ui_pointer_value_truthy(&available))
        return true;
    if (!controls->eagle_present)
        return ab_fail(error, "Missing required live GameHud pause Eagle");
    if (returning)
        show = !truthy(global(controls, "eagleBaitLaunched"));
    return ab_ui_mighty_eagle_button_visible(&controls->eagle, boolean(show), error);
}

static bool powerups(AbUiGameHudControls *controls, bool show, AbError *error) {
    if (!truthy(global(controls, "g_powerups_enabled")))
        return true;
    if (!controls->powerups_present)
        return ab_fail(error, "Missing required live GameHud pause powerup slider");
    /* The ordinary Android constructor has no birds_button. Foreign graph
     * mutations and alternate constructors are outside this owner's domain. */
    AbUiPointerValue value = boolean(false);
    if (show &&
        !ab_ui_game_hud_query(controls, AB_UI_HUD_POWERUP_VISIBLE, &value, error))
        return false;
    return visible(&controls->powerups.slider.box.widget, controls->powerups_present,
                   value, error);
}

static bool blackbird(AbUiGameHudControls *controls, bool show, bool returning,
                      AbError *error) {
    if (!truthy(global(controls, "g_show_ingame_black_bird_button")) ||
        !truthy(global(controls, "g_powerups_enabled")))
        return true;
    if (!controls->blackbird_present)
        return ab_fail(error, "Missing required live GameHud pause black-bird control");
    AbUiPointerValue value = boolean(false);
    AbUiGameHudQuery query = returning && truthy(global(controls, "Sugotoku"))
                                 ? AB_UI_HUD_POWERUP_VISIBLE
                                 : AB_UI_HUD_SHOCKWAVE_VISIBLE;
    if (show && !ab_ui_game_hud_query(controls, query, &value, error))
        return false;
    return visible(&controls->blackbird.button, controls->blackbird_present, value,
                   error);
}

static bool rmf(AbUiGameHudControls *controls, bool show, AbError *error) {
    if (!controls->rmf_present)
        return true;
    if (show) {
        AbUiPointerValue value;
        if (!ab_ui_game_hud_query(controls, AB_UI_HUD_RMF_VISIBLE, &value, error))
            return false;
        if (!ab_ui_pointer_value_truthy(&value))
            return true;
    }
    return visible(&controls->rmf, true, boolean(show), error);
}

static bool controls_visibility(AbUiGameHudControls *controls, bool show,
                                bool returning, AbError *error) {
    if (!visible(&controls->pause.widget, true, boolean(show), error) ||
        !visible(&controls->skip.widget, true,
                 show ? controls->show_skip_button : boolean(false), error) ||
        !eagle(controls, show, returning, error) || !powerups(controls, show, error))
        return false;
    if (ab_ui_pointer_value_truthy(&controls->host.restart_control) &&
        !visible(&controls->restart.widget, controls->restart_present, boolean(show),
                 error))
        return false;
    return blackbird(controls, show, returning, error) && rmf(controls, show, error);
}

static bool back_notice(AbUiGameHudControls *controls, AbError *error) {
    AbUiPausePageHost *host = &controls->host.pause;
    const AbDataValue *id =
        ab_data_get(host->event_ids, text("EID_BACK_TO_GAME_CLICKED"));
    if (!host->events || !id || id->kind == AB_DATA_NIL || !controls->host.notices ||
        controls->notice_count >= controls->host.notice_capacity)
        return ab_fail(error, "Missing actual GameHud return event/notice storage");
    AbUiGameHudNotice *notice = controls->host.notices + controls->notice_count++;
    notice->id = (AbDataEntry){text("id"), *id};
    notice->table = (AbDataTable){&notice->id, 1, 1, NULL};
    return ab_event_manager_queue(host->events, &notice->table, error);
}

static bool pause_action(AbUiGameHudControls *controls, AbUiGameHudPauseAction action,
                         AbError *error) {
    bool show = action == AB_UI_GAME_HUD_SHOW_PAUSE ||
                action == AB_UI_GAME_HUD_INSTANT_SHOW_PAUSE;
    bool returning = action == AB_UI_GAME_HUD_RETURN_TO_GAME;
    if (!controls_visibility(controls, !show, returning, error))
        return false;
    AbUiPausePage *page = &controls->pause_page;
    bool instant = action == AB_UI_GAME_HUD_INSTANT_SHOW_PAUSE ||
                   action == AB_UI_GAME_HUD_INSTANT_HIDE_PAUSE;
    if (instant) {
        page->extend_state = show ? 1 : 0;
        if (!ab_ui_widget_lifecycle(&page->widget, AB_UI_FRAME_LAYOUT, error))
            return false;
    }
    if (!(show ? ab_ui_pause_page_show(page, error)
               : ab_ui_pause_page_hide(page, error)))
        return false;
    if (instant)
        page->widget.frame->fields.visible = boolean(show);
    if (!returning)
        return true;
    if (!back_notice(controls, error))
        return false;
    if (!controls->host.sling)
        return ab_fail(error, "Missing actual GameHud return slingshot owner");
    return ab_classic_sling_cancel_drag(controls->host.sling, error);
}

bool ab_ui_game_hud_controls_pause(AbUiGameHudControls *controls,
                                   AbUiGameHudPauseAction action, AbError *error) {
    if (!controls || !controls->initialized || !controls->widget.context ||
        controls->widget.context->failed || controls->pause_active ||
        controls->layout_active || (unsigned)action > AB_UI_GAME_HUD_RETURN_TO_GAME)
        return ab_fail(error, "Invalid, active or stopped GameHud pause owner");
    controls->pause_active = true;
    bool okay = pause_action(controls, action, error);
    controls->pause_active = false;
    if (!okay)
        controls->widget.context->failed = true;
    return okay;
}
