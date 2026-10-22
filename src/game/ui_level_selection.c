#include "ab/ui_level_selection.h"

#include "ab/ui_value_text.h"

#include <math.h>
#include <stdio.h>
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

static AbDataValue data_number(size_t index) {
    return (AbDataValue){.kind = AB_DATA_NUMBER,
                         .bits = number((float)index).number_bits};
}

static AbDataValue get(const AbDataTable *table, AbDataValue key) {
    const AbDataValue *value = ab_data_get(table, key);
    return value ? *value : (AbDataValue){0};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    return get(table, key(name));
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool equal(AbDataValue left, AbDataValue right) {
    AbDataEntry entry = {left, {.kind = AB_DATA_BOOLEAN, .bits = 1}};
    AbDataTable singleton = {&entry, 1, 1, NULL};
    return ab_data_get(&singleton, right) != NULL;
}

static bool named(AbUiPointerValue value, const char *name) {
    return value.kind == AB_UI_POINTER_TEXT && value.text.size == strlen(name) &&
           !memcmp(value.text.data, name, value.text.size);
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Missing numeric level-selection state");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite level-selection state");
}

static bool ui_numeric(AbUiPointerValue value, float *output, AbError *error) {
    return value.kind == AB_UI_POINTER_NUMBER
               ? numeric(
                     (AbDataValue){.kind = AB_DATA_NUMBER, .bits = value.number_bits},
                     output, error)
               : ab_fail(error, "Missing numeric level-selection geometry");
}

static bool ui_value(AbDataValue value, AbUiPointerValue *output, AbError *error) {
    if (value.kind == AB_DATA_STRING)
        *output = (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = value.string};
    else if (value.kind == AB_DATA_NUMBER)
        *output =
            (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER, .number_bits = value.bits};
    else if (value.kind == AB_DATA_BOOLEAN)
        *output = boolean(value.bits != 0);
    else if (value.kind == AB_DATA_NIL)
        *output = (AbUiPointerValue){0};
    else
        return ab_fail(error, "Unsupported level-selection scalar metadata");
    return ab_ui_pointer_value_valid(output);
}

static AbDataValue event_id(const AbUiLevelSelection *owner, const char *name) {
    return field(owner->host.levels.event_ids, name);
}

static bool base_lifecycle(AbUiLevelSelection *owner, AbUiFrameMethod method,
                           AbError *error) {
    AbUiWidgetMethods *root = &owner->host.root;
    return root->lifecycle
               ? root->lifecycle(root->context, &owner->widget, method, error)
               : ab_ui_widget_lifecycle_base(&owner->widget, method, error);
}

static bool image(AbUiLevelSelection *owner, AbUiWidget *widget, const char *name,
                  const char *resource, const char *action, AbError *error) {
    AbUiImageButtonArguments arguments = {.image.frame.name = text(name)};
    if (action)
        arguments.return_value = text(action);
    if (!ab_ui_widget_init(widget, owner->widget.context,
                           action ? AB_UI_WIDGET_IMAGE_BUTTON : AB_UI_WIDGET_IMAGE,
                           (AbUiWidgetChildren){0}, NULL,
                           action ? NULL : &arguments.image, action ? &arguments : NULL,
                           error))
        return false;
    bool okay =
        action
            ? ab_ui_image_button_state_set_image(&widget->state, text(resource),
                                                 (AbUiPointerValue){0},
                                                 &owner->widget.context->images, error)
            : ab_ui_image_state_set_image(&widget->state.image, text(resource),
                                          &owner->widget.context->images, error);
    return okay && ab_ui_widget_add(&owner->widget, widget, error);
}

static bool decoration(AbUiLevelSelection *owner, AbUiWidget *widget,
                       const char *property, AbError *error) {
    AbDataValue value = field(owner->metadata, property);
    widget->frame->fields.visible = boolean(value.kind != AB_DATA_NIL);
    if (value.kind == AB_DATA_NIL)
        return true;
    AbUiPointerValue selected;
    return ui_value(value, &selected, error) &&
           ab_ui_image_state_set_image(&widget->state.image, selected,
                                       &owner->widget.context->images, error);
}

