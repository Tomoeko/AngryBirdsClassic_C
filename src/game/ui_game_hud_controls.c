#include "ab/ui_game_hud_controls.h"
#include "ui_game_hud_internal.h"

#include <math.h>
#include <string.h>

static AbUiPointerValue text(const char *name) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                              .text = {(const uint8_t *)name, strlen(name)}};
}

static AbUiPointerValue number(float value) {
    AbUiPointerValue output = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&output.number_bits, &value, sizeof(value));
    return output;
}

static AbUiPointerValue boolean(bool value) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = value};
}

static AbDataValue key(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = text(name).text};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, key(name));
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool named(AbSpan value, const char *name) {
    return value.size == strlen(name) &&
           (!value.size || (value.data && !memcmp(value.data, name, value.size)));
}

static bool literal_false(AbDataValue value) {
    return value.kind == AB_DATA_BOOLEAN && !value.bits;
}

bool ab_ui_game_hud_project(AbDataValue value, AbUiPointerValue *output,
                            AbError *error) {
    AbUiPointerValue result = {0};
    switch (value.kind) {
        case AB_DATA_NIL:
            break;
        case AB_DATA_BOOLEAN:
            result = boolean(value.bits != 0);
            break;
        case AB_DATA_NUMBER:
            result = (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER,
                                        .number_bits = value.bits};
            break;
        case AB_DATA_STRING:
            result =
                (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = value.string};
            break;
        case AB_DATA_TABLE:
            result = (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT,
                                        .object = (uintptr_t)value.table};
            break;
        case AB_DATA_OBJECT:
            result = (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT,
                                        .object = (uintptr_t)value.object};
            break;
        default:
            return ab_fail(error, "Unsupported GameHud source value");
    }
    if (!ab_ui_pointer_value_valid(&result))
        return ab_fail(error, "Invalid GameHud source value");
    *output = result;
    return true;
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind == AB_DATA_NUMBER) {
        memcpy(output, &value.bits, sizeof(*output));
        if (isfinite(*output))
            return true;
    }
    return ab_fail(error, "GameHud construction requires finite bird counts");
}

bool ab_ui_game_hud_query(AbUiGameHudControls *controls, AbUiGameHudQuery kind,
                          AbUiPointerValue *output, AbError *error) {
    if (!controls->host.query)
        return ab_fail(error, "Missing actual GameHud construction query");
    if (!controls->host.query(controls->host.context, controls, kind, output, error))
        return false;
    return ab_ui_pointer_value_valid(output) ||
           ab_fail(error, "Invalid actual GameHud query result");
}

bool ab_ui_game_hud_eagle_query(AbUiGameHudControls *controls, AbUiEagleQuery kind,
                                AbUiPointerValue *output, AbError *error) {
    AbUiMightyEagleHost *host = &controls->host.eagle;
    if (!host->query)
        return ab_fail(error, "Missing actual GameHud Eagle query binding");
    if (!host->query(host->context, kind, output, error))
        return false;
    return ab_ui_pointer_value_valid(output) ||
           ab_fail(error, "Invalid actual GameHud Eagle query result");
}

static bool eagle_query(void *context, AbUiPointerValue *output, AbError *error) {
    return ab_ui_game_hud_eagle_query(context, AB_UI_EAGLE_LEVEL_ENDING, output, error);
}

static bool show_eagle(AbUiGameHudControls *controls, AbUiPointerValue *output,
                       AbError *error) {
    return ab_ui_game_hud_eagle_query(controls, AB_UI_EAGLE_SHOW_ELEMENTS, output,
                                      error);
}

static bool add_hud(AbUiGameHudControls *controls, AbUiWidget *widget, AbError *error) {
    if (!ab_ui_widget_add(&controls->widget, widget, error))
        return false;
    if (controls->hud_button_count == AB_UI_GAME_HUD_BUTTONS)
        return ab_fail(error, "GameHud button list exceeds its source bound");
    controls->hud_buttons[controls->hud_button_count++] = widget;
    return true;
}

