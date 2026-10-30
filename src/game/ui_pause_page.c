#include "ab/ui_pause_page.h"

#include "ab/fill_rectangle.h"
#include "ab/game_rectangle.h"

#include <math.h>
#include <string.h>

static bool page_lifecycle(void *, AbUiWidget *, AbUiFrameMethod, AbError *);
static bool page_update(void *, AbUiWidget *, float, float, AbError *);
static bool page_draw(void *, AbUiWidget *, const AbUiDrawInput *, AbError *);
static bool page_pointer(void *, AbUiWidget *, AbSpan, AbVec2, AbUiPointerReply *,
                         AbError *);
static bool middle_lifecycle(void *, AbUiWidget *, AbUiFrameMethod, AbError *);
static bool event(void *, AbDataTable *, AbError *);

static const char *const subscriptions[] = {
    "EID_AUDIO_STATE_CHANGED", "EID_CHANNEL_CONNECTING_STARTED",
    "EID_CHANNEL_LOADING_FAILED", "EID_CHANNEL_CANCELLED", "EID_CHANNEL_CLOSED"};

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

static bool same(AbDataValue left, AbDataValue right) {
    AbDataEntry entry = {left, {.kind = AB_DATA_BOOLEAN}};
    AbDataTable table = {&entry, 1, 1, NULL};
    return ab_data_get(&table, right) != NULL;
}

static bool named(AbSpan value, const char *name) {
    return value.size == strlen(name) &&
           (!value.size || (value.data && !memcmp(value.data, name, value.size)));
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
    return ab_fail(error, "PausePage requires finite numeric geometry");
}

static bool healthy(AbUiPausePage *page, AbError *error) {
    return (page && page->initialized && page->widget.initialized &&
            page->widget.context && !page->widget.context->failed) ||
           ab_fail(error, "Invalid or stopped PausePage");
}

static bool finish(AbUiPausePage *page, bool okay) {
    if (!okay)
        page->widget.context->failed = true;
    return okay;
}

static bool notify(AbUiPausePage *page, const char *name, AbError *error) {
    if (!page->host.events || page->notice_count >= page->host.notice_capacity)
        return ab_fail(error, "PausePage notice owner/storage is unavailable");
    AbDataValue id = field(page->host.event_ids, name);
    if (id.kind == AB_DATA_NIL)
        return ab_fail(error, "Missing actual PausePage event ID");
    AbUiPauseNotice *notice = page->host.notices + page->notice_count++;
    notice->id = (AbDataEntry){key("id"), id};
    notice->table = (AbDataTable){&notice->id, 1, 1, NULL};
    return ab_event_manager_notify(page->host.events, &notice->table, error);
}

static bool frame(AbUiPausePage *page, AbUiWidget *widget, const char *name,
                  AbUiWidgetChildren children, AbError *error) {
    AbUiFrameFields fields = {.name = text(name)};
    return ab_ui_widget_init(widget, page->widget.context, AB_UI_WIDGET_FRAME, children,
                             &fields, NULL, NULL, error);
}

static bool control(AbUiPausePage *page, AbUiPauseControl role, const char *name,
                    const char *image, const char *disabled, const char *action,
                    AbUiWidget *parent, AbError *error) {
    AbUiWidget *widget = page->controls + role;
    AbUiImageButtonArguments args = {
        .image.frame.name = text(name),
        .return_value = text(action),
        .activate_on_release = {.kind = AB_UI_POINTER_BOOLEAN, .boolean = true}};
    bool plain = role == AB_UI_PAUSE_CHECKPOINT || role == AB_UI_PAUSE_SHOP;
    if (!ab_ui_widget_init(widget, page->widget.context, AB_UI_WIDGET_IMAGE_BUTTON,
                           (AbUiWidgetChildren){0}, NULL, NULL, plain ? NULL : &args,
                           error))
        return false;
    if (plain) {
        widget->frame->fields.name = text(name);
        if (role == AB_UI_PAUSE_CHECKPOINT)
            widget->state.return_value = text(action);
    }
    if (!ab_ui_image_button_state_set_image(&widget->state, text(image),
                                            disabled ? text(disabled)
                                                     : (AbUiPointerValue){0},
                                            &widget->context->images, error))
        return false;
    if (role == AB_UI_PAUSE_CHECKPOINT)
        widget->state.activate_on_release = boolean(true);
    if (role == AB_UI_PAUSE_SHOP) {
        widget->state.return_value = text(action);
        widget->state.disabled_return_value = text("GOTO_SHOP_DISABLED");
    }
    widget->state.click_sound =
        text(role == AB_UI_PAUSE_RESUME ? "menu_back" : "menu_confirm");
    if (role == AB_UI_PAUSE_SHOP)
        widget->state.activate_on_release = boolean(true);
    if (role == AB_UI_PAUSE_TUTORIALS)
        widget->frame->fields.visible = boolean(!page->host.environment->mattel_active);
    return ab_ui_widget_add(parent, widget, error);
}

