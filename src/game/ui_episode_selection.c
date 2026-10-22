#include "ab/ui_episode_selection.h"

#include "ab/ui_value_text.h"

#include <math.h>
#include <string.h>

static AbUiPointerValue text(const char *name) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                              .text = {(const uint8_t *)name, strlen(name)}};
}

static AbUiPointerValue number(float value) {
    AbUiPointerValue result = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&result.number_bits, &value, sizeof(value));
    return result;
}

static AbDataValue key(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = text(name).text};
}

static AbDataValue get(const AbDataTable *table, AbDataValue name) {
    const AbDataValue *value = ab_data_get(table, name);
    return value ? *value : (AbDataValue){0};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    return get(table, key(name));
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool equal(AbUiPointerValue value, const char *name) {
    return value.kind == AB_UI_POINTER_TEXT && value.text.size == strlen(name) &&
           !memcmp(value.text.data, name, value.text.size);
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind == AB_DATA_NUMBER) {
        memcpy(output, &value.bits, sizeof(*output));
        if (isfinite(*output))
            return true;
    }
    ab_fail(error, "Episode selection requires finite numeric state");
    return false;
}

static AbDataValue data_number(float value) {
    return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = number(value).number_bits};
}

static AbDataValue event_id(AbUiEpisodeSelection *owner, const char *name) {
    return field(owner->host.event_ids, name);
}

static bool base_lifecycle(AbUiEpisodeSelection *owner, AbUiFrameMethod method,
                           AbError *error) {
    AbUiWidgetMethods *base = &owner->host.root;
    return base->lifecycle
               ? base->lifecycle(base->context, &owner->widget, method, error)
               : ab_ui_widget_lifecycle_base(&owner->widget, method, error);
}

static bool image(AbUiEpisodeSelection *owner, AbUiWidget *widget, const char *name,
                  const char *resource, const char *action, AbError *error) {
    AbUiImageButtonArguments arguments = {
        .image.frame.name = text(name),
        .return_value = action ? text(action) : (AbUiPointerValue){0},
        .activate_on_release = {.kind = AB_UI_POINTER_BOOLEAN, .boolean = true}};
    if (!ab_ui_widget_init(widget, owner->widget.context,
                           action ? AB_UI_WIDGET_IMAGE_BUTTON : AB_UI_WIDGET_IMAGE,
                           (AbUiWidgetChildren){0}, NULL,
                           action ? NULL : &arguments.image, action ? &arguments : NULL,
                           error))
        return false;
    return action ? ab_ui_image_button_state_set_image(&widget->state, text(resource),
                                                       (AbUiPointerValue){0},
                                                       &widget->context->images, error)
                  : ab_ui_image_state_set_image(&widget->state.image, text(resource),
                                                &widget->context->images, error);
}

static bool decorations(AbUiEpisodeSelection *owner, AbError *error) {
    return image(owner, &owner->left_decoration, "left", "LS_MAIN_LEFT", NULL, error) &&
           image(owner, &owner->right_decoration, "right", "LS_MAIN_RIGHT", NULL,
                 error) &&
           ab_ui_widget_add(&owner->widget, &owner->left_decoration, error) &&
           ab_ui_widget_add(&owner->widget, &owner->right_decoration, error);
}

bool ab_ui_episode_selection_update_selection(AbUiEpisodeSelection *owner, bool force,
                                              AbError *error) {
    size_t selection =
        force ? owner->current_selection : owner->cards.scroll.current_anchor;
    if (!force && selection == owner->current_selection)
        return true;
    if (!ab_episode_preferences_select(owner->host.preferences,
                                       data_number((float)selection), error))
        return false;
    owner->current_selection = owner->cards.scroll.current_anchor = selection;
    size_t visible = owner->cards.count - owner->cards.scroll.anchor_count + 1;
    if (owner->arrows) {
        owner->left.frame->fields.visible =
            (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = selection > 1};
        owner->right.frame->fields.visible =
            (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN,
                               .boolean = visible < owner->cards.count &&
                                          selection <= owner->cards.count - visible};
    }
    return true;
}