static bool image(AbUiWidget *widget, const char *enabled, const char *disabled,
                  AbError *error) {
    return ab_ui_image_button_state_set_image(&widget->state, text(enabled),
                                              disabled ? text(disabled)
                                                       : (AbUiPointerValue){0},
                                              &widget->context->images, error);
}

static bool button(AbUiGameHudControls *controls, AbUiWidget *widget,
                   AbUiWidgetChildren children, const char *name, const char *action,
                   AbError *error) {
    AbUiImageButtonArguments args = {.image.frame.name = text(name),
                                     .return_value =
                                         action ? text(action) : (AbUiPointerValue){0}};
    return ab_ui_widget_init(widget, controls->widget.context,
                             AB_UI_WIDGET_IMAGE_BUTTON, children, NULL, NULL, &args,
                             error);
}

static bool restart(AbUiGameHudControls *controls, AbError *error) {
    if (!ab_ui_pointer_value_truthy(&controls->host.restart_control))
        return true;
    AbUiImageButtonArguments args = {.image.frame.name = text("restart_button"),
                                     .return_value = text("RESTART_LEVEL")};
    if (!ab_ui_hud_pause_button_init(&controls->restart, controls->widget.context,
                                     (AbUiWidgetChildren){0}, &args, false, NULL,
                                     error) ||
        !image(&controls->restart.widget, "INGAME_BUTTON_REPLAY", NULL, error) ||
        !add_hud(controls, &controls->restart.widget, error))
        return false;
    controls->restart_present = true;
    return true;
}

static bool ads(AbUiGameHudControls *controls, AbError *error) {
    if (!truthy(field(controls->host.globals, "g_use_ads")))
        return true;
    AbUiPointerValue premium;
    if (!ab_ui_game_hud_query(controls, AB_UI_HUD_PREMIUM, &premium, error))
        return false;
    if (ab_ui_pointer_value_truthy(&premium))
        return true;
    if (!button(controls, &controls->ads_off, (AbUiWidgetChildren){0},
                "purchaseAdsOffButton", "PURCHASE_ADS_OFF", error) ||
        !image(&controls->ads_off, "BUTTON_X_CLOSE", NULL, error) ||
        !ab_ui_game_hud_project(field(controls->host.globals, "hasIaps"),
                                &controls->ads_off.frame->fields.visible, error) ||
        !add_hud(controls, &controls->ads_off, error))
        return false;
    controls->ads_present = true;
    return true;
}

static bool skip(AbUiGameHudControls *controls, AbError *error) {
    AbUiWidget *widget = &controls->skip.widget;
    if (!ab_ui_hud_skip_button_init(&controls->skip, controls->widget.context,
                                    (AbUiWidgetChildren){0}, controls, eagle_query,
                                    error))
        return false;
    widget->frame->fields.name = text("skip_button");
    if (!image(widget, "BUTTON_NEXTLEVEL_SKIP", NULL, error))
        return false;
    widget->frame->fields.visible = boolean(false);
    widget->state.activate_on_release = boolean(true);
    widget->state.return_value = text("SKIP_TO_LEVEL_END");
    return add_hud(controls, widget, error);
}

static bool powerups(AbUiGameHudControls *controls, AbError *error) {
    if (!truthy(field(controls->host.globals, "g_powerups_enabled")))
        return true;
    AbUiHudPowerupsSource source = {0};
    if (!ab_ui_game_hud_query(controls, AB_UI_HUD_POWERUP_VISIBLE, &source.visible,
                              error) ||
        !numeric(field(controls->host.globals, "birdsShot"), &source.birds_shot,
                 error) ||
        !numeric(field(controls->host.globals, "birdCount"), &source.bird_count, error))
        return false;
    source.plus_signs =
        truthy(field(controls->host.globals, "g_use_plus_signs_on_powerups"));
    if (source.plus_signs) {
        AbUiPointerValue trial;
        if (!ab_ui_game_hud_query(controls, AB_UI_HUD_TRIAL, &trial, error))
            return false;
        source.trial = ab_ui_pointer_value_truthy(&trial);
    }
    AbUiHudPowerupsHost host = controls->host.powerups;
    host.device_model = text("android").text;
    if (!ab_ui_game_hud_project(field(controls->host.globals, "g_powerups_enabled"),
                                &host.global_enabled, error) ||
        !ab_ui_hud_powerups_init(&controls->powerups, &controls->widget, host, source,
                                 error))
        return false;
    controls->powerups_present = true;
    return true;
}