static bool shop_visible(AbUiPausePage *page) {
    const AbUiPausePageEnvironment *source = page->host.environment;
    return source->powerups_enabled && source->purchase_elements &&
           !source->hide_powerup_button && !source->trial;
}

static bool toons_visible(AbUiPausePage *page) {
    const AbUiPausePageEnvironment *source = page->host.environment;
    return source->channel_enabled && source->channel_available;
}

static bool constructor(AbUiPausePage *page, AbError *error) {
    AbUiWidgetContext *context = page->widget.context;
    const AbUiPausePageEnvironment *source = page->host.environment;
    float time = 0;
    if (truthy(*page->host.time)) {
        if (page->host.time->kind != AB_DATA_NUMBER)
            return ab_fail(error, "PausePage timer has nonnumeric source time");
        memcpy(&time, &page->host.time->bits, sizeof(time));
    }
    if (!ab_event_timer_init(&page->timer, page->host.timers, page->host.timer_capacity,
                             time, error))
        return false;
    page->widget.frame->fields.pause_page_show = boolean(false);
    page->extend_state = 0;
    page->extend_speed = 5;
    page->widget.frame->fields.visible = boolean(false);
    if (!frame(page, &page->dummy, "dummyFrame",
               (AbUiWidgetChildren){page->dummy_frames, page->dummy_draw,
                                    page->dummy_pointer, 5},
               error) ||
        !ab_ui_widget_add(&page->widget, &page->dummy, error) ||
        !control(page, AB_UI_PAUSE_RESUME, "buttonResume", "BUTTON_RESUME", NULL,
                 "BACK_TO_GAME", &page->dummy, error) ||
        !frame(page, &page->middle, "middleButtonsFrame",
               (AbUiWidgetChildren){page->middle_frames, page->middle_draw,
                                    page->middle_pointer, 5},
               error))
        return false;
    page->available_height = page->host.screen->y * 0.5f;
    page->middle.methods =
        (AbUiWidgetMethods){.context = page, .lifecycle = middle_lifecycle};
    if (!ab_ui_widget_add(&page->dummy, &page->middle, error) ||
        !control(page, AB_UI_PAUSE_CHECKPOINT, "restartCheckpointButton",
                 "BUTTON_CHECKPOINT_FLAG", NULL, "RESTART_CHECKPOINT", &page->middle,
                 error) ||
        !control(page, AB_UI_PAUSE_RESTART, "buttonRestart", "BUTTON_RESTART", NULL,
                 "RESTART_LEVEL", &page->middle, error) ||
        !control(page, AB_UI_PAUSE_MENU, "buttonBackToMenu", "BUTTON_MENU", NULL,
                 "BACK_TO_LEVEL_SELECTION", &page->middle, error))
        return false;
    if (source->shop_demo) {
        if (!ab_ui_image_button_state_set_image(
                &page->controls[AB_UI_PAUSE_MENU].state, text("BUTTON_RETURN_SHOP"),
                (AbUiPointerValue){0}, &context->images, error))
            return false;
    } else {
        if (!source->mattel_active) {
            if (!control(page, AB_UI_PAUSE_SHOP, "shop_button", "BUTTON_SHOP",
                         "BUTTON_SHOP_DISABLED", "GOTO_SHOP_FROM_PAUSE", &page->middle,
                         error))
                return false;
            page->shop_present = true;
        }
        if (toons_visible(page)) {
            if (!control(page, AB_UI_PAUSE_TOONS, "buttonToons",
                         "BUTTON_PAUSEMENU_TOONS", NULL, "GOTO_TOONS", &page->middle,
                         error))
                return false;
            page->toons_present = true;
        }
    }
    if (!control(page, AB_UI_PAUSE_TUTORIALS, "buttonTutorials", "MENU_TUTORIALS", NULL,
                 "SHOW_TUTORIALS", &page->dummy, error))
        return false;
    AbUiImageButtonArguments sounds = {
        .image.frame.name = text("buttonSounds"),
        .return_value = text("TOGGLE_SOUNDS"),
        .activate_on_release = {.kind = AB_UI_POINTER_BOOLEAN, .boolean = true}};
    if (!ab_ui_toggle_button_init(&page->sounds, context, (AbUiWidgetChildren){0},
                                  &sounds, error))
        return false;
    page->sound_image_fields[0] = (AbDataEntry){
        {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3f800000)}, key("BUTTON_SFX")};
    page->sound_image_fields[1] = (AbDataEntry){
        {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x40000000)}, key("BUTTON_SFX_OFF")};
    page->sound_images = (AbDataTable){page->sound_image_fields, 2, 2, NULL};
    if (!ab_ui_toggle_button_set_images(&page->sounds, &page->sound_images,
                                        (AbUiPointerValue){0}, error))
        return false;
    page->sounds.widget.state.click_sound = text("menu_confirm");
    if (!ab_ui_widget_add(&page->dummy, &page->sounds.widget, error))
        return false;
    AbUiFrameFields fields = {.name = text("levelNumberText")};
    if (!ab_ui_text_widget_init(&page->number, context, (AbUiWidgetChildren){0},
                                page->host.fonts, &fields, NULL, error) ||
        !ab_ui_text_widget_set_font(&page->number, source->menu_font, error))
        return false;
    page->number.state.fields.box_size = number(page->host.screen->x);
    page->number.state.fields.horizontal_anchor = text("HCENTER");
    page->number.state.fields.vertical_anchor = text("VCENTER");
    return ab_ui_widget_add(&page->dummy, &page->number.widget, error);
}