static bool selection_index(AbUiLevelSelection *owner, AbDataValue value,
                            size_t *output, AbError *error) {
    float selected = 0;
    if (!numeric(value, &selected, error))
        return false;
    if (selected < 1 || selected > (float)owner->levels.page_count ||
        truncf(selected) != selected)
        return ab_fail(error, "Unsupported level-selection page index");
    *output = (size_t)selected;
    return true;
}

bool ab_ui_level_selection_update_selection(AbUiLevelSelection *owner, bool force,
                                            AbError *error) {
    if (!owner || !owner->widget.initialized || owner->widget.context->failed)
        return ab_fail(error, "Invalid level-selection owner");
    size_t selected = owner->levels.scroll.current_anchor;
    if (force) {
        AbDataValue stored;
        if (!ab_episode_preferences_current_page(owner->host.preferences,
                                                 owner->episode, &stored, error) ||
            !selection_index(owner, stored, &selected, error))
            return false;
    }
    if (!force && selected == owner->current_selection)
        return true;
    if (!selected || selected > owner->levels.page_count || !owner->current_selection ||
        owner->current_selection > owner->levels.page_count)
        return ab_fail(error, "Level-selection anchor is outside actual pages");
    if (!ab_episode_preferences_set_page(owner->host.preferences, owner->episode,
                                         data_number(selected), error) ||
        !ab_episode_preferences_visit_page(owner->host.preferences, owner->episode,
                                           data_number(selected), error))
        return false;
    if (!ab_ui_image_state_set_image(
            &owner->dots[owner->current_selection - 1].state.image,
            text("LS_DOT_BLACK"), &owner->widget.context->images, error))
        return false;
    owner->page_numbers[owner->current_selection - 1].widget.frame->fields.visible =
        boolean(false);
    if (!ab_ui_image_state_set_image(&owner->dots[selected - 1].state.image,
                                     text("LS_DOT_WHITE"),
                                     &owner->widget.context->images, error))
        return false;
    owner->page_numbers[selected - 1].widget.frame->fields.visible = boolean(true);
    owner->current_selection = owner->levels.current_selection = selected;
    if (owner->host.levels.arrows) {
        owner->left.frame->fields.visible = boolean(selected > 1);
        owner->right.frame->fields.visible =
            boolean(selected < owner->levels.page_count);
    }
    return true;
}

static bool layout(AbUiLevelSelection *owner, AbError *error) {
    AbUiScaling *scaling = owner->host.levels.buttons.scaling;
    AbUiScalingEnvironment view = {0};
    float scale = 0, back_w = 0, back_h = 0;
    AbVec2 margin = owner->host.levels.margin;
    if (!scaling->host.environment(scaling->host.context, &view, error) ||
        !ab_ui_scaling_get(scaling, &scale, error) ||
        !ab_ui_image_state_refresh(&owner->back.state.image,
                                   &owner->widget.context->images, error) ||
        !ui_numeric(owner->back.frame->fields.w, &back_w, error) ||
        !ui_numeric(owner->back.frame->fields.h, &back_h, error))
        return false;
    owner->left_decoration.frame->fields.x = number(0);
    owner->left_decoration.frame->fields.y = number(view.screen.y);
    owner->right_decoration.frame->fields.x = number(view.screen.x);
    owner->right_decoration.frame->fields.y = number(view.screen.y);
    owner->back.frame->fields.x = number(back_w * 0.5f * scale + margin.x);
    owner->back.frame->fields.y =
        number(view.screen.y - back_h * 0.5f * scale - margin.y);
    AbUiWidget *fixed[] = {&owner->left_decoration, &owner->right_decoration,
                           &owner->back};
    for (size_t index = 0; index < 3; ++index)
        if (!ab_ui_scaling_set_to(scaling, &fixed[index]->frame->fields, error))
            return false;
    AbVec2 dot;
    if (!owner->widget.context->images.bounds(owner->widget.context->images.context,
                                              text("LS_DOT_WHITE").text, &dot, error))
        return false;
    float spacing = (dot.x * 2) * scale;
    float origin =
        view.screen.x * 0.5f - (((float)owner->levels.page_count - 1) * 0.5f) * spacing;
    for (size_t index = 0; index < owner->levels.page_count; ++index) {
        AbUiFrameFields *frame = &owner->dots[index].frame->fields;
        frame->x = number(origin + spacing * (float)index);
        frame->y = number(view.screen.y - 10 - margin.y);
        AbUiTextWidget *label = owner->page_numbers + index;
        float height = 0;
        if (!ab_ui_scaling_set_to(scaling, frame, error) ||
            !ab_ui_scaling_set_to(scaling, &label->widget.frame->fields, error) ||
            !ab_ui_text_widget_get_height(label, &height, error))
            return false;
        label->widget.frame->fields.x = frame->x;
        label->widget.frame->fields.y = number(view.screen.y - 12 - margin.y - height);
    }
    if (owner->host.levels.arrows) {
        AbVec2 size;
        if (!ab_ui_scaling_sprite(scaling, &owner->widget.context->images,
                                  text("BUTTON_ARROW_LEFT").text, false, &size, error))
            return false;
        owner->left.frame->fields.x = number(size.x * 0.5f + margin.x);
        owner->right.frame->fields.x = number(view.screen.x - size.x * 0.5f - margin.x);
        owner->left.frame->fields.y = owner->right.frame->fields.y =
            number(view.screen.y * 0.5f);
        if (!ab_ui_scaling_set_to(scaling, &owner->left.frame->fields, error) ||
            !ab_ui_scaling_set_to(scaling, &owner->right.frame->fields, error))
            return false;
    }
    AbVec2 anchors[AB_UI_LEVEL_SCROLL_PAGES];
    for (size_t index = 0; index < owner->levels.page_count; ++index)
        anchors[index] = (AbVec2){(float)index * view.screen.x, -margin.y};
    return ab_ui_scroll_frame_set_anchors(&owner->levels.scroll, anchors,
                                          owner->levels.page_count, error) &&
           base_lifecycle(owner, AB_UI_FRAME_LAYOUT, error);
}