static bool mattel(AbUiGameHudControls *controls, bool *output, AbError *error) {
    AbDataValue value = field(controls->host.globals, "Mattel");
    if (!truthy(value)) {
        *output = false;
        return true;
    }
    if (value.kind != AB_DATA_TABLE || !value.table)
        return ab_fail(error, "GameHud indexes an invalid Mattel source object");
    *output = truthy(field(value.table, "g_mattel_active"));
    return true;
}

static bool score_indicator(AbUiGameHudControls *controls, AbError *error) {
    if (!controls->powerups_present ||
        !truthy(field(controls->host.globals, "g_separate_powerup_highscore")))
        return true;
    AbUiImageArguments args = {.frame.name = text("powerupHighscoreIndicator")};
    if (!ab_ui_widget_init(&controls->highscore_indicator, controls->widget.context,
                           AB_UI_WIDGET_IMAGE, (AbUiWidgetChildren){0}, NULL, &args,
                           NULL, error) ||
        !ab_ui_image_state_set_image(&controls->highscore_indicator.state.image,
                                     text("INGAME_BUTTON_POWERUPANIM"),
                                     &controls->widget.context->images, error))
        return false;
    controls->highscore_indicator.frame->fields.visible = boolean(false);
    if (!ab_ui_widget_add(&controls->widget, &controls->highscore_indicator, error))
        return false;
    controls->highscore_present = true;
    return true;
}

static bool banner(AbUiGameHudControls *controls, AbUiPointerValue returning,
                   AbError *error) {
    if (!controls->powerups_present ||
        truthy(field(controls->host.globals, "levelRestartedFrom")) ||
        ab_ui_pointer_value_truthy(&returning))
        return true;
    AbUiPointerValue selected;
    if (!ab_ui_game_hud_query(controls, AB_UI_HUD_OFFER_ON_START, &selected, error))
        return false;
    if (!ab_ui_pointer_value_truthy(&selected))
        return true;
    if (!ab_ui_game_hud_query(controls, AB_UI_HUD_POWERUP_VISIBLE, &selected, error))
        return false;
    if (!ab_ui_pointer_value_truthy(&selected) ||
        truthy(field(controls->host.level->episode, "powerups_disabled")))
        return true;
    bool active = false;
    if (!mattel(controls, &active, error))
        return false;
    if (active)
        return true;
    if (!ab_ui_game_hud_query(controls, AB_UI_HUD_CHILDLOCK, &selected, error))
        return false;
    if (ab_ui_pointer_value_truthy(&selected))
        return true;
    if (!controls->host.sale_banner)
        return ab_fail(error, "Missing actual GameHud SaleBanner constructor");
    if (!controls->host.sale_banner(controls->host.context, &controls->banner, error))
        return false;
    if (!controls->banner || !controls->banner->initialized ||
        controls->banner->context != controls->widget.context)
        return ab_fail(error, "Invalid actual GameHud SaleBanner instance");
    controls->banner->frame->fields.name = text("shopAdBanner");
    return ab_ui_widget_add(&controls->widget, controls->banner, error);
}

static bool eagle(AbUiGameHudControls *controls, AbError *error) {
    AbUiPointerValue visible;
    if (!show_eagle(controls, &visible, error))
        return false;
    if (!ab_ui_pointer_value_truthy(&visible))
        return true;
    AbUiImageButtonArguments args = {.image.frame.name = text("eagleButton"),
                                     .return_value = text("ME_BUTTON_CLICKED")};
    if (!ab_ui_mighty_eagle_button_init(&controls->eagle, controls->widget.context,
                                        &args, (AbUiPointerValue){0},
                                        controls->host.eagle, error))
        return false;
    AbUiWidget *widget = &controls->eagle.widget;
    widget->state.activate_on_release = boolean(true);
    widget->state.click_sound = text("menu_confirm");
    widget->padding[2] = number(50);
    if (!add_hud(controls, widget, error))
        return false;
    controls->eagle_present = true;
    return true;
}