bool ab_ui_pause_page_init(AbUiPausePage *page, AbUiWidgetContext *context,
                           AbUiPausePageHost host, AbError *error) {
    if (!page || page->initialized || !context || context->failed || !host.scaling ||
        !host.fonts || !host.events || !host.event_ids || !host.screen ||
        !host.margin || !host.environment || !host.time || !host.tutorial_active ||
        !host.current_music || !host.skip_input || !host.notices ||
        !host.notice_capacity)
        return ab_fail(error, "Invalid actual PausePage construction bindings");
    if (!ab_ui_widget_init(
            &page->widget, context, AB_UI_WIDGET_FRAME,
            (AbUiWidgetChildren){page->frames, page->draw, page->pointer, 1}, NULL,
            NULL, NULL, error))
        return false;
    page->widget.frame->fields.name = text("pausePage");
    page->host = host;
    bool okay =
        ab_ui_text_clipping_init(&page->number_clipping, host.label_storage, error) &&
        constructor(page, error);
    if (!okay)
        return finish(page, false);
    page->listener = (AbEventListener){
        .name = text("PausePage").text, .context = page, .event_triggered = event};
    page->widget.methods = (AbUiWidgetMethods){.context = page,
                                               .lifecycle = page_lifecycle,
                                               .draw = page_draw,
                                               .pointer = page_pointer,
                                               .update = page_update};
    page->initialized = true;
    return true;
}

static bool audio_state(AbUiPausePage *page, AbError *error) {
    bool enabled;
    if (!page->host.audio_enabled)
        return ab_fail(error, "Missing actual PausePage audio settings query");
    return page->host.audio_enabled(page->host.context, &enabled, error) &&
           ab_ui_toggle_button_set_state(&page->sounds, enabled ? 1.0f : 2.0f, error);
}