static bool layout(AbUiEpisodeSelection *owner, AbError *error) {
    AbUiScaling *scaling = owner->host.cards.buttons.scaling;
    AbUiScalingEnvironment view;
    float scale, margin_x, margin_y, back_w, back_h;
    AbDataValue margins = field(owner->host.cards.globals, "g_layoutMargin");
    AbUiTextMetricBoundaries fonts =
        ab_ui_font_resources_boundaries(owner->host.cards.buttons.fonts);
    if (!fonts.resources.set_font(fonts.resources.context, text("FONT_BASIC"), error) ||
        !scaling->host.environment(scaling->host.context, &view, error) ||
        !ab_ui_scaling_get(scaling, &scale, error) || margins.kind != AB_DATA_TABLE ||
        !numeric(field(margins.table, "x"), &margin_x, error) ||
        !numeric(field(margins.table, "y"), &margin_y, error) ||
        !ab_ui_image_state_refresh(&owner->back.state.image,
                                   &owner->widget.context->images, error) ||
        !numeric((AbDataValue){.kind = AB_DATA_NUMBER,
                               .bits = owner->back.frame->fields.w.number_bits},
                 &back_w, error) ||
        !numeric((AbDataValue){.kind = AB_DATA_NUMBER,
                               .bits = owner->back.frame->fields.h.number_bits},
                 &back_h, error))
        return false;
    owner->left_decoration.frame->fields.x = number(0);
    owner->left_decoration.frame->fields.y = number(view.screen.y);
    owner->right_decoration.frame->fields.x = number(view.screen.x);
    owner->right_decoration.frame->fields.y = number(view.screen.y);
    owner->back.frame->fields.x = number(back_w * 0.5f * scale + margin_x);
    owner->back.frame->fields.y =
        number(view.screen.y - back_h * 0.5f * scale - margin_y);
    AbUiWidget *fixed[] = {&owner->left_decoration, &owner->right_decoration,
                           &owner->back};
    for (size_t index = 0; index < 3; ++index)
        if (!ab_ui_scaling_set_to(scaling, &fixed[index]->frame->fields, error))
            return false;
    if (owner->arrows) {
        AbVec2 bounds;
        if (!ab_ui_scaling_sprite(scaling, &owner->widget.context->images,
                                  text("BUTTON_ARROW_LEFT").text, false, &bounds,
                                  error))
            return false;
        owner->left.frame->fields.x = number(bounds.x * 0.5f + margin_x);
        owner->right.frame->fields.x =
            number(view.screen.x - bounds.x * 0.5f - margin_x);
        owner->left.frame->fields.y = owner->right.frame->fields.y =
            number(view.screen.y * 0.5f);
        if (!ab_ui_scaling_set_to(scaling, &owner->left.frame->fields, error) ||
            !ab_ui_scaling_set_to(scaling, &owner->right.frame->fields, error))
            return false;
    }
    return ab_ui_episode_selection_update_selection(owner, true, error) &&
           base_lifecycle(owner, AB_UI_FRAME_LAYOUT, error);
}

static bool lifecycle(void *context, AbUiWidget *widget, AbUiFrameMethod method,
                      AbError *error) {
    (void)widget;
    AbUiEpisodeSelection *owner = context;
    if (method == AB_UI_FRAME_LAYOUT)
        return layout(owner, error);
    if (method == AB_UI_FRAME_ENTRY &&
        !ab_ui_episode_selection_update_selection(owner, true, error))
        return false;
    return base_lifecycle(owner, method, error) &&
           (method != AB_UI_FRAME_ENTRY || layout(owner, error));
}

static bool draw(void *context, AbUiWidget *widget, const AbUiDrawInput *input,
                 AbError *error) {
    AbUiEpisodeSelection *owner = context;
    AbUiWidgetMethods *base = &owner->host.root;
    return base->draw ? base->draw(base->context, widget, input, error)
                      : ab_ui_widget_draw_base(widget, input, error);
}

static bool update(void *context, AbUiWidget *widget, float seconds, float time,
                   AbError *error) {
    AbUiEpisodeSelection *owner = context;
    AbUiWidgetMethods *base = &owner->host.root;
    bool okay = base->update ? base->update(base->context, widget, seconds, time, error)
                             : ab_ui_widget_update_base(widget, seconds, time, error);
    return okay && ab_ui_episode_selection_update_selection(owner, false, error);
}

static bool load_theme(void *context, AbError *error) {
    AbUiEpisodeSelection *owner = context;
    AbDataValue record =
        get(owner->host.cards.buttons.episodes, owner->selected_episode);
    if (!truthy(record))
        return true;
    if (record.kind != AB_DATA_TABLE || !record.table)
        return ab_fail(error, "Malformed selected episode record");
    AbDataValue pages = field(record.table, "pages");
    AbDataValue first = pages.kind == AB_DATA_TABLE ? get(pages.table, data_number(1))
                                                    : (AbDataValue){0};
    AbDataValue theme =
        first.kind == AB_DATA_TABLE ? field(first.table, "theme") : (AbDataValue){0};
    if (!truthy(theme))
        return true;
    return theme.kind == AB_DATA_STRING && owner->host.load_theme
               ? owner->host.load_theme(owner->host.context, theme.string, error)
               : ab_fail(error, "Missing actual episode loading theme boundary");
}