static bool pause_button(AbUiGameHudControls *controls, AbError *error) {
    AbUiImageButtonArguments args = {.image.frame.name = text("pauseButton"),
                                     .return_value = text("PAUSE_GAME")};
    if (!ab_ui_hud_pause_button_init(&controls->pause, controls->widget.context,
                                     (AbUiWidgetChildren){0}, &args, false, NULL,
                                     error))
        return false;
    AbUiWidget *widget = &controls->pause.widget;
    if (!image(widget, "INGAME_BUTTON_PAUSE", NULL, error))
        return false;
    widget->state.click_sound = text("menu_confirm");
    widget->state.activate_on_release = boolean(true);
    widget->padding[2] = number(50);
    return add_hud(controls, widget, error);
}

static bool blackbird(AbUiGameHudControls *controls, AbError *error) {
    if (!truthy(field(controls->host.globals, "g_show_ingame_black_bird_button")) ||
        !truthy(field(controls->host.globals, "g_powerups_enabled")))
        return true;
    AbUiHudPowerup *power = &controls->blackbird;
    AbUiWidgetChildren children = {controls->blackbird_frames, controls->blackbird_draw,
                                   controls->blackbird_pointer, 3};
    if (!button(controls, &power->button, children, "blackBirdButton", NULL, error))
        return false;
    power->button.state.return_value = text("BB_BUTTON_CLICKED");
    if (!image(&power->button, "SHOCKWAVE_BUTTON", "SHOCKWAVE_BUTTON_GREY", error))
        return false;
    power->button.state.activate_on_release = boolean(true);
    power->button.state.image.alpha = number(0.7f);
    if (!ab_ui_game_hud_query(controls, AB_UI_HUD_SHOCKWAVE_VISIBLE,
                              &power->button.frame->fields.visible, error))
        return false;
    power->meta_fields[0] = (AbDataEntry){key("settingsName"), key("shockwave")};
    power->meta_fields[1] = (AbDataEntry){key("name"), key("shockwave")};
    power->meta = (AbDataTable){power->meta_fields, 2, 2, NULL};
    power->button.state.meta = (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT,
                                                  .object = (uintptr_t)&power->meta};
    if (!add_hud(controls, &power->button, error) ||
        !ab_ui_hud_powerup_add_counter(power, controls->host.powerups.fonts,
                                       text("android").text, error))
        return false;
    power->background.state.return_value = text("BB_BUTTON_CLICKED");
    AbUiAnimatedImage *animation = &controls->electricity;
    AbUiImageArguments args = {.frame.name = text("electricity")};
    if (!ab_ui_animated_image_init(animation, controls->widget.context,
                                   (AbUiWidgetChildren){0}, &args, error))
        return false;
    if (!isfinite(controls->host.default_profile))
        return ab_fail(error, "Missing actual GameHud default resource profile");
    float profile_scale = controls->host.default_profile == 480 ? 0.5f : 1;
    animation->widget.frame->fields.scale_x = number(0.75f * profile_scale);
    animation->widget.frame->fields.scale_y = animation->widget.frame->fields.scale_x;
    animation->widget.frame->fields.visible = boolean(false);
    animation->widget.frame->fields.active = boolean(true);
    animation->looping = boolean(false);
    animation->speed = number(2);
    static const char *const frames[] = {"ELECTRIC_BUGALOO_1", "ELECTRIC_BUGALOO_2",
                                         "ELECTRIC_BUGALOO_3", "ELECTRIC_BUGALOO_4",
                                         "ELECTRIC_BUGALOO_5", "ELECTRIC_BUGALOO_6"};
    for (size_t index = 0; index < 6; ++index)
        controls->electricity_fields[index] = (AbDataEntry){
            {.kind = AB_DATA_NUMBER, .bits = number((float)(index + 1)).number_bits},
            key(frames[index])};
    controls->electricity_frames =
        (AbDataTable){controls->electricity_fields, 6, 6, NULL};
    if (!ab_ui_animated_image_frames(animation, &controls->electricity_frames, error) ||
        !ab_ui_widget_add(&power->button, &animation->widget, error))
        return false;
    power->present = true;
    controls->blackbird_present = true;
    return true;
}