bool ab_ui_pause_page_audio_state(AbUiPausePage *page, AbError *error) {
    return healthy(page, error) && finish(page, audio_state(page, error));
}

static bool background_audio(AbUiPausePage *page, bool muted, AbError *error) {
    if (!page->host.environment->no_audio_on_pause)
        return true;
    bool enabled;
    if (!page->host.audio_enabled)
        return ab_fail(error, "Missing actual PausePage audio settings query");
    if (!page->host.audio_enabled(page->host.context, &enabled, error))
        return false;
    if (!enabled)
        return true;
    AbAudioMixerView view = ab_audio_mixer_view(page->host.mixer);
    if (!view.track_gains || !page->host.ramp)
        return ab_fail(error, "Missing actual PausePage mixer/ramp owner");
    page->host.ramp->music_volume = (AbDataValue){
        .kind = AB_DATA_NUMBER, .bits = number(view.track_gains[7]).number_bits};
    page->host.ramp->music_length = (AbDataValue){
        .kind = AB_DATA_NUMBER, .bits = number(muted ? -0.5f : 0.5f).number_bits};
    return !page->host.egg_audio ||
           page->host.egg_audio(page->host.context, muted, error);
}

static bool subscribe(AbUiPausePage *page, size_t index, bool add, AbError *error) {
    AbDataValue id = field(page->host.event_ids, subscriptions[index]);
    if (id.kind == AB_DATA_NIL)
        return ab_fail(error, "Missing actual PausePage subscription ID");
    bool okay =
        add ? ab_event_manager_add(page->host.events, id, &page->listener, error)
            : ab_event_manager_remove(page->host.events, id, &page->listener, error);
    if (okay)
        page->registered[index] = add;
    return okay;
}

bool ab_ui_pause_page_detach(AbUiPausePage *page, AbError *error) {
    if (!page || page->busy)
        return ab_fail(error, "Missing or active PausePage lifetime owner");
    for (size_t index = 0; index < 5; ++index)
        if (page->registered[index] && !subscribe(page, index, false, error))
            return false;
    return true;
}

static bool event(void *context, AbDataTable *notice, AbError *error) {
    AbUiPausePage *page = context;
    AbDataValue id = field(notice, "id");
    bool okay = true;
    if (same(id, field(page->host.event_ids, subscriptions[0]))) {
        AbDataValue from = field(notice, "from");
        if (from.kind == AB_DATA_STRING && named(from.string, "KEYBOARD_COMMAND"))
            okay = audio_state(page, error);
        if (okay)
            okay = background_audio(page, true, error);
    } else if (same(id, field(page->host.event_ids, subscriptions[1]))) {
        if (!ab_ui_pointer_value_truthy(&page->widget.frame->fields.pause_page_show))
            okay = notify(page, "EID_PAUSE_CLICKED", error);
        if (okay)
            page->block_input = true;
    } else {
        for (size_t index = 2; index < 5; ++index)
            if (same(id, field(page->host.event_ids, subscriptions[index])))
                page->block_input = false;
    }
    return finish(page, okay);
}

bool ab_ui_pause_page_show(AbUiPausePage *page, AbError *error) {
    if (!healthy(page, error))
        return false;
    if (page->busy)
        return ab_fail(error, "Reentrant PausePage show operation");
    page->busy = true;
    bool okay = background_audio(page, true, error);
    if (okay) {
        page->widget.frame->fields.pause_page_show = boolean(true);
        page->widget.frame->fields.visible = boolean(true);
        okay = audio_state(page, error) && notify(page, "EID_PAUSE_PAGE_SHOWN", error);
    }
    if (okay)
        okay = page->host.show_pause_menu
                   ? page->host.show_pause_menu(page->host.context, error)
                   : ab_fail(error, "Missing actual common-game showPauseMenu service");
    if (okay)
        okay = page->host.scaling_enabled
                   ? page->host.scaling_enabled(page->host.context, false, error)
                   : ab_fail(error,
                             "Missing actual common-game setScalingEnabled service");
    page->busy = false;
    return finish(page, okay);
}

