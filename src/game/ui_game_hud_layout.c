#include "ui_game_hud_internal.h"

#include <math.h>
#include <string.h>

static AbUiPointerValue number(float value) {
    AbUiPointerValue output = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&output.number_bits, &value, sizeof(value));
    return output;
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    AbDataValue key = {.kind = AB_DATA_STRING,
                       .string = {(const uint8_t *)name, strlen(name)}};
    const AbDataValue *value = ab_data_get(table, key);
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool numeric(AbUiPointerValue value, float *output, AbError *error) {
    if (value.kind == AB_UI_POINTER_NUMBER) {
        memcpy(output, &value.number_bits, sizeof(*output));
        if (isfinite(*output))
            return true;
    }
    return ab_fail(error, "GameHud layout requires finite numeric geometry");
}

static bool scale_to(AbUiGameHudControls *controls, AbUiWidget *widget,
                     AbError *error) {
    return ab_ui_scaling_set_to(controls->host.pause.scaling, &widget->frame->fields,
                                error);
}

static bool refresh(AbUiWidget *widget, AbError *error) {
    return ab_ui_image_state_refresh(&widget->state.image, &widget->context->images,
                                     error);
}

static bool position_pause(AbUiGameHudControls *controls, float scale, AbVec2 margin,
                           float *lower_y, AbError *error) {
    AbUiWidget *pause = &controls->pause.widget;
    if (!refresh(pause, error))
        return false;
    float px = 0, py = 0, height = 0;
    if (!numeric(pause->state.image.resource_pivot_x, &px, error) ||
        !numeric(pause->state.image.resource_pivot_y, &py, error))
        return false;
    pause->frame->fields.x = number((px * 1.1f) * scale + margin.x);
    pause->frame->fields.y = number((py * 1.1f) * scale + margin.y);
    if (!numeric(pause->frame->fields.h, &height, error))
        return false;
    float y = (py * 1.1f) * scale + margin.y;
    *lower_y = y + height * scale;
    return scale_to(controls, pause, error);
}

static bool blackbird(AbUiGameHudControls *controls, float scale, float lower_y,
                      AbError *error) {
    if (!truthy(field(controls->host.globals, "g_show_ingame_black_bird_button")) ||
        !truthy(field(controls->host.globals, "g_powerups_enabled")))
        return true;
    if (!controls->blackbird_present)
        return ab_fail(error, "Live GameHud layout has no required black-bird control");
    AbUiHudPowerup *power = &controls->blackbird;
    power->button.frame->fields.x = controls->pause.widget.frame->fields.x;
    power->button.frame->fields.y = number(lower_y);
    AbUiPointerValue image = power->background.state.image.image;
    if (image.kind != AB_UI_POINTER_TEXT)
        return ab_fail(error, "Invalid GameHud black-bird counter image");
    AbUiImageResourceBoundaries *images = &controls->widget.context->images;
    AbVec2 size, pivot;
    if (!ab_ui_scaling_sprite(controls->host.pause.scaling, images, image.text, false,
                              &size, error))
        return false;
    if (!images->pivot)
        return ab_fail(error, "Missing actual GameHud counter pivot provider");
    if (!images->pivot(images->context, image.text, &pivot, error))
        return false;
    pivot.x = pivot.x * scale;
    pivot.y = pivot.y * scale;
    power->background.frame->fields.y = number(size.y * 0.75f);
    power->counter.widget.frame->fields.y = number(size.y * 0.52f - pivot.y);
    power->counter.widget.frame->fields.x = number(0);
    if (!scale_to(controls, &power->button, error))
        return false;
    power->tag.frame->fields.x = power->counter.widget.frame->fields.x;
    float counter_y = 0;
    if (!numeric(power->counter.widget.frame->fields.y, &counter_y, error))
        return false;
    power->tag.frame->fields.y = number(counter_y + 0.7f * size.y);
    power->tag.frame->fields.scale_x = number(1.2f);
    power->tag.frame->fields.scale_y = power->tag.frame->fields.scale_x;
    return true;
}

static bool rmf(AbUiGameHudControls *controls, float scale, float lower_y,
                AbVec2 margin, AbError *error) {
    if (!truthy(field(controls->host.globals, "g_powerups_enabled")) ||
        !controls->rmf_present)
        return true;
    bool beside = false;
    if (truthy(field(controls->host.globals, "g_show_ingame_black_bird_button"))) {
        AbUiPointerValue visible;
        if (!ab_ui_game_hud_query(controls, AB_UI_HUD_SHOCKWAVE_VISIBLE, &visible,
                                  error))
            return false;
        beside = ab_ui_pointer_value_truthy(&visible);
    }
    AbUiWidget *pause = &controls->pause.widget;
    if (beside) {
        float px = 0;
        if (!numeric(pause->state.image.resource_pivot_x, &px, error))
            return false;
        controls->rmf.frame->fields.x = number((scale * px) * 3.2f + margin.x);
    } else
        controls->rmf.frame->fields.x = pause->frame->fields.x;
    controls->rmf.frame->fields.y = number(lower_y);
    return scale_to(controls, &controls->rmf, error);
}

static bool upper_control(AbUiGameHudControls *controls, AbUiWidget *widget,
                          float scale, AbVec2 margin, bool scale_first,
                          AbError *error) {
    if (scale_first && !scale_to(controls, widget, error))
        return false;
    if (!refresh(widget, error))
        return false;
    float px = 0;
    if (!numeric(controls->pause.widget.state.image.resource_pivot_x, &px, error))
        return false;
    widget->frame->fields.x = number((scale * px) * 3.2f + margin.x);
    widget->frame->fields.y = controls->pause.widget.frame->fields.y;
    return scale_first || scale_to(controls, widget, error);
}