static bool lifecycle(void *context, AbUiWidget *widget, AbUiFrameMethod method,
                      AbError *error) {
    AbUiLevelSelection *owner = context;
    (void)widget;
    if (method == AB_UI_FRAME_LAYOUT)
        return layout(owner, error);
    if (method == AB_UI_FRAME_ENTRY) {
        if (!ab_ui_level_selection_update_selection(owner, true, error) ||
            !ab_event_manager_add(owner->host.levels.scroll.events,
                                  event_id(owner, "EID_MIGHTY_EAGLE_AVAILABLE"),
                                  &owner->listener, error))
            return false;
        owner->eagle_subscribed = true;
        if (!ab_episode_preferences_see_change(owner->host.preferences, owner->episode,
                                               error) ||
            (owner->host.query_enter_level_selection &&
             !owner->host.query_enter_level_selection(owner->host.context, error)))
            return false;
    } else if (method == AB_UI_FRAME_EXIT && owner->eagle_subscribed) {
        if (!ab_event_manager_remove(owner->host.levels.scroll.events,
                                     event_id(owner, "EID_MIGHTY_EAGLE_AVAILABLE"),
                                     &owner->listener, error))
            return false;
        owner->eagle_subscribed = false;
    }
    return base_lifecycle(owner, method, error);
}

static bool shade(AbUiLevelSelection *owner, float position, AbError *error) {
    if (!isfinite(position))
        return ab_fail(error, "Nonfinite level-selection background position");
    size_t first = 1, second = 1;
    float amount = 0;
    if (position >= (float)owner->levels.page_count)
        first = second = owner->levels.page_count;
    else if (position > 1) {
        first = (size_t)floorf(position);
        second = first + 1;
        amount = position - (float)first;
    }
    AbDataValue left = get(owner->levels.pages, data_number(first));
    AbDataValue right = get(owner->levels.pages, data_number(second));
    AbDataValue left_color =
        left.kind == AB_DATA_TABLE ? field(left.table, "colour") : (AbDataValue){0};
    AbDataValue right_color =
        right.kind == AB_DATA_TABLE ? field(right.table, "colour") : (AbDataValue){0};
    if (left_color.kind != AB_DATA_TABLE || right_color.kind != AB_DATA_TABLE)
        return ab_fail(error, "Missing real level-selection page colors");
    const char *const channels[] = {"r", "g", "b"};
    for (size_t index = 0; index < 3; ++index) {
        float left_value = 0, right_value = 0;
        if (!numeric(field(left_color.table, channels[index]), &left_value, error) ||
            !numeric(field(right_color.table, channels[index]), &right_value, error))
            return false;
        owner->background[index] = left_value + amount * (right_value - left_value);
    }
    return true;
}