bool ab_ui_pause_page_hide(AbUiPausePage *page, AbError *error) {
    if (!healthy(page, error))
        return false;
    if (page->busy)
        return ab_fail(error, "Reentrant PausePage hide operation");
    page->busy = true;
    bool okay = background_audio(page, false, error);
    if (okay) {
        page->widget.frame->fields.pause_page_show = boolean(false);
        *page->host.skip_input = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
        okay = notify(page, "EID_PAUSE_PAGE_CLOSED", error);
    }
    if (okay)
        okay = page->host.scaling_enabled
                   ? page->host.scaling_enabled(page->host.context, true, error)
                   : ab_fail(error,
                             "Missing actual common-game setScalingEnabled service");
    page->busy = false;
    return finish(page, okay);
}

bool ab_ui_pause_page_total_width(AbUiPausePage *page, float *output, AbError *error) {
    if (!page || !page->widget.initialized || !page->widget.context ||
        page->widget.context->failed || !page->host.screen || !page->host.environment ||
        !output)
        return ab_fail(error, "Missing PausePage width bindings");
    AbVec2 screen = *page->host.screen;
    float ratio = page->host.environment->width_ratio;
    float width = ceilf(fmaxf(screen.x, screen.y) * ratio);
    if (!isfinite(screen.x) || !isfinite(screen.y) || screen.x <= 0 || screen.y <= 0 ||
        !isfinite(ratio) || !isfinite(width))
        return ab_fail(error, "Invalid actual PausePage screen dimensions");
    *output = width;
    return true;
}

static bool middle_lifecycle(void *context, AbUiWidget *widget, AbUiFrameMethod method,
                             AbError *error) {
    AbUiPausePage *page = context;
    if (method != AB_UI_FRAME_LAYOUT)
        return ab_ui_widget_lifecycle_base(widget, method, error);
    if (!ab_ui_widget_lifecycle_base(widget, method, error))
        return false;
    float count = 0, height = 0, scale;
    if (!ab_ui_scaling_get(page->host.scaling, &scale, error))
        return false;
    for (size_t index = 0; index < widget->frame->child_count; ++index) {
        AbUiWidget *child = widget->children.frames[index]->context;
        if (!ab_ui_pointer_value_truthy(&child->frame->fields.visible))
            continue;
        float child_height;
        if (!numeric(child->frame->fields.h, &child_height, error))
            return false;
        count = count + 1;
        height = height + child_height * scale;
    }
    float total = page->available_height - height * 1.1f > 1 ? height * 1.1f
                                                             : page->available_height;
    float origin =
        (-page->available_height * 0.5f) + ((page->available_height - total) * 0.5f);
    float ordinal = 0;
    if (!isfinite(total) || !isfinite(origin))
        return ab_fail(error, "MiddleButtonsFrame layout overflow");
    for (size_t index = 0; index < widget->frame->child_count; ++index) {
        AbUiWidget *child = widget->children.frames[index]->context;
        if (!ab_ui_pointer_value_truthy(&child->frame->fields.visible))
            continue;
        float y = origin + (ordinal * (total / count)) * 1.1f;
        if (!isfinite(y))
            return ab_fail(error, "MiddleButtonsFrame child position overflow");
        child->frame->fields.y = number(y);
        child->frame->fields.x = number(0);
        ordinal = ordinal + 1;
        if (!ab_ui_scaling_set_to(page->host.scaling, &child->frame->fields, error))
            return false;
    }
    return true;
}

static bool scaled_size(AbUiPausePage *page, AbUiWidget *widget, AbVec2 *output,
                        AbError *error) {
    AbUiPointerValue image = widget->state.image.image;
    if (image.kind != AB_UI_POINTER_TEXT)
        return ab_fail(error, "Missing ordinary PausePage image");
    return ab_ui_scaling_sprite(page->host.scaling, &widget->context->images,
                                image.text, false, output, error);
}