static bool rmf(AbUiGameHudControls *controls, AbError *error) {
    const AbClassicLevelRequest *level = controls->host.level;
    AbDataValue episode = field(level->episode, "rmf_button_enabled");
    AbDataValue page = field(level->page, "rmf_button_enabled");
    bool allowed = !literal_false(episode) && !literal_false(page);
    AbDataValue forced = truthy(episode) ? episode : page;
    if (!allowed)
        return true;
    bool active = false;
    if (!mattel(controls, &active, error))
        return false;
    if (active || !truthy(field(controls->host.globals, "g_powerups_enabled")))
        return true;
    AbUiPointerValue purchased;
    if (!ab_ui_game_hud_query(controls, AB_UI_HUD_RMF_PURCHASED, &purchased, error))
        return false;
    if (!ab_ui_pointer_value_truthy(&purchased) && !truthy(forced))
        return true;
    if (!button(controls, &controls->rmf, (AbUiWidgetChildren){0},
                "redsMightyFeathersButton", NULL, error))
        return false;
    controls->rmf.state.return_value = text("RMF_BUTTON_CLICKED");
    if (!image(&controls->rmf, "BUTTON_REDSUITS", "BUTTON_REDSUITS_DISABLED", error))
        return false;
    controls->rmf.state.activate_on_release = boolean(true);
    controls->rmf.state.image.alpha = number(0.7f);
    if (!ab_ui_game_hud_query(controls, AB_UI_HUD_RMF_VISIBLE,
                              &controls->rmf.frame->fields.visible, error))
        return false;
    controls->rmf_fields[0] =
        (AbDataEntry){key("settingsName"), key("redsmightyfeathers")};
    controls->rmf_fields[1] = (AbDataEntry){key("name"), key("redsmightyfeathers")};
    controls->rmf_meta = (AbDataTable){controls->rmf_fields, 2, 2, NULL};
    controls->rmf.state.meta = (AbUiPointerValue){
        .kind = AB_UI_POINTER_OBJECT, .object = (uintptr_t)&controls->rmf_meta};
    if (!ab_ui_scaling_set_to(controls->host.powerups.slider.scaling,
                              &controls->rmf.frame->fields, error) ||
        !add_hud(controls, &controls->rmf, error))
        return false;
    controls->rmf_present = true;
    return true;
}

static bool tutorials_show(void *context, AbError *error) {
    AbUiGameHudControls *controls = context;
    (void)error;
    controls->can_show_tutorials = true;
    return true;
}

static bool tutorials_done(void *context, AbDataValue *output, AbError *error) {
    AbUiGameHudControls *controls = context;
    if (!output)
        return ab_fail(error, "Missing GameHud tutorial completion output");
    bool done = false;
    if (field(controls->host.globals, "g_tutorialActive").kind == AB_DATA_NIL) {
        AbDataValue pending = field(controls->host.globals, "blockTutorialPopups");
        if (pending.kind == AB_DATA_NIL)
            done = true;
        else if (pending.kind == AB_DATA_STRING) {
            if (pending.string.size && !pending.string.data)
                return ab_fail(error, "Invalid GameHud tutorial string");
            done = pending.string.size == 0;
        } else if (pending.kind == AB_DATA_TABLE && pending.table) {
            AbDataValue first = {.kind = AB_DATA_NUMBER, .bits = number(1).number_bits};
            const AbDataValue *value = ab_data_get(pending.table, first);
            done = !value || value->kind == AB_DATA_NIL;
            if (done) {
                for (size_t index = 0; index < pending.table->count; ++index) {
                    AbDataEntry row = pending.table->entries[index];
                    if (row.key.kind != AB_DATA_NUMBER || row.value.kind == AB_DATA_NIL)
                        continue;
                    float index_value;
                    if (!numeric(row.key, &index_value, error))
                        return false;
                    if (index_value >= 1 && truncf(index_value) == index_value)
                        return ab_fail(error,
                                       "Sparse GameHud tutorial list unsupported");
                }
            }
        } else
            return ab_fail(error, "GameHud tutorial completion indexes invalid list");
    }
    *output = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = done ? 1 : 0};
    return true;
}