static bool update(void *context, AbUiWidget *widget, float seconds, float time,
                   AbError *error) {
    AbUiLevelSelection *owner = context;
    if (owner->host.interpolate_background) {
        AbUiScalingEnvironment view = {0};
        AbUiScaling *scaling = owner->host.levels.buttons.scaling;
        float x = 0;
        if (!scaling->host.environment(scaling->host.context, &view, error) ||
            !ui_numeric(owner->levels.scroll.widget.scroll[0], &x, error) ||
            !shade(owner, x / view.screen.x + 1, error))
            return false;
    }
    if (owner->scroll_to_next_page) {
        float elapsed = *owner->host.script_time - owner->next_page_timestamp;
        if (!isfinite(elapsed))
            return ab_fail(error, "Nonfinite deferred level-page time");
        if (elapsed > 0.1f) {
            if (!ab_ui_scroll_frame_next(&owner->levels.scroll, error))
                return false;
            owner->scroll_to_next_page = false;
        }
    }
    AbUiWidgetMethods *root = &owner->host.root;
    bool okay = root->update ? root->update(root->context, widget, seconds, time, error)
                             : ab_ui_widget_update_base(widget, seconds, time, error);
    return okay && ab_ui_level_selection_update_selection(owner, false, error);
}

static bool draw(void *context, AbUiWidget *widget, const AbUiDrawInput *input,
                 AbError *error) {
    AbUiLevelSelection *owner = context;
    AbUiWidgetMethods *root = &owner->host.root;
    return root->draw ? root->draw(root->context, widget, input, error)
                      : ab_ui_widget_draw_base(widget, input, error);
}

static bool launch(AbUiLevelSelection *owner, AbUiPointerValue metadata,
                   AbError *error) {
    AbDataTable *meta = NULL;
    const AbDataTable *page = NULL;
    for (size_t index = 0; index < owner->levels.count; ++index) {
        AbUiLevelButton *button = owner->levels.buttons + index;
        if (metadata.kind == AB_UI_POINTER_OBJECT &&
            metadata.object == (uintptr_t)&button->meta) {
            meta = &button->meta;
            page = button->page_metadata;
            break;
        }
    }
    if (!meta || !page)
        return ab_fail(error, "Level launch requires actual owned button metadata");
    AbDataValue cutscene = field(meta, "intro_cutscene");
    owner->launch_fields[0] = (AbDataEntry){
        key("id"), event_id(owner, truthy(cutscene) ? "EID_LOAD_INTRO_CUTSCENE"
                                                    : "EID_CHANGE_LEVEL")};
    owner->launch_fields[1] =
        (AbDataEntry){key("data"), {.kind = AB_DATA_TABLE, .table = meta}};
    owner->launch_fields[2] =
        truthy(cutscene) ? (AbDataEntry){key("cutscene"), cutscene}
                         : (AbDataEntry){key("from"), key("levelselection menu")};
    owner->launch_event = (AbDataTable){owner->launch_fields, 3, 4, NULL};
    bool allowed = !owner->host.trial_version || truthy(field(page, "free_to_play"));
    return ab_event_manager_notify(
        owner->host.levels.scroll.events,
        allowed ? &owner->launch_event : &owner->registration, error);
}