static bool layout(AbUiPausePage *page, AbError *error) {
    float total = 0, scale;
    if (!ab_ui_pause_page_total_width(page, &total, error))
        return false;
    page->dummy.frame->fields.x = number(0);
    page->dummy.frame->fields.y = number(0);
    AbUiWidget *resume = page->controls + AB_UI_PAUSE_RESUME;
    AbUiWidget *tutorials = page->controls + AB_UI_PAUSE_TUTORIALS;
    AbUiWidget *sounds = &page->sounds.widget;
    AbVec2 resume_size, tutorial_size, sound_size;
    AbVec2 margin = *page->host.margin, screen = *page->host.screen;
    if (!isfinite(margin.x) || !isfinite(margin.y))
        return ab_fail(error, "Invalid actual PausePage margin");
    if (!scaled_size(page, resume, &resume_size, error))
        return false;
    resume->frame->fields.y = number(resume_size.y * 0.5f + margin.y);
    resume->frame->fields.x = number((-total + resume_size.x * 0.5f) + margin.x);
    if (!ab_ui_scaling_set_to(page->host.scaling, &resume->frame->fields, error) ||
        !scaled_size(page, tutorials, &tutorial_size, error))
        return false;
    tutorials->frame->fields.y = number(screen.y - tutorial_size.y * 0.75f);
    tutorials->frame->fields.x = number((-total * 0.5f) + tutorial_size.x * 0.5f);
    if (!ab_ui_scaling_set_to(page->host.scaling, &tutorials->frame->fields, error) ||
        !scaled_size(page, sounds, &sound_size, error))
        return false;
    sounds->frame->fields.y = tutorials->frame->fields.y;
    sounds->frame->fields.x = number((-total * 0.5f) - sound_size.x * 0.5f);
    if (!ab_ui_pointer_value_truthy(&tutorials->frame->fields.visible))
        sounds->frame->fields.x = number(-total * 0.5f);
    if (!ab_ui_scaling_set_to(page->host.scaling, &sounds->frame->fields, error))
        return false;
    for (size_t index = 0; index < page->dummy.frame->child_count; ++index) {
        AbUiWidget *child = page->dummy.children.frames[index]->context;
        if (!ab_ui_pointer_value_truthy(&child->frame->fields.scale_x))
            child->frame->fields.scale_x = number(1);
        if (!ab_ui_pointer_value_truthy(&child->frame->fields.scale_y))
            child->frame->fields.scale_y = number(1);
    }
    if (!page->host.environment->use_scaling &&
        total < tutorial_size.x + sound_size.x) {
        float ratio = total / (tutorial_size.x + sound_size.x);
        float sx, sy;
        if (!numeric(tutorials->frame->fields.scale_x, &sx, error) ||
            !numeric(tutorials->frame->fields.scale_y, &sy, error))
            return false;
        tutorials->frame->fields.scale_x = number(sx * ratio);
        tutorials->frame->fields.scale_y = number(sy * ratio);
        if (!numeric(sounds->frame->fields.scale_x, &sx, error) ||
            !numeric(sounds->frame->fields.scale_y, &sy, error))
            return false;
        sounds->frame->fields.scale_x = number(sx * ratio);
        sounds->frame->fields.scale_y = number(sy * ratio);
        tutorial_size.x = tutorial_size.x * ratio;
        tutorial_size.y = tutorial_size.y * ratio;
        tutorials->frame->fields.y = number(screen.y - tutorial_size.y * 0.75f);
        tutorials->frame->fields.x = number((-total * 0.5f) + tutorial_size.x * 0.5f);
        sound_size.x = sound_size.x * ratio;
        sound_size.y = sound_size.y * ratio;
        sounds->frame->fields.y = tutorials->frame->fields.y;
        sounds->frame->fields.x = number((-total * 0.5f) - sound_size.x * 0.5f);
    }
    float resume_x, resume_y, resume_width, resume_height;
    if (!numeric(resume->frame->fields.x, &resume_x, error) ||
        !ab_ui_scaling_get(page->host.scaling, &scale, error) ||
        !numeric(resume->frame->fields.w, &resume_width, error))
        return false;
    float remaining = -(resume_x + (scale * resume_width) * 0.5f);
    page->number.widget.frame->fields.scale_x = number(scale * 0.5f);
    page->number.widget.frame->fields.scale_y =
        page->number.widget.frame->fields.scale_x;
    if (!ab_ui_text_widget_clip(&page->number, &page->number_clipping,
                                (AbUiPointerValue){0}, (AbUiPointerValue){0}, error))
        return false;
    page->number.widget.frame->fields.x = number(-remaining * 0.5f);
    if (!numeric(resume->frame->fields.y, &resume_y, error) ||
        !numeric(resume->frame->fields.h, &resume_height, error))
        return false;
    page->number.widget.frame->fields.y =
        number(resume_y + (scale * resume_height) * 0.05f);
    if (page->shop_present)
        page->controls[AB_UI_PAUSE_SHOP].frame->fields.visible =
            boolean(shop_visible(page));
    if (page->toons_present)
        page->controls[AB_UI_PAUSE_TOONS].frame->fields.visible =
            boolean(toons_visible(page));
    page->middle.frame->fields.x = number(-total * 0.5f);
    page->middle.frame->fields.y = number(screen.y / 2);
    float top = resume_y + resume_height * scale;
    float tutorial_y, sound_y, tutorial_height, sound_height;
    if (!numeric(tutorials->frame->fields.y, &tutorial_y, error) ||
        !numeric(sounds->frame->fields.y, &sound_y, error) ||
        !numeric(tutorials->frame->fields.h, &tutorial_height, error) ||
        !numeric(sounds->frame->fields.h, &sound_height, error))
        return false;
    float bottom = fminf(tutorial_y - (scale * tutorial_height) * 0.5f,
                         sound_y - (scale * sound_height) * 0.5f);
    page->middle.frame->fields.y = number(top + (bottom - top) * 0.5f);
    page->available_height = bottom - top;
    return ab_ui_widget_lifecycle_base(&page->widget, AB_UI_FRAME_LAYOUT, error);
}