static bool loading(AbUiEpisodeSelection *owner, AbUiPointerValue episode, bool locked,
                    AbError *error) {
    if (episode.kind != AB_UI_POINTER_NUMBER && episode.kind != AB_UI_POINTER_TEXT)
        return ab_fail(error, "Episode navigation requires a catalog identifier");
    uint8_t scratch[32];
    AbSpan suffix;
    if (!ab_ui_value_text(episode, scratch, sizeof(scratch), &suffix) ||
        suffix.size > sizeof(owner->target) - 16)
        return ab_fail(error, "Episode navigation target exceeds storage");
    owner->selected_episode =
        episode.kind == AB_UI_POINTER_TEXT
            ? (AbDataValue){.kind = AB_DATA_STRING, .string = episode.text}
            : (AbDataValue){.kind = AB_DATA_NUMBER, .bits = episode.number_bits};
    memcpy(owner->target, "LEVEL_SELECTION_", 16);
    memcpy(owner->target + 16, suffix.data, suffix.size);
    owner->completion_fields[1].value = (AbDataValue){
        .kind = AB_DATA_STRING, .string = {owner->target, 16 + suffix.size}};
    AbEventManager *events = owner->host.cards.scroll.events;
    return ab_event_manager_notify(
        events, locked ? &owner->registration : &owner->loading_event, error);
}

static bool pointer(void *context, AbUiWidget *widget, AbSpan event, AbVec2 point,
                    AbUiPointerReply *output, AbError *error) {
    AbUiEpisodeSelection *owner = context;
    AbUiWidgetMethods *base = &owner->host.root;
    bool okay = base->pointer
                    ? base->pointer(base->context, widget, event, point, output, error)
                    : ab_ui_widget_pointer_base(widget, event, point, output, error);
    if (!okay)
        return false;
    AbUiPointerValue action = output->values[0];
    if (equal(action, "GOTO_MAIN_MENU"))
        return ab_event_manager_notify(owner->host.cards.scroll.events,
                                       &owner->back_event, error);
    if (equal(action, "SCROLL_LEFT"))
        return ab_ui_scroll_frame_previous(&owner->cards.scroll, error);
    if (equal(action, "SCROLL_RIGHT"))
        return ab_ui_scroll_frame_next(&owner->cards.scroll, error);
    bool locked = equal(action, "LOCKED_LEVEL_SELECTION");
    if (!locked && !equal(action, "GOTO_LEVEL_SELECTION"))
        return true;
    AbUiWidgetContext *ui = widget->context;
    if (!ui->play_audio)
        return ab_fail(error, "Missing episode menu audio boundary");
    if (!locked && equal(output->values[1], "birdday5UGC") &&
        !truthy(
            field(owner->host.cards.buttons.settings, "UGCEpisodeCardGiftBoxOpened"))) {
        AbUiPointerValue sound;
        if (!owner->host.open_gift ||
            !owner->host.open_gift(owner->host.context, &sound, error))
            return ab_fail(error, "Missing actual UGC gift settings/audio boundary");
        if (!ui->play_audio(ui->audio_context, sound, 1, false, error))
            return false;
        *output = (AbUiPointerReply){0};
        return true;
    }
    return ui->play_audio(ui->audio_context, text("menu_confirm"), 1, false, error) &&
           loading(owner, output->values[1], locked, error);
}

static bool event(void *context, AbDataTable *data, AbError *error) {
    AbUiEpisodeSelection *owner = context;
    (void)error;
    /* Source eventTriggered only stores the request. Its update, after the
     * base ScrollFrame update, consumes the nil/explicit target branch once. */
    owner->cards.autoscroll_on_entry = true;
    owner->cards.scroll_to_episode = field(data, "targetEpisode");
    return true;
}

static void events(AbUiEpisodeSelection *owner) {
    owner->load = (AbClassicLoaderCall){owner, load_theme};
    owner->calls[0] = &owner->load;
    owner->loading_calls = (AbClassicLoaderCalls){owner->calls, 2};
    owner->item =
        (AbDataEntry){data_number(1), {.kind = AB_DATA_OBJECT, .object = &owner->load}};
    owner->loading_items = (AbDataTable){&owner->item, 1, 1, NULL};
    owner->completion_fields[0] =
        (AbDataEntry){key("id"), event_id(owner, "EID_CHANGE_SCENE")};
    owner->completion_fields[1].key = key("target");
    owner->completion_fields[2] = (AbDataEntry){key("from"), key("EPISODE_SELECTION")};
    owner->completion = (AbDataTable){owner->completion_fields, 3, 3, NULL};
    owner->loading_fields[0] =
        (AbDataEntry){key("id"), event_id(owner, "EID_DO_LOADING")};
    owner->loading_fields[1] = (AbDataEntry){
        key("items"), {.kind = AB_DATA_TABLE, .table = &owner->loading_items}};
    owner->loading_fields[2] = (AbDataEntry){
        key("completion_event"), {.kind = AB_DATA_TABLE, .table = &owner->completion}};
    owner->loading_event = (AbDataTable){owner->loading_fields, 3, 3, NULL};
    owner->back_fields[0] = owner->completion_fields[0];
    owner->back_fields[1] = (AbDataEntry){key("target"), key("EXIT_EPISODE_SELECTION")};
    owner->back_fields[2] = owner->completion_fields[2];
    owner->back_event = (AbDataTable){owner->back_fields, 3, 3, NULL};
    owner->registration_fields[0] =
        (AbDataEntry){key("id"), event_id(owner, "EID_SHOW_REGISTRATION_DIALOG")};
    owner->registration_fields[1] = (AbDataEntry){key("from"), key("LEVEL_SELECTION")};
    owner->registration_fields[2] = (AbDataEntry){
        key("success_event"), {.kind = AB_DATA_TABLE, .table = &owner->loading_event}};
    owner->registration = (AbDataTable){owner->registration_fields, 3, 3, NULL};
}

