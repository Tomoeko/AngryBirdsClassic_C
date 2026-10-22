#include "ab/ui_mighty_eagle_button.h"

#include "ab/script_math.h"
#include "ab/script_time.h"
#include "ab/time_format.h"
#include "ab/ui_image_button_animation.h"

#include <math.h>
#include <string.h>

static const char *const event_names[] = {
    "EID_MIGHTY_EAGLE_AVAILABLE",  "EID_LEVEL_GOALS_CLEARED",
    "EID_LEVEL_COMPLETED",         "EID_LEVEL_COMPLETE_INIT",
    "EID_FREE_EAGLE_ENABLED",      "EID_FREE_EAGLE_DISABLED",
    "EID_POWERUP_ACTIVATION_DONE", "EID_POWERUP_ACTIVATION_STARTED"};

static AbUiPointerValue text(const char *value) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                              .text = {(const uint8_t *)value, strlen(value)}};
}

static AbUiPointerValue boolean(bool value) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = value};
}

static AbUiPointerValue number(float value) {
    AbUiPointerValue output = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&output.number_bits, &value, sizeof(value));
    return output;
}

static bool literal_true(AbUiPointerValue value) {
    return value.kind == AB_UI_POINTER_BOOLEAN && value.boolean;
}

static bool named(AbUiPointerValue value, const char *name) {
    return value.kind == AB_UI_POINTER_TEXT && value.text.size == strlen(name) &&
           value.text.data && !memcmp(value.text.data, name, value.text.size);
}