static bool finish_graph(AbUiGameHudControls *controls, AbError *error) {
    if (!ab_ui_pause_page_init(&controls->pause_page, controls->widget.context,
                               controls->host.pause, error) ||
        !ab_ui_widget_add(&controls->widget, &controls->pause_page.widget, error))
        return false;
    if (truthy(field(controls->host.globals, "g_isAccelerometerControl"))) {
        controls->pause.widget.state.hover_enabled = boolean(false);
        if (!controls->restart_present)
            return ab_fail(error, "GameHud accelerometer path has no restart control");
        controls->restart.widget.state.hover_enabled = boolean(false);
    }
    AbUiPointerValue ending;
    if (!eagle_query(controls, &ending, error))
        return false;
    controls->show_skip_button = ending;
    if (ab_ui_pointer_value_truthy(&ending))
        controls->show_skip_button =
            boolean(!truthy(field(controls->host.globals, "eagleBaitLaunched")));
    controls->skip.widget.frame->fields.visible = controls->show_skip_button;
    controls->can_show_tutorials = false;
    bool active = false;
    if (!mattel(controls, &active, error))
        return false;
    if (!active) {
        if (!controls->host.tutorials_requested)
            return ab_fail(error, "Missing canonical GameHud tutorial request flag");
        if (truthy(*controls->host.tutorials_requested))
            controls->can_show_tutorials = true;
        else {
            *controls->host.tutorials_requested =
                (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
            if (!controls->host.flow)
                return ab_fail(error, "Missing actual GameHud LevelStartScreenFlow");
            controls->tutorials =
                (AbClassicLevelFlowListener){.context = controls,
                                             .on_show = tutorials_show,
                                             .is_done = tutorials_done};
            if (!ab_classic_level_flow_push(controls->host.flow, text("tutorials").text,
                                            30, &controls->tutorials, error))
                return false;
        }
    }
    controls->allow_pause = true;
    return true;
}

bool ab_ui_game_hud_controls_init(AbUiGameHudControls *controls,
                                  AbUiWidgetContext *context,
                                  const AbUiFrameFields *arguments,
                                  AbUiPointerValue returning_to_game,
                                  AbUiGameHudControlsHost host, AbError *error) {
    if (!controls || controls->initialized || !context || !host.globals ||
        !host.level || !host.level->episode || !host.level->page ||
        !host.level->level || host.eagle.globals != host.globals ||
        !ab_ui_pointer_value_valid(&returning_to_game) ||
        !ab_ui_pointer_value_valid(&host.restart_control))
        return ab_fail(error, "Invalid actual GameHud constructor bindings");
    AbDataValue model = field(host.globals, "deviceModel");
    if (model.kind != AB_DATA_STRING || !named(model.string, "android"))
        return ab_fail(error, "GameHud constructor requires the pinned Android model");
    controls->host = host;
    if (!ab_ui_widget_init(&controls->widget, context, AB_UI_WIDGET_FRAME,
                           (AbUiWidgetChildren){controls->frames, controls->draw,
                                                controls->pointer,
                                                AB_UI_GAME_HUD_CHILDREN},
                           arguments, NULL, NULL, error))
        return false;
    controls->widget.frame->fields.name = text("gameHud");
    controls->hud_button_count = 0;
    controls->text_group = text("TEXTS_BASIC");
    controls->text_id = text("MI_SCORE");
    bool okay = restart(controls, error) && ads(controls, error);
    if (okay) {
        controls->frame = 0;
        okay = skip(controls, error) && powerups(controls, error) &&
               score_indicator(controls, error) &&
               banner(controls, returning_to_game, error) && eagle(controls, error) &&
               pause_button(controls, error) && blackbird(controls, error) &&
               rmf(controls, error) && finish_graph(controls, error);
    }
    if (!okay)
        context->failed = true;
    controls->initialized = okay;
    return okay;
}