static bool pointer(void *context, AbUiWidget *widget, AbSpan action, AbVec2 point,
                    AbUiPointerReply *reply, AbError *error) {
    AbUiLevelSelection *owner = context;
    AbUiWidgetMethods *root = &owner->host.root;
    bool okay = root->pointer
                    ? root->pointer(root->context, widget, action, point, reply, error)
                    : ab_ui_widget_pointer_base(widget, action, point, reply, error);
    if (!okay)
        return false;
    if (named(reply->values[0], "SCROLL_LEFT"))
        return ab_ui_scroll_frame_previous(&owner->levels.scroll, error);
    if (named(reply->values[0], "SCROLL_RIGHT"))
        return ab_ui_scroll_frame_next(&owner->levels.scroll, error);
    /* The unbound inherited pointer uses the source ordinary Back/launch subset
     * for isolated components. A real inherited root already owns those calls. */
    if (!root->pointer && named(reply->values[0], "EPISODE_SELECTION")) {
        owner->back_fields[1].value = owner->exit_link;
        return ab_event_manager_notify(owner->host.levels.scroll.events,
                                       &owner->back_event, error);
    }
    if (!root->pointer && named(reply->values[0], "GOTO_LEVEL"))
        return launch(owner, reply->values[1], error);
    if (!root->pointer && owner->host.free_version &&
        named(reply->values[0], "DISABLED_LEVEL"))
        return ab_fail(
            error, "Reached free-version locked-level dialog requires root services");
    return true;
}

static bool event(void *context, AbDataTable *data, AbError *error) {
    AbUiLevelSelection *owner = context;
    AbDataValue id = field(data, "id");
    if (equal(id, event_id(owner, "EID_SCROLL_TO_NEXT_WORLD")) &&
        equal(field(data, "episode"), owner->episode)) {
        owner->scroll_to_next_page = true;
        owner->next_page_timestamp = *owner->host.script_time;
    }
    if (equal(id, event_id(owner, "EID_MIGHTY_EAGLE_AVAILABLE"))) {
        for (size_t index = 0; index < owner->levels.count; ++index) {
            AbUiLevelButton *button = owner->levels.buttons + index;
            if (ab_ui_pointer_value_truthy(&button->eagle.frame->fields.visible)) {
                button->eagle.frame->fields.visible = boolean(false);
                button->number.widget.frame->fields.visible = boolean(true);
            }
        }
        return ab_ui_level_scroll_visible(&owner->levels, error);
    }
    return true;
}

static bool restore_selection(AbUiLevelSelection *owner, AbError *error) {
    AbDataValue stored;
    float selected = 0;
    if (!ab_episode_preferences_current_page(owner->host.preferences, owner->episode,
                                             &stored, error) ||
        !numeric(stored, &selected, error))
        return false;
    bool new_levels = false;
    if (!ab_episode_progress_has_new_levels(owner->host.episode_progress,
                                            owner->episode, &new_levels, error))
        return false;
    if (new_levels) {
        AbDataValue page;
        size_t new_page = 0;
        if (!ab_episode_progress_new_page(owner->host.episode_progress, owner->episode,
                                          &page, error) ||
            !selection_index(owner, page, &new_page, error))
            return false;
        bool all_visited = true, visited = false;
        for (size_t index = 1; index <= new_page; ++index) {
            if (!ab_episode_progress_page_visited(owner->host.episode_progress,
                                                  owner->episode, data_number(index),
                                                  &visited, error))
                return false;
            if (!visited)
                all_visited = false;
        }
        if (all_visited) {
            if (!ab_episode_progress_page_visited(owner->host.episode_progress,
                                                  owner->episode, page, &visited,
                                                  error))
                return false;
            if (!visited)
                selected = (float)new_page;
        }
    }
    if (selected > (float)owner->levels.page_count) {
        selected = (float)owner->levels.page_count;
        if (!ab_episode_preferences_set_page(owner->host.preferences, owner->episode,
                                             data_number(owner->levels.page_count),
                                             error))
            return false;
    }
    if (!selection_index(
            owner,
            (AbDataValue){.kind = AB_DATA_NUMBER, .bits = number(selected).number_bits},
            &owner->current_selection, error))
        return false;
    owner->levels.current_selection = owner->current_selection;
    return true;
}