static bool page_lifecycle(void *context, AbUiWidget *widget, AbUiFrameMethod method,
                           AbError *error) {
    AbUiPausePage *page = context;
    if (method == AB_UI_FRAME_LAYOUT)
        return layout(page, error);
    const AbUiPausePageEnvironment *source = page->host.environment;
    bool desktop =
        named(source->device_model, "windows") || named(source->device_model, "osx");
    if (method == AB_UI_FRAME_ENTRY) {
        AbUiPointerValue display;
        if (!page->host.level_display)
            return ab_fail(error, "Missing actual PausePage level-display query");
        if (!page->host.level_display(page->host.context, &display, error))
            return false;
        page->number.state.fields.text = display;
        if (!ab_ui_widget_lifecycle_base(widget, method, error))
            return false;
        page->controls[AB_UI_PAUSE_CHECKPOINT].frame->fields.visible =
            boolean(source->show_checkpoint.kind == AB_UI_POINTER_BOOLEAN &&
                    source->show_checkpoint.boolean);
        if (page->shop_present && source->powerups_enabled &&
            source->purchase_elements) {
            AbUiWidget *shop = page->controls + AB_UI_PAUSE_SHOP;
            if (!ab_ui_image_button_state_enable(&shop->state,
                                                 boolean(!source->childlock_on),
                                                 &widget->context->images, error))
                return false;
            shop->frame->fields.visible = boolean(!source->hide_powerup_button);
        }
        for (size_t index = desktop ? 0 : 1; index < 5; ++index)
            if (!subscribe(page, index, true, error))
                return false;
        return audio_state(page, error);
    }
    if (!background_audio(page, false, error) ||
        !ab_ui_widget_lifecycle_base(widget, method, error) ||
        !notify(page, "EID_PAUSE_PAGE_CLOSED", error))
        return false;
    for (size_t index = desktop ? 0 : 1; index < 5; ++index)
        if (!subscribe(page, index, false, error))
            return false;
    return true;
}