static bool numeric(AbUiPointerValue value, float *output, AbError *error) {
    if (value.kind == AB_UI_POINTER_NUMBER) {
        memcpy(output, &value.number_bits, sizeof(*output));
        if (isfinite(*output))
            return true;
    }
    return ab_fail(error, "MightyEagleButton requires a finite numeric field");
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    AbDataValue key = {.kind = AB_DATA_STRING, .string = text(name).text};
    const AbDataValue *value = ab_data_get(table, key);
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool same(AbDataValue left, AbDataValue right) {
    AbDataEntry entry = {left, {.kind = AB_DATA_BOOLEAN}};
    AbDataTable table = {&entry, 1, 1, NULL};
    return ab_data_get(&table, right) != NULL;
}

static bool finish(AbUiMightyEagleButton *button, bool okay) {
    if (!okay)
        button->widget.context->failed = true;
    return okay;
}

static bool healthy(AbUiMightyEagleButton *button, AbError *error) {
    return (button && button->initialized && button->widget.context &&
            !button->widget.context->failed) ||
           ab_fail(error, "Invalid or stopped MightyEagleButton");
}

static bool query(AbUiMightyEagleButton *button, AbUiEagleQuery kind,
                  AbUiPointerValue *output, AbError *error) {
    if (!button->host.query)
        return ab_fail(error, "Missing actual MightyEagleButton query provider");
    if (!button->host.query(button->host.context, kind, output, error))
        return false;
    return ab_ui_pointer_value_valid(output) ||
           ab_fail(error, "Invalid actual MightyEagleButton query result");
}

static bool enable(AbUiMightyEagleButton *button, AbUiPointerValue value,
                   AbError *error) {
    return ab_ui_image_button_state_enable(&button->widget.state, value,
                                           &button->widget.context->images, error);
}

static bool visible(AbUiMightyEagleButton *button, AbUiPointerValue value,
                    AbError *error) {
    AbUiPointerValue result;
    if (!query(button, AB_UI_EAGLE_SHOW_ELEMENTS, &result, error))
        return false;
    bool allowed = false;
    if (literal_true(result)) {
        if (!query(button, AB_UI_EAGLE_LEVEL_DISABLED, &result, error))
            return false;
        if (!ab_ui_pointer_value_truthy(&result) &&
            !truthy(field(button->host.globals, "eagleBaitLaunched")) &&
            button->state != AB_UI_EAGLE_DISAPPEAR) {
            if (!query(button, AB_UI_EAGLE_LEVEL_ENDING, &result, error))
                return false;
            allowed = !ab_ui_pointer_value_truthy(&result) ||
                      button->state == AB_UI_EAGLE_BOUNCE;
        }
    }
    button->want_visible = value;
    button->widget.frame->fields.visible = boolean(allowed && literal_true(value));
    return true;
}

bool ab_ui_mighty_eagle_button_visible(AbUiMightyEagleButton *button,
                                       AbUiPointerValue value, AbError *error) {
    if (!healthy(button, error))
        return false;
    return finish(button, ab_ui_pointer_value_valid(&value)
                              ? visible(button, value, error)
                              : ab_fail(error, "Invalid Eagle visibility argument"));
}

static bool subscribe(AbUiMightyEagleButton *button, size_t index, bool add,
                      AbError *error) {
    AbDataValue id = field(button->host.event_ids, event_names[index]);
    if (!button->host.events || id.kind == AB_DATA_NIL)
        return ab_fail(error, "Missing actual MightyEagleButton event binding");
    bool okay =
        add ? ab_event_manager_add(button->host.events, id, &button->listener, error)
            : ab_event_manager_remove(button->host.events, id, &button->listener,
                                      error);
    if (okay)
        button->subscriptions[index] = add;
    return okay;
}

bool ab_ui_mighty_eagle_button_detach(AbUiMightyEagleButton *button, AbError *error) {
    if (!button || button->widget.drawing || button->widget.pointing ||
        button->widget.updating)
        return ab_fail(error, "Missing or active MightyEagleButton lifetime owner");
    for (size_t index = 0; index < 8; ++index)
        if (button->subscriptions[index] && !subscribe(button, index, false, error))
            return false;
    return true;
}

static bool enter(AbUiMightyEagleButton *button, AbError *error) {
    AbUiPointerValue enabled = boolean(true);
    if (!truthy(field(button->host.globals, "startedFromEditor"))) {
        for (size_t index = 0; index < 8; ++index)
            if (!subscribe(button, index, true, error))
                return false;
        if (!query(button, AB_UI_EAGLE_AVAILABLE, &enabled, error))
            return false;
    }
    if (!enable(button, enabled, error) ||
        !visible(button, button->want_visible, error))
        return false;
    AbUiPointerValue locked;
    if (!query(button, AB_UI_EAGLE_CHILDLOCK, &locked, error))
        return false;
    if (ab_ui_pointer_value_truthy(&locked)) {
        if (!query(button, AB_UI_EAGLE_SETTINGS_ENABLED, &enabled, error))
            return false;
        if (!ab_ui_pointer_value_truthy(&enabled)) {
            if (!ab_ui_image_button_state_set_image(
                    &button->widget.state, text("BUTTON_USE_EAGLE_DISABLED"),
                    (AbUiPointerValue){0}, &button->widget.context->images, error))
                return false;
            button->widget.state.return_value = text("TOGGLE_POWERUPS_DISABLED");
        }
    }
    if (!ab_ui_widget_lifecycle_base(&button->widget, AB_UI_FRAME_ENTRY, error))
        return false;
    return !truthy(field(button->host.globals, "levelCompleted")) ||
           visible(button, boolean(false), error);
}

static bool layout(AbUiMightyEagleButton *button, AbError *error) {
    if (!ab_ui_widget_lifecycle_base(&button->widget, AB_UI_FRAME_LAYOUT, error))
        return false;
    float width, height, x, scale_x, scale_y;
    AbVec2 bounds;
    if (!ab_ui_text_widget_get_width(&button->timer, &width, error) ||
        !ab_ui_text_widget_get_height(&button->timer, &height, error))
        return false;
    AbUiPointerValue image = button->widget.state.image.image;
    if (image.kind != AB_UI_POINTER_TEXT ||
        !ab_ui_scaling_sprite(button->host.scaling, &button->widget.context->images,
                              image.text, false, &bounds, error))
        return ab_fail(error, "MightyEagleButton layout requires its scaled sprite");
    AbUiFrameFields *timer = &button->timer.widget.frame->fields;
    timer->x = timer->y = number(0);
    if (!numeric(button->widget.frame->fields.x, &x, error) ||
        !numeric(timer->scale_x, &scale_x, error))
        return false;
    /* The initial empty label deliberately divides by zero here. IEEE Inf/NaN
     * simply fail the two source comparisons; no finite fallback is applied. */
    float ratio = x / ((width / 2) * scale_x);
    if (ratio < 1 && ratio > 0) {
        timer->scale_x = number(scale_x * ratio);
        if (!numeric(timer->scale_y, &scale_y, error))
            return false;
        timer->scale_y = number(scale_y * ratio);
    }
    return true;
}

static bool lifecycle(void *context, AbUiWidget *widget, AbUiFrameMethod method,
                      AbError *error) {
    AbUiMightyEagleButton *button = context;
    if (method == AB_UI_FRAME_ENTRY)
        return enter(button, error);
    if (method == AB_UI_FRAME_LAYOUT)
        return layout(button, error);
    if (method == AB_UI_FRAME_EXIT &&
        !truthy(field(button->host.globals, "startedFromEditor"))) {
        for (size_t index = 0; index < 8; ++index) {
            /* The original onExit omits its LEVEL_COMPLETED registration. */
            if (index != 2 && !subscribe(button, index, false, error))
                return false;
        }
    }
    return ab_ui_widget_lifecycle_base(widget, method, error);
}

static bool event_triggered(void *context, AbDataTable *notice, AbError *error) {
    return ab_ui_mighty_eagle_button_event(context, notice, error);
}

static bool handle_event(AbUiMightyEagleButton *button, AbDataTable *notice,
                         AbError *error) {
    if (!notice)
        return ab_fail(error, "Missing MightyEagleButton event");
    AbDataValue id = field(notice, "id");
    size_t index;
    for (index = 0; index < 8; ++index)
        if (same(id, field(button->host.event_ids, event_names[index])))
            break;
    AbUiPointerValue result;
    switch (index) {
        case 0:
            if (!enable(button, boolean(true), error) ||
                !query(button, AB_UI_EAGLE_LEVEL_ENDING, &result, error))
                return false;
            if (ab_ui_pointer_value_truthy(&result))
                return true;
            if (!query(button, AB_UI_EAGLE_ENABLED, &result, error))
                return false;
            if (!ab_ui_pointer_value_truthy(&result))
                return true;
            if (!button->widget.context->play_audio)
                return ab_fail(error, "Missing actual Mighty Eagle audio provider");
            return button->widget.context->play_audio(
                button->widget.context->audio_context, text("mighty_eagle_fly"), 1,
                false, error);
        case 1:
            if (ab_ui_pointer_value_truthy(&button->widget.state.enabled)) {
                button->scale_timer = number(0);
                button->state = AB_UI_EAGLE_BOUNCE;
            }
            if (!query(button, AB_UI_EAGLE_SUBSYSTEM_ENABLED, &result, error))
                return false;
            if (!ab_ui_pointer_value_truthy(&result))
                button->state = AB_UI_EAGLE_DISAPPEAR;
            return true;
        case 3:
            button->state = AB_UI_EAGLE_DISAPPEAR;
            button->scale_timer = number(1);
            if (!query(button, AB_UI_EAGLE_SUBSYSTEM_ENABLED, &result, error))
                return false;
            return ab_ui_pointer_value_truthy(&result) ||
                   visible(button, boolean(false), error);
        case 4:
        case 5:
            return visible(button, button->want_visible, error);
        case 6:
        case 7:
            return query(button, AB_UI_EAGLE_AVAILABLE, &result, error) &&
                   enable(button, result, error);
        default:
            return true;
    }
}

bool ab_ui_mighty_eagle_button_event(AbUiMightyEagleButton *button, AbDataTable *notice,
                                     AbError *error) {
    if (!healthy(button, error))
        return false;
    return finish(button, handle_event(button, notice, error));
}

static bool pointer(void *context, AbUiWidget *widget, AbSpan event, AbVec2 point,
                    AbUiPointerReply *output, AbError *error) {
    AbUiMightyEagleButton *button = context;
    AbUiPointerReply reply;
    if (!ab_ui_widget_pointer_base(widget, event, point, &reply, error))
        return false;
    if (named(reply.values[0], "ME_BUTTON_CLICKED_DISABLED")) {
        button->info_timer = number(3);
        *output = (AbUiPointerReply){.values = {number(-1)}};
    } else if (button->state == AB_UI_EAGLE_DISAPPEAR)
        *output = (AbUiPointerReply){0};
    else {
        reply.values[2] = (AbUiPointerValue){0};
        *output = reply;
    }
    return true;
}

static bool draw(void *context, AbUiWidget *widget, const AbUiDrawInput *input,
                 AbError *error) {
    (void)context;
    if (!ab_ui_pointer_value_truthy(&widget->frame->fields.visible))
        return true;
    if (!input)
        return ab_fail(error, "Missing MightyEagleButton parent draw arguments");
    AbUiImageButtonAnimation animation = {
        .hover_enabled = ab_ui_pointer_value_truthy(&widget->state.hover_enabled)};
    if (animation.hover_enabled &&
        !numeric(widget->state.hover_scale, &animation.hover_scale, error))
        return false;
    AbUiDrawInput parent = *input;
    parent.present = (input->present & 3) | 60;
    parent.transform.scale = (AbVec2){1, 1};
    parent.transform.angle = 0;
    if (!ab_ui_image_button_hover_scale(&animation, &parent.transform.local_factor))
        return ab_fail(error, "Invalid Mighty Eagle hover scale");
    return ab_ui_widget_draw_base(widget, &parent, error);
}

static bool calendar(AbDataValue value, AbCalendarTime *output, AbError *error) {
    if (value.kind != AB_DATA_TABLE || !value.table)
        return ab_fail(error, "Mighty Eagle used time requires an actual date table");
    const char *const names[] = {"day", "hour", "minutes", "seconds"};
    float *const fields[] = {&output->day, &output->hour, &output->minutes,
                             &output->seconds};
    for (size_t index = 0; index < 4; ++index) {
        AbDataValue item = field(value.table, names[index]);
        if (item.kind != AB_DATA_NUMBER ||
            !numeric((AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER,
                                        .number_bits = item.bits},
                     fields[index], error))
            return ab_fail(error, "Invalid reached Mighty Eagle date field");
    }
    return true;
}

static bool update_timer(AbUiMightyEagleButton *button, float seconds, AbDataValue used,
                         AbError *error) {
    if (button->info_timer.kind == AB_UI_POINTER_NONE ||
        field(button->host.globals, "eagleLockedTime").kind == AB_DATA_NIL ||
        used.kind == AB_DATA_NIL) {
        button->timer.widget.frame->fields.visible = boolean(false);
        return true;
    }
    button->timer.widget.frame->fields.visible = boolean(true);
    AbDataValue locked_value = field(button->host.globals, "eagleLockedTime");
    AbCalendarTime now, prior;
    float locked, elapsed;
    if (!button->host.clock.now)
        return ab_fail(error, "Missing actual Mighty Eagle calendar provider");
    if (!button->host.clock.now(button->host.clock.context, &now, error) ||
        !calendar(used, &prior, error) ||
        !ab_script_time_difference(&now, &prior, &elapsed, error))
        return false;
    if (locked_value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Mighty Eagle cooldown requires numeric locked time");
    if (!numeric((AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER,
                                    .number_bits = locked_value.bits},
                 &locked, error))
        return false;
    char formatted[64];
    if (!ab_time_format(locked - elapsed, formatted, sizeof(formatted), error))
        return false;
    if (!named(button->timer.state.fields.text, formatted)) {
        memcpy(button->timer_text, formatted, strlen(formatted) + 1);
        button->timer.state.fields.text = text(button->timer_text);
        if (!layout(button, error) ||
            !ab_ui_text_widget_clip(&button->timer, &button->clipping,
                                    (AbUiPointerValue){0}, (AbUiPointerValue){0},
                                    error))
            return false;
    }
    float timer;
    if (!numeric(button->info_timer, &timer, error))
        return false;
    timer -= seconds;
    button->info_timer = timer < 0 ? (AbUiPointerValue){0} : number(timer);
    return true;
}

static bool update_scale(AbUiMightyEagleButton *button, float seconds, AbError *error) {
    if (button->state == AB_UI_EAGLE_STILL ||
        button->scale_timer.kind == AB_UI_POINTER_NONE)
        return true;
    float timer, scale;
    if (!numeric(button->scale_timer, &timer, error))
        return false;
    if (button->state == AB_UI_EAGLE_BOUNCE) {
        timer += seconds;
        button->scale_timer = number(timer);
        float angle = (0x1.921fb6p+1f * timer) * 2;
        float cosine;
        if (!ab_script_cosine(angle, &cosine))
            return ab_fail(error, "Mighty Eagle bounce exceeds script cosine domain");
        scale = 0.75f + cosine * 0.25f;
    } else if (button->state == AB_UI_EAGLE_DISAPPEAR) {
        timer -= seconds * 3;
        button->scale_timer = number(timer);
        scale = timer > 0 ? timer : 0;
    } else
        return ab_fail(error, "Unsupported Mighty Eagle scaling state");
    float scaling;
    if (!ab_ui_scaling_get(button->host.scaling, &scaling, error))
        return false;
    button->widget.frame->fields.scale_x = number(scale * scaling);
    button->widget.frame->fields.scale_y = button->widget.frame->fields.scale_x;
    if (button->state == AB_UI_EAGLE_DISAPPEAR && timer < 0) {
        button->scale_timer = (AbUiPointerValue){0};
        button->state = AB_UI_EAGLE_STILL;
        return visible(button, boolean(false), error);
    }
    return true;
}

static bool update(void *context, AbUiWidget *widget, float seconds, float time,
                   AbError *error) {
    AbUiMightyEagleButton *button = context;
    if (!ab_ui_pointer_value_truthy(&widget->frame->fields.visible))
        return true;
    if (!ab_ui_widget_update_base(widget, seconds, time, error))
        return false;
    AbUiPointerValue free_enabled;
    AbDataValue used;
    if (!query(button, AB_UI_EAGLE_FREE_ENABLED, &free_enabled, error))
        return false;
    if (!button->host.used_time)
        return ab_fail(error, "Missing actual Mighty Eagle used-time provider");
    return button->host.used_time(button->host.context, free_enabled, &used, error) &&
           update_timer(button, seconds, used, error) &&
           update_scale(button, seconds, error);
}

bool ab_ui_mighty_eagle_button_init(AbUiMightyEagleButton *button,
                                    AbUiWidgetContext *context,
                                    const AbUiImageButtonArguments *arguments,
                                    AbUiPointerValue from, AbUiMightyEagleHost host,
                                    AbError *error) {
    if (!button || !context || !host.globals)
        return ab_fail(error, "Missing canonical MightyEagleButton bindings");
    button->host = host;
    AbUiWidgetChildren children = {button->frames, button->draw, button->pointer, 1};
    if (!ab_ui_widget_init(&button->widget, context, AB_UI_WIDGET_IMAGE_BUTTON,
                           children, NULL, NULL, arguments, error))
        return false;
    const AbDataTable *power = NULL;
    if (!host.episode_power)
        return finish(button, ab_fail(error, "Missing actual episode-power lookup"));
    if (!host.episode_power(host.context, &power, error))
        return finish(button, false);
    const char *const fields[] = {"buttonEnabled", "buttonDisabled"};
    AbUiPointerValue images[] = {text("INGAME_BUTTON_EAGLE"),
                                 text("BUTTON_USE_EAGLE_DISABLED")};
    for (size_t index = 0; index < 2; ++index) {
        AbDataValue value = field(power, fields[index]);
        if (value.kind == AB_DATA_NIL || (value.kind == AB_DATA_BOOLEAN && !value.bits))
            continue;
        if (value.kind != AB_DATA_STRING)
            return finish(button, ab_fail(error, "Unsupported Eagle image override"));
        images[index] =
            (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = value.string};
    }
    if (!ab_ui_image_button_state_set_image(&button->widget.state, images[0], images[1],
                                            &context->images, error))
        return finish(button, false);
    if (host.basic_font_getter.kind != AB_UI_POINTER_FUNCTION)
        return finish(button, ab_fail(error, "Missing actual getFontBasic binding"));
    AbUiFrameFields frame = {.name = text("timerText")};
    AbUiTextArguments label = {.text = text(""), .font = host.basic_font_getter};
    if (!ab_ui_text_widget_init(&button->timer, context, (AbUiWidgetChildren){0},
                                host.fonts, &frame, &label, error))
        return finish(button, false);
    button->timer.state.fields.vertical_anchor = text("TOP");
    if (!ab_ui_widget_add(&button->widget, &button->timer.widget, error))
        return finish(button, false);
    button->timer_hancor = text("HCENTER");
    button->timer.state.fields.vertical_anchor = text("VCENTER");
    button->result = text("ME_BUTTON_CLICKED");
    button->from = ab_ui_pointer_value_truthy(&from) ? from : text("INGAME");
    button->widget.state.disabled_return_value = text("ME_BUTTON_CLICKED_DISABLED");
    button->widget.state.return_value = text("ME_BUTTON_CLICKED");
    button->state = AB_UI_EAGLE_STILL;
    button->want_visible = boolean(true);
    if (!ab_ui_text_clipping_init(&button->clipping, host.timer_storage, error))
        return finish(button, false);
    button->listener = (AbEventListener){.name = button->widget.frame->fields.name.text,
                                         .context = button,
                                         .event_triggered = event_triggered};
    button->widget.methods = (AbUiWidgetMethods){.context = button,
                                                 .lifecycle = lifecycle,
                                                 .draw = draw,
                                                 .pointer = pointer,
                                                 .update = update};
    button->initialized = true;
    return true;
}