bool ab_ui_level_selection_init(AbUiLevelSelection *owner, AbUiWidgetContext *context,
                                AbDataValue episode, AbUiLevelSelectionHost host,
                                AbError *error) {
    if (!owner || owner->widget.initialized || !context || !host.preferences ||
        !host.episode_progress || !host.script_time ||
        host.exit_link.kind == AB_DATA_NIL)
        return ab_fail(error, "Invalid level-selection constructor host/storage");
    owner->host = host;
    owner->episode = episode;
    owner->exit_link = host.exit_link;
    AbDataValue record = get(host.levels.buttons.episodes, episode);
    if (record.kind != AB_DATA_TABLE || !record.table)
        return ab_fail(error, "Missing actual level-selection episode");
    owner->metadata = record.table;
    AbDataValue bonus = field(record.table, "bonus_levels");
    if (truthy(bonus) &&
        (bonus.kind != AB_DATA_TABLE || !bonus.table || bonus.table->count))
        return ab_fail(error,
                       "Nonempty bonus level UI remains a separate layout binding");
    AbUiPointerValue id;
    uint8_t scratch[32];
    AbSpan suffix;
    if (!ui_value(episode, &id, error) ||
        !ab_ui_value_text(id, scratch, sizeof(scratch), &suffix) ||
        suffix.size > sizeof(owner->name) - 15)
        return ab_fail(error, "Level-selection name exceeds source identifier storage");
    memcpy(owner->name, "LevelSelection", 14);
    memcpy(owner->name + 14, suffix.data, suffix.size);
    owner->name[14 + suffix.size] = 0;
    AbUiFrameFields frame = {.name = text(owner->name)};
    if (!ab_ui_widget_init(&owner->widget, context, AB_UI_WIDGET_FRAME,
                           (AbUiWidgetChildren){owner->frames, owner->draw,
                                                owner->pointer,
                                                AB_UI_LEVEL_SELECTION_CHILDREN},
                           &frame, NULL, NULL, error))
        return false;
    owner->widget.methods = (AbUiWidgetMethods){.context = owner,
                                                .lifecycle = lifecycle,
                                                .draw = draw,
                                                .pointer = pointer,
                                                .update = update};
    owner->background[3] = 255;
    if (!image(owner, &owner->left_decoration, "left", "LS_MAIN_LEFT", NULL, error) ||
        !image(owner, &owner->right_decoration, "right", "LS_MAIN_RIGHT", NULL,
               error) ||
        !ab_ui_level_scroll_init(&owner->levels, context, episode, host.levels,
                                 error) ||
        !ab_ui_widget_add(&owner->widget, &owner->levels.scroll.widget, error) ||
        !restore_selection(owner, error) ||
        !decoration(owner, &owner->left_decoration, "decor_left", error) ||
        !decoration(owner, &owner->right_decoration, "decor_right", error))
        return false;
    owner->levels.scroll.widget.frame->fields.name = text("levelButtons");
    for (size_t index = 0; index < owner->levels.page_count; ++index) {
        int count = snprintf(owner->dot_names[index], sizeof(owner->dot_names[index]),
                             "dot%zu", index + 1);
        if (count < 1 || (size_t)count >= sizeof(owner->dot_names[index]) ||
            !image(owner, owner->dots + index, owner->dot_names[index], "LS_DOT_BLACK",
                   NULL, error))
            return false;
    }
    for (size_t index = 0; index < owner->levels.page_count; ++index) {
        int count = snprintf(owner->page_names[index], sizeof(owner->page_names[index]),
                             "pageNumber%zu", index + 1);
        if (count < 1 || (size_t)count >= sizeof(owner->page_names[index]))
            return ab_fail(error, "Actual page-number widget name exceeds storage");
        AbUiTextWidget *label = owner->page_numbers + index;
        if (!ab_ui_text_widget_init(label, context, (AbUiWidgetChildren){0},
                                    host.levels.buttons.fonts, NULL, NULL, error))
            return false;
        label->widget.frame->fields.name = text(owner->page_names[index]);
        label->widget.frame->fields.visible = boolean(false);
        label->state.fields.font = text("FONT_LS_SMALL");
        AbDataValue page = get(owner->levels.pages, data_number(index + 1));
        AbDataValue display = field(page.table, "display_dot_numbers");
        AbDataValue value = key("");
        if (!(display.kind == AB_DATA_BOOLEAN && !display.bits)) {
            value = field(page.table, "level_selection_display_number");
            if (!truthy(value))
                value = field(page.table, "display_number");
            if (!truthy(value))
                value = key("");
        }
        if (!ui_value(value, &label->state.fields.text, error) ||
            !ab_ui_widget_add(&owner->widget, &label->widget, error))
            return false;
    }
    if (!image(owner, &owner->back, "backButton", "ARROW_LEFT", "EPISODE_SELECTION",
               error))
        return false;
    owner->back.state.activate_on_release = boolean(true);
    owner->back.state.click_sound = text("menu_back");
    if (host.levels.arrows && (!image(owner, &owner->left, "left_button",
                                      "BUTTON_ARROW_LEFT", "SCROLL_LEFT", error) ||
                               !image(owner, &owner->right, "right_button",
                                      "BUTTON_ARROW_RIGHT", "SCROLL_RIGHT", error)))
        return false;
    owner->back_fields[0] =
        (AbDataEntry){key("id"), event_id(owner, "EID_CHANGE_SCENE")};
    owner->back_fields[1] = (AbDataEntry){key("target"), owner->exit_link};
    owner->back_fields[2] = (AbDataEntry){key("from"), key("LEVEL_SELECTION")};
    owner->back_event = (AbDataTable){owner->back_fields, 3, 3, NULL};
    owner->registration_fields[0] =
        (AbDataEntry){key("id"), event_id(owner, "EID_SHOW_REGISTRATION_DIALOG")};
    owner->registration_fields[1] = (AbDataEntry){key("from"), key("LEVEL_SELECTION")};
    owner->registration_fields[2] = (AbDataEntry){
        key("success_event"), {.kind = AB_DATA_TABLE, .table = &owner->launch_event}};
    owner->registration = (AbDataTable){owner->registration_fields, 3, 3, NULL};
    owner->listener = (AbEventListener){.context = owner, .event_triggered = event};
    if (!ab_event_manager_add(host.levels.scroll.events,
                              event_id(owner, "EID_SCROLL_TO_NEXT_WORLD"),
                              &owner->listener, error))
        return false;
    owner->scroll_subscribed = true;
    return true;
}