static bool page_update(void *context, AbUiWidget *widget, float seconds, float time,
                        AbError *error) {
    AbUiPausePage *page = context;
    if (!ab_event_timer_update(&page->timer, seconds, error))
        return false;
    bool show = ab_ui_pointer_value_truthy(&widget->frame->fields.pause_page_show);
    if (page->extend_state == 0 && !show)
        return true;
    if (page->extend_state < 1 && show) {
        page->extend_state =
            fminf(page->extend_state + page->extend_speed * seconds, 1);
        if (page->extend_state == 1 && !notify(page, "EID_PAUSE_PAGE_EXTENDED", error))
            return false;
    } else if (page->extend_state > 0 && !show) {
        page->extend_state =
            fmaxf(page->extend_state - page->extend_speed * seconds, 0);
        if (page->extend_state <= 0) {
            if (!notify(page, "EID_PAUSE_PAGE_HIDE_COMPLETED", error))
                return false;
            widget->frame->fields.visible = boolean(false);
        }
    }
    if (ab_ui_pointer_value_truthy(&widget->frame->fields.visible) &&
        ab_ui_pointer_value_truthy(&widget->frame->fields.pause_page_show) &&
        page->extend_state == 1 && truthy(*page->host.current_music)) {
        if (page->host.current_music->kind != AB_DATA_STRING || !page->host.audio)
            return ab_fail(error, "Missing actual PausePage ambient music resource");
        bool playing;
        if (!ab_audio_bank_is_playing(
                page->host.audio, page->host.current_music->string, &playing, error))
            return false;
        if (!playing) {
            uint32_t handle;
            if (!ab_audio_bank_play_resource(page->host.audio,
                                             page->host.current_music->string, 1, true,
                                             7, &handle, error))
                return false;
        }
    }
    float width;
    if (!ab_ui_pause_page_total_width(page, &width, error))
        return false;
    page->dummy.frame->fields.x = number(page->extend_state * width);
    return ab_ui_widget_update_base(widget, seconds, time, error);
}

static bool rectangle(AbUiPausePage *page, float alpha, float right, float bottom,
                      AbError *error) {
    AbUiWidgetContext *context = page->widget.context;
    AbGameRectangle rectangle = {
        .color = {0, 0, 0, alpha}, .right = right, .bottom = bottom};
    AbGameRectanglePlan plan;
    if (!ab_game_rectangle_plan(&rectangle, &plan, error))
        return false;
    context->render_state->geometry = (AbSpriteRenderState){.scale_x = 1, .scale_y = 1};
    context->render_state->alpha = 1;
    AbDrawCommand command = {0};
    bool visible;
    if (!ab_fill_rectangle_reset_command(
            (AbFillRectangle){plan.argb, plan.left, plan.top, plan.width, plan.height,
                              AB_FILL_BLEND_AUTO},
            context->render.viewport_width, context->render.viewport_height,
            page->host.solid_texture, &command, &visible, error))
        return false;
    command.scissor = context->render_state->clip;
    return !visible || ab_draw_list_push(context->draw_list, &command, error);
}

static bool page_draw(void *context, AbUiWidget *widget, const AbUiDrawInput *input,
                      AbError *error) {
    AbUiPausePage *page = context;
    if (widget->frame->fields.visible.kind != AB_UI_POINTER_BOOLEAN ||
        !widget->frame->fields.visible.boolean ||
        page->host.tutorial_active->kind != AB_DATA_NIL)
        return true;
    float width;
    if (!ab_ui_pause_page_total_width(page, &width, error) || !input)
        return false;
    if (!rectangle(page, page->extend_state * 0.75f, ceilf(page->host.screen->x),
                   ceilf(page->host.screen->y), error) ||
        !rectangle(page, page->extend_state * 1, width * page->extend_state,
                   ceilf(page->host.screen->y), error))
        return false;
    AbUiDrawInput parent = *input;
    parent.present &= 15;
    return ab_ui_widget_draw_base(widget, &parent, error);
}

static bool page_pointer(void *context, AbUiWidget *widget, AbSpan event_name,
                         AbVec2 point, AbUiPointerReply *output, AbError *error) {
    AbUiPausePage *page = context;
    AbUiPointerReply reply = {0};
    if (page->block_input) {
        reply.values[0] = text("BLOCK");
    } else {
        if (!ab_ui_widget_pointer_base(widget, event_name, point, &reply, error))
            return false;
        if (!ab_ui_pointer_value_truthy(reply.values))
            reply = (AbUiPointerReply){.values = {number(-1)}};
    }
    *output = reply;
    return true;
}