static bool eagle_and_restart(AbUiGameHudControls *controls, float scale, AbVec2 margin,
                              AbError *error) {
    AbUiPointerValue visible;
    if (!ab_ui_game_hud_eagle_query(controls, AB_UI_EAGLE_SHOW_ELEMENTS, &visible,
                                    error))
        return false;
    if (ab_ui_pointer_value_truthy(&visible)) {
        if (!controls->eagle_present)
            return ab_fail(error, "Live GameHud layout has no required Eagle control");
        if (!upper_control(controls, &controls->eagle.widget, scale, margin, true,
                           error))
            return false;
    }
    if (ab_ui_pointer_value_truthy(&controls->host.restart_control)) {
        if (!controls->restart_present)
            return ab_fail(error,
                           "Live GameHud layout has no required restart control");
        if (!upper_control(controls, &controls->restart.widget, scale, margin, false,
                           error))
            return false;
    }
    return true;
}

static bool ads(AbUiGameHudControls *controls, AbError *error) {
    if (!controls->ads_present)
        return true;
    AbDataValue value =
        field(controls->host.globals, "g_show_ads_off_x_button_on_gamehud");
    if (truthy(value)) {
        value = field(controls->host.globals, "g_use_ads");
        if (truthy(value))
            value = field(controls->host.globals, "hasIaps");
    }
    AbUiPointerValue visible = {0};
    if (!ab_ui_game_hud_project(value, &visible, error))
        return false;
    controls->ads_off.frame->fields.visible = visible;
    return scale_to(controls, &controls->ads_off, error);
}

static bool skip(AbUiGameHudControls *controls, float sx, float sy, AbVec2 screen,
                 AbVec2 margin, AbError *error) {
    AbUiWidget *widget = &controls->skip.widget;
    if (!refresh(widget, error))
        return false;
    widget->frame->fields.scale_x = number(sx);
    widget->frame->fields.scale_y = number(sy);
    float width = 0, height = 0;
    if (!numeric(widget->frame->fields.w, &width, error) ||
        !numeric(widget->frame->fields.h, &height, error))
        return false;
    widget->frame->fields.x = number((screen.x - (width * 0.55f) * sx) - margin.x);
    widget->frame->fields.y = number((screen.y - (height * 0.55f) * sy) - margin.y);
    return scale_to(controls, widget, error);
}

static bool powerups(AbUiGameHudControls *controls, float scale, AbVec2 screen,
                     AbVec2 margin, AbError *error) {
    if (!truthy(field(controls->host.globals, "g_powerups_enabled")))
        return true;
    AbUiPointerValue visible, disabled;
    if (!ab_ui_game_hud_eagle_query(controls, AB_UI_EAGLE_SHOW_ELEMENTS, &visible,
                                    error) ||
        !ab_ui_game_hud_eagle_query(controls, AB_UI_EAGLE_LEVEL_DISABLED, &disabled,
                                    error))
        return false;
    if (ab_ui_pointer_value_truthy(&disabled))
        visible = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    if (!controls->powerups_present)
        return ab_fail(error, "Live GameHud layout has no required powerup slider");
    if (!ab_ui_hud_powerups_layout(&controls->powerups, &controls->pause.widget, margin,
                                   ab_ui_pointer_value_truthy(&visible), error))
        return false;
    if (controls->banner) {
        controls->banner->frame->fields.scale_x = number(0.5f * scale);
        controls->banner->frame->fields.scale_y = number(0.5f * scale);
        controls->banner->frame->fields.x = number(screen.x * 0.5f);
        float height = 0;
        if (!numeric(controls->banner->frame->fields.h, &height, error))
            return false;
        controls->banner->frame->fields.y = number(screen.y - (height * 0.3f) * scale);
    }
    return true;
}

static bool layout(AbUiGameHudControls *controls, AbError *error) {
    float sx = 0, sy = 0;
    AbUiScaling *scaling = controls->host.pause.scaling;
    if (!ab_ui_scaling_get(scaling, &sx, error) ||
        !ab_ui_scaling_get(scaling, &sy, error))
        return false;
    AbVec2 screen = *controls->host.pause.screen, margin = *controls->host.pause.margin;
    float lower_y = 0;
    if (!position_pause(controls, sx, margin, &lower_y, error) ||
        !blackbird(controls, sx, lower_y, error) ||
        !rmf(controls, sx, lower_y, margin, error) ||
        !eagle_and_restart(controls, sx, margin, error) || !ads(controls, error) ||
        !skip(controls, sx, sy, screen, margin, error) ||
        !powerups(controls, sx, screen, margin, error))
        return false;
    if (truthy(field(controls->host.globals, "startedFromEditor")) &&
        controls->eagle_present &&
        !ab_ui_mighty_eagle_button_visible(
            &controls->eagle,
            (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true}, error))
        return false;
    return ab_ui_widget_lifecycle_base(&controls->widget, AB_UI_FRAME_LAYOUT, error);
}

bool ab_ui_game_hud_controls_layout(AbUiGameHudControls *controls, AbError *error) {
    if (!controls || !controls->initialized || controls->layout_active ||
        controls->pause_active || !controls->widget.context ||
        controls->widget.context->failed || !controls->host.pause.screen ||
        !controls->host.pause.margin)
        return ab_fail(error, "Invalid, stopped or active GameHud layout owner");
    controls->layout_active = true;
    bool okay = layout(controls, error);
    controls->layout_active = false;
    if (!okay)
        controls->widget.context->failed = true;
    return okay;
}