bool ab_ui_level_selection_detach(AbUiLevelSelection *owner, AbError *error) {
    if (!owner)
        return true;
    if (owner->scroll_subscribed) {
        if (!ab_event_manager_remove(owner->host.levels.scroll.events,
                                     event_id(owner, "EID_SCROLL_TO_NEXT_WORLD"),
                                     &owner->listener, error))
            return false;
        owner->scroll_subscribed = false;
    }
    if (owner->eagle_subscribed) {
        if (!ab_event_manager_remove(owner->host.levels.scroll.events,
                                     event_id(owner, "EID_MIGHTY_EAGLE_AVAILABLE"),
                                     &owner->listener, error))
            return false;
        owner->eagle_subscribed = false;
    }
    return ab_ui_level_scroll_detach(&owner->levels, error);
}

bool ab_ui_level_selection_background(AbUiWidgetContext *context, AbVec2 screen,
                                      bool free_version, AbError *error) {
    if (free_version)
        return true;
    if (!context || context->failed || !context->render_state || !isfinite(screen.x) ||
        !isfinite(screen.y) || screen.x <= 0 || screen.y <= 0)
        return ab_fail(error, "Invalid level-selection background view");
    AbUiImageRenderState *state = context->render_state;
    bool direct = state->geometry.direct_vertices;
    state->geometry =
        (AbSpriteRenderState){.scale_x = -1, .scale_y = 1, .direct_vertices = direct};
    AbUiSpriteRender sprite = {.image = text("LS_BACKGROUND").text,
                               .position = {-screen.x, 0},
                               .size = {ceilf(screen.x / 2), screen.y},
                               .vertical = AB_ANCHOR_TOP,
                               .horizontal = AB_ANCHOR_LEFT,
                               .sized = true};
    if (!ab_ui_sprite_render(&sprite, state, &context->render, context->draw_list,
                             error))
        return false;
    state->geometry.scale_x = 1;
    sprite.position.x = 0;
    sprite.size.x = floorf(screen.x / 2);
    if (!ab_ui_sprite_render(&sprite, state, &context->render, context->draw_list,
                             error))
        return false;
    state->geometry =
        (AbSpriteRenderState){.scale_x = 1, .scale_y = 1, .direct_vertices = direct};
    return true;
}