bool ab_ui_episode_selection_init(AbUiEpisodeSelection *owner,
                                  AbUiWidgetContext *context,
                                  AbUiEpisodeSelectionHost host, AbError *error) {
    if (!owner || owner->widget.initialized || !context || !host.preferences ||
        !host.event_ids || !host.cards.globals || !host.cards.episode_ids)
        return ab_fail(error, "Invalid episode-selection constructor");
    owner->host = host;
    owner->arrows =
        truthy(field(host.cards.globals, "g_episode_selection_use_scroll_buttons"));
    AbUiFrameFields frame = {.name = text("episodeSelectionRoot")};
    if (!ab_ui_widget_init(
            &owner->widget, context, AB_UI_WIDGET_FRAME,
            (AbUiWidgetChildren){owner->frames, owner->draw, owner->pointer, 6}, &frame,
            NULL, NULL, error))
        return false;
    owner->widget.methods = (AbUiWidgetMethods){.context = owner,
                                                .lifecycle = lifecycle,
                                                .draw = draw,
                                                .pointer = pointer,
                                                .update = update};
    bool behind = truthy(field(host.cards.globals, "g_decorationInBackground"));
    if ((behind && !decorations(owner, error)) ||
        !ab_ui_episode_scroll_init(&owner->cards, context, NULL, host.cards, error))
        return false;
    owner->cards.scroll.widget.frame->fields.name = text("episodeButtons");
    if (!ab_ui_widget_add(&owner->widget, &owner->cards.scroll.widget, error) ||
        (!behind && !decorations(owner, error)) ||
        !image(owner, &owner->back, "backButton", "ARROW_LEFT", "GOTO_MAIN_MENU",
               error))
        return false;
    owner->back.state.click_sound = text("menu_back");
    if (!ab_ui_widget_add(&owner->widget, &owner->back, error))
        return false;
    AbDataValue saved;
    float selected;
    if (!ab_episode_preferences_selected(host.preferences, &saved, error) ||
        !numeric(saved, &selected, error))
        return false;
    float clamped = fminf(fmaxf(selected, 1), (float)owner->cards.count);
    if (truncf(clamped) != clamped)
        return ab_fail(error,
                       "Fractional episode selection is outside the menu domain");
    owner->current_selection = owner->cards.scroll.current_anchor = (size_t)clamped;
    if (clamped != selected &&
        !ab_episode_preferences_select(host.preferences, data_number(clamped), error))
        return false;
    if (owner->arrows && (!image(owner, &owner->left, "left_button",
                                 "BUTTON_ARROW_LEFT", "SCROLL_LEFT", error) ||
                          !ab_ui_widget_add(&owner->widget, &owner->left, error) ||
                          !image(owner, &owner->right, "right_button",
                                 "BUTTON_ARROW_RIGHT", "SCROLL_RIGHT", error) ||
                          !ab_ui_widget_add(&owner->widget, &owner->right, error)))
        return false;
    events(owner);
    owner->listener = (AbEventListener){.context = owner, .event_triggered = event};
    if (host.cards.scroll.events) {
        if (!ab_event_manager_add(host.cards.scroll.events,
                                  event_id(owner, "EID_SCROLL_TO_EPISODE"),
                                  &owner->listener, error))
            return false;
        owner->scroll_subscribed = true;
    }
    return true;
}

bool ab_ui_episode_selection_detach(AbUiEpisodeSelection *owner, AbError *error) {
    if (!owner)
        return true;
    if (owner->scroll_subscribed) {
        if (!ab_event_manager_remove(owner->host.cards.scroll.events,
                                     event_id(owner, "EID_SCROLL_TO_EPISODE"),
                                     &owner->listener, error))
            return false;
        owner->scroll_subscribed = false;
    }
    return ab_ui_episode_scroll_detach(&owner->cards, error);
}
