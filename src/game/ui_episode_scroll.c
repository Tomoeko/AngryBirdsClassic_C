#include "ab/ui_episode_scroll.h"

#include "ab/data_order.h"
#include "ab/ui_value_text.h"

#include <math.h>
#include <string.h>

static bool layout(AbUiEpisodeScroll *owner, AbError *error);

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

static AbDataValue get(const AbDataTable *table, AbDataValue index) {
    const AbDataValue *value = ab_data_get(table, index);
    return value ? *value : (AbDataValue){0};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    return get(table, key(name));
}

static AbDataValue index_value(size_t index) {
    return (AbDataValue){.kind = AB_DATA_NUMBER,
                         .bits = number((float)index).number_bits};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind == AB_DATA_NUMBER) {
        memcpy(output, &value.bits, sizeof(*output));
        if (isfinite(*output))
            return true;
    }
    ab_fail(error, "Episode scroll requires a finite numeric field");
    return false;
}

static bool value(AbDataValue source, AbUiPointerValue *output, AbError *error) {
    if (source.kind == AB_DATA_STRING)
        *output = (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = source.string};
    else if (source.kind == AB_DATA_NUMBER)
        *output = (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER,
                                     .number_bits = source.bits};
    else
        return ab_fail(error, "Episode scroll requires numeric/text catalog values");
    return true;
}

static AbDataValue episode_id(const AbUiEpisodeScroll *owner, size_t index) {
    return get(owner->host.episode_ids, index_value(index + 1));
}

static const AbDataTable *episode(const AbUiEpisodeScroll *owner, AbDataValue id) {
    AbDataValue record = get(owner->host.buttons.episodes, id);
    return record.kind == AB_DATA_TABLE ? record.table : NULL;
}

static bool scaled(AbUiEpisodeScroll *owner, AbDataValue image, AbVec2 *output,
                   AbError *error) {
    return image.kind == AB_DATA_STRING &&
           ab_ui_scaling_sprite(owner->host.buttons.scaling,
                                &owner->scroll.widget.context->images, image.string,
                                false, output, error);
}

static bool environment(AbUiEpisodeScroll *owner, AbUiScalingEnvironment *output,
                        AbError *error) {
    AbUiScaling *scaling = owner->host.buttons.scaling;
    return scaling && scaling->host.environment &&
           scaling->host.environment(scaling->host.context, output, error) &&
           isfinite(output->screen.x) && isfinite(output->screen.y) &&
           output->screen.x > 0 && output->screen.y > 0;
}

static bool profile(AbUiEpisodeScroll *owner, AbError *error) {
    const AbDataTable *globals = owner->host.globals;
    return !globals || truthy(field(globals, "g_hasWebBrowser")) ||
                   truthy(field(globals, "g_isAccelerometerControl")) ||
                   !truthy(field(globals, "g_use_new_layout"))
               ? ab_fail(error, "Episode scroll profile requires new layout without "
                                "browser extras or accelerometer control")
               : true;
}

static bool dimensions(AbUiEpisodeScroll *owner, AbError *error) {
    AbUiTextMetricBoundaries fonts =
        ab_ui_font_resources_boundaries(owner->host.buttons.fonts);
    if (!fonts.resources.set_font(fonts.resources.context, text("FONT_BASIC"), error))
        return false;
    float width = 0, height = 0, name_width = 0;
    for (size_t index = 0; index < owner->count; ++index) {
        const AbDataTable *record = episode(owner, episode_id(owner, index));
        AbVec2 bounds;
        AbUiPointerValue name, measured;
        if (!record)
            return ab_fail(error, "Missing episode scroll dimension metadata");
        if (!scaled(owner, field(record, "icon"), &bounds, error) ||
            !value(field(record, "name"), &name, error) ||
            !fonts.resources.measure(fonts.resources.context, name, &measured, error))
            return false;
        float label;
        if (measured.kind != AB_UI_POINTER_NUMBER ||
            !numeric(
                (AbDataValue){.kind = AB_DATA_NUMBER, .bits = measured.number_bits},
                &label, error))
            return false;
        width = fmaxf(width, bounds.x);
        height = fmaxf(height, bounds.y);
        name_width = fmaxf(name_width, label);
    }
    owner->button_dimensions =
        (AbVec2){fmaxf(name_width * 0.8f, width * 0.95f), height * 1.7f};
    return true;
}

static bool add_cards(AbUiEpisodeScroll *owner, AbError *error) {
    AbDataEntry strings[AB_UI_EPISODE_SCROLL_CAPACITY];
    size_t string_count = 0, numeric_count = 0;
    size_t numeric_indices[AB_UI_EPISODE_SCROLL_CAPACITY] = {0};
    for (size_t index = 0; index < owner->count; ++index) {
        AbDataValue id = episode_id(owner, index);
        if (id.kind == AB_DATA_NUMBER) {
            float number_id;
            if (!numeric(id, &number_id, error) || number_id < 1 ||
                number_id > AB_UI_EPISODE_SCROLL_CAPACITY ||
                truncf(number_id) != number_id ||
                numeric_indices[(size_t)number_id - 1])
                return ab_fail(error, "Invalid episode scroll numeric catalog key");
            numeric_indices[(size_t)number_id - 1] = index + 1;
            ++numeric_count;
        } else if (id.kind == AB_DATA_STRING) {
            strings[string_count++] = (AbDataEntry){id, index_value(index + 1)};
        } else {
            return ab_fail(error, "Invalid episode scroll catalog key");
        }
    }
    for (size_t index = 0; index < numeric_count; ++index)
        if (!numeric_indices[index] ||
            !ab_ui_widget_add(&owner->scroll.widget,
                              &owner->buttons[numeric_indices[index] - 1].widget,
                              error))
            return ab_fail(error, "Episode scroll numeric keys are not dense");
    /* Source pairs over the temporary card dictionary visits its dense numeric
     * array first, then its growing string hash. The target has 3 or 4 strings. */
    AbDataTable table = {.entries = strings,
                         .count = string_count,
                         .capacity = AB_UI_EPISODE_SCROLL_CAPACITY};
    size_t indices[AB_UI_EPISODE_SCROLL_CAPACITY], capacity;
    AbStringOrderNode nodes[32];
    AbStringOrderWorkspace workspace = {indices, AB_UI_EPISODE_SCROLL_CAPACITY, nodes,
                                        32};
    if (!ab_data_string_order_growing(&table, &workspace, &capacity, error))
        return false;
    for (size_t index = 0; index < string_count; ++index) {
        float card_index;
        if (!numeric(strings[indices[index]].value, &card_index, error) ||
            !ab_ui_widget_add(&owner->scroll.widget,
                              &owner->buttons[(size_t)card_index - 1].widget, error))
            return false;
    }
    return true;
}

static bool create_cards(AbUiEpisodeScroll *owner, AbError *error) {
    if (!profile(owner, error))
        return false;
    AbUiWidget *reference = NULL;
    for (size_t index = 0; index < owner->count; ++index) {
        AbUiEpisodeButton *button = owner->buttons + index;
        AbDataValue id = episode_id(owner, index);
        const AbDataTable *record = episode(owner, id);
        AbDataValue subclass = field(record, "episode_selection_button_subclass");
        AbUiEpisodeButtonClass class_kind = AB_UI_EPISODE_STANDARD;
        if (subclass.kind != AB_DATA_NIL) {
            AbDataEntry names[] = {
                {key("EggDefenderEpisodeButton"),
                 index_value(AB_UI_EPISODE_EGG_DEFENDER)},
                {key("GoldenEggButton"), index_value(AB_UI_EPISODE_GOLDEN_EGGS)},
                {key("EpisodeButtonUGC"), index_value(AB_UI_EPISODE_UGC)}};
            AbDataTable classes = {.entries = names, .count = 3};
            const AbDataValue *selected = ab_data_get(&classes, subclass);
            float selected_class;
            if (!selected || !numeric(*selected, &selected_class, error))
                return ab_fail(error, "Unsupported declared episode-card subclass");
            class_kind = (AbUiEpisodeButtonClass)selected_class;
        }
        if (!record ||
            !ab_ui_episode_button_init_class(button, owner->scroll.widget.context, NULL,
                                             owner->host.buttons, class_kind, error))
            return false;
        AbUiPointerValue ui_id;
        AbSpan suffix;
        uint8_t temporary[32];
        if (!value(id, &ui_id, error) ||
            !ab_ui_value_text(ui_id, temporary, sizeof(temporary), &suffix) ||
            suffix.size > sizeof(owner->names[index]) - 7)
            return ab_fail(error, "Episode scroll name exceeds storage");
        memcpy(owner->names[index], "episode", 7);
        memcpy(owner->names[index] + 7, suffix.data, suffix.size);
        button->widget.frame->fields.name = (AbUiPointerValue){
            .kind = AB_UI_POINTER_TEXT, .text = {owner->names[index], 7 + suffix.size}};
        if (!ab_ui_episode_button_set_episode(button, ui_id, record, error))
            return false;
        button->widget.state.return_value = text("GOTO_LEVEL_SELECTION");
        button->widget.state.disabled_return_value = text("LOCKED_LEVEL_SELECTION");
        if (!ab_ui_scaling_set_to(owner->host.buttons.scaling,
                                  &button->widget.frame->fields, error))
            return false;
        if (id.kind == AB_DATA_NUMBER && id.bits == UINT32_C(0x3f800000))
            reference = &button->widget;
        if (truthy(field(record, "disableEpisodeButtonScores"))) {
            AbUiWidget *hidden[] = {
                &button->score_box,   &button->score.widget,
                &button->star_box,    &button->stars.widget,
                &button->feather_box, &button->feather_score.widget};
            for (size_t item = 0; item < sizeof(hidden) / sizeof(hidden[0]); ++item) {
                if (!hidden[item]->initialized)
                    return ab_fail(error, "Episode score gate reached a missing child");
                hidden[item]->frame->fields.visible =
                    (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
            }
        }
    }
    if (!dimensions(owner, error))
        return false;
    for (size_t index = 0; index < owner->count; ++index)
        owner->buttons[index].reference_button = reference;
    if (!add_cards(owner, error))
        return false;
    return !ab_ui_pointer_value_truthy(&owner->scroll.widget.tree.entered) ||
           layout(owner, error);
}

static bool unlock_cards(AbUiEpisodeScroll *owner, AbError *error) {
    for (size_t index = 0; index < owner->count; ++index) {
        bool open;
        if (!ab_episode_progress_open(owner->host.progress, episode_id(owner, index),
                                      &open, error) ||
            !ab_ui_episode_button_set_open(
                owner->buttons + index,
                (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = open},
                error))
            return false;
    }
    return true;
}

static bool arrange(AbUiEpisodeScroll *owner, bool position_cards, AbError *error) {
    if (!profile(owner, error))
        return false;
    AbUiScalingEnvironment view;
    if (!environment(owner, &view, error))
        return ab_fail(error, "Missing episode scroll screen dimensions");
    bool arrows =
        truthy(field(owner->host.globals, "g_episode_selection_use_scroll_buttons"));
    float arrow_width = 0, border = 0;
    if (arrows) {
        AbVec2 arrow;
        AbDataValue margins = field(owner->host.globals, "g_layoutMargin");
        float margin;
        if (!scaled(owner, key("BUTTON_ARROW_LEFT"), &arrow, error) ||
            margins.kind != AB_DATA_TABLE ||
            !numeric(field(margins.table, "x"), &margin, error))
            return ab_fail(error, "Missing episode scroll arrow margin");
        arrow_width = arrow.x;
        border = arrow_width + margin;
    }
    AbVec2 bounds = {0};
    bool first = truthy(
        field(owner->host.globals, "g_episode_selection_use_first_card_width_for_all"));
    const AbDataTable *catalog = owner->host.buttons.episodes;
    size_t count = first ? 1 : catalog->count;
    for (size_t index = 0; index < count; ++index) {
        const AbDataTable *record = first ? episode(owner, index_value(1))
                                          : catalog->entries[index].value.table;
        AbDataValue image = field(record, "button");
        if (!truthy(image))
            continue;
        AbVec2 size;
        if (!scaled(owner, image, &size, error))
            return ab_fail(error, "Missing episode scroll card bound");
        bounds.x = fmaxf(bounds.x, size.x);
        bounds.y = fmaxf(bounds.y, size.y);
    }
    AbDataValue padding =
        field(owner->host.globals, "g_episode_selection_button_padding");
    if (truthy(padding)) {
        float factor;
        if (!numeric(padding, &factor, error))
            return false;
        bounds.x = bounds.x + factor * bounds.y;
    }
    if (!(bounds.x > 0))
        return ab_fail(error, "Episode scroll card width is not positive");
    float visible = fmaxf(1, floorf((view.screen.x - arrow_width * 2) / bounds.x));
    visible = fminf((float)owner->count, visible);
    float spacing = (view.screen.x - border * 2) / visible;
    size_t anchor_count = owner->count - (size_t)visible + 1;
    AbVec2 anchors[AB_UI_EPISODE_SCROLL_CAPACITY];
    for (size_t index = 0; index < anchor_count; ++index)
        anchors[index] = (AbVec2){(float)index * spacing, 0};
    AbVec2 background;
    if (!scaled(owner, key("BTN_EP1_BACKGROUND"), &background, error))
        return ab_fail(error, "Missing episode scroll reference background");
    AbUiScrollClip clip = {border, 0, view.screen.x - border * 2,
                           view.screen.y - (view.screen.y - background.y) * 0.5f};
    if (!ab_ui_scroll_frame_set_clip(&owner->scroll, &clip, error) ||
        !ab_ui_scroll_frame_set_anchors(&owner->scroll, anchors, anchor_count, error))
        return false;
    if (!position_cards)
        return true;
    if (!ab_ui_scroll_frame_set_current(&owner->scroll,
                                        owner->scroll.current_anchor < anchor_count
                                            ? owner->scroll.current_anchor
                                            : anchor_count,
                                        error))
        return false;
    bool backwards = truthy(field(owner->host.globals, "g_episodesBackwards"));
    for (size_t index = 0; index < owner->count; ++index) {
        size_t slot = backwards ? owner->count - index - 1 : index;
        AbUiFrameFields *frame = &owner->buttons[index].widget.frame->fields;
        frame->x = number(floorf(((float)slot + 0.5f) * spacing + border));
        frame->y = number(view.screen.y * 0.5f);
        if (!ab_ui_scaling_set_to(owner->host.buttons.scaling, frame, error))
            return false;
    }
    return owner->base_methods.lifecycle(
        owner->base_methods.context, &owner->scroll.widget, AB_UI_FRAME_LAYOUT, error);
}

static bool layout(AbUiEpisodeScroll *owner, AbError *error) {
    return arrange(owner, true, error);
}

static bool event(void *context, AbDataTable *data, AbError *error) {
    AbUiEpisodeScroll *owner = context;
    AbDataValue id = field(data, "id");
    AbDataEntry expected[] = {
        {owner->host.extras_changed, {.kind = AB_DATA_BOOLEAN}},
        {owner->host.free_version_unlocked, {.kind = AB_DATA_BOOLEAN}}};
    AbDataTable matches = {.entries = expected, .count = 2};
    if (!ab_data_get(&matches, id))
        return true;
    AbDataTable extras = {.entries = expected, .count = 1};
    if (ab_data_get(&extras, id)) {
        if (!ab_ui_frame_clear_children(&owner->scroll.widget.tree, error))
            return false;
        memset(owner->buttons, 0, sizeof(owner->buttons));
        if (!create_cards(owner, error))
            return false;
    }
    return unlock_cards(owner, error) && layout(owner, error);
}

static bool lifecycle(void *context, AbUiWidget *widget, AbUiFrameMethod method,
                      AbError *error) {
    AbUiScrollFrame *scroll = context;
    AbUiEpisodeScroll *owner = scroll->derived_context;
    if (method == AB_UI_FRAME_LAYOUT)
        return layout(owner, error);
    if (method == AB_UI_FRAME_ENTRY) {
        if (!unlock_cards(owner, error) ||
            !ab_event_manager_add(owner->host.scroll.events,
                                  owner->host.free_version_unlocked, &owner->listener,
                                  error))
            return false;
        owner->free_subscribed = true;
    }
    if (!owner->base_methods.lifecycle(context, widget, method, error))
        return false;
    if (method == AB_UI_FRAME_EXIT && owner->free_subscribed) {
        if (!ab_event_manager_remove(owner->host.scroll.events,
                                     owner->host.free_version_unlocked,
                                     &owner->listener, error))
            return false;
        owner->free_subscribed = false;
    }
    return method != AB_UI_FRAME_ENTRY || arrange(owner, false, error);
}

static bool update(void *context, AbUiWidget *widget, float seconds, float time,
                   AbError *error) {
    AbUiScrollFrame *scroll = context;
    AbUiEpisodeScroll *owner = scroll->derived_context;
    if (!owner->base_methods.update(context, widget, seconds, time, error))
        return false;
    if (!owner->autoscroll_on_entry)
        return true;
    if (owner->scroll_to_episode.kind == AB_DATA_NIL) {
        if (!ab_ui_scroll_frame_next(scroll, error))
            return false;
    } else {
        float target;
        if (!numeric(owner->scroll_to_episode, &target, error) || target < 1 ||
            target > (float)owner->count || truncf(target) != target)
            return ab_fail(error, "Unsupported episode autoscroll anchor");
        if (!ab_ui_scroll_frame_set_target(scroll, (size_t)target, error))
            return false;
        owner->scroll_to_episode = (AbDataValue){0};
    }
    owner->autoscroll_on_entry = false;
    return true;
}

bool ab_ui_episode_scroll_init(AbUiEpisodeScroll *owner, AbUiWidgetContext *context,
                               const AbUiScrollArguments *arguments,
                               AbUiEpisodeScrollHost host, AbError *error) {
    if (!owner || owner->scroll.widget.initialized || !host.episode_ids ||
        !host.episode_ids->count ||
        host.episode_ids->count > AB_UI_EPISODE_SCROLL_CAPACITY ||
        !host.buttons.episodes || !host.progress)
        return ab_fail(error, "Invalid episode scroll constructor");
    owner->host = host;
    owner->count = host.episode_ids->count;
    if (!ab_ui_scroll_frame_init(&owner->scroll, context,
                                 (AbUiWidgetChildren){owner->frames, owner->draw,
                                                      owner->pointer,
                                                      AB_UI_EPISODE_SCROLL_CAPACITY},
                                 owner->anchors, AB_UI_EPISODE_SCROLL_CAPACITY,
                                 arguments, host.scroll, error))
        return false;
    owner->base_methods = owner->scroll.widget.methods;
    owner->scroll.derived_context = owner;
    owner->scroll.draw_in_three_phases = true;
    owner->scroll.widget.methods.lifecycle = lifecycle;
    owner->scroll.widget.methods.update = update;
    owner->listener = (AbEventListener){.context = owner, .event_triggered = event};
    if (!create_cards(owner, error))
        return false;
    if (host.scroll.events) {
        if (!ab_event_manager_add(host.scroll.events, host.extras_changed,
                                  &owner->listener, error))
            return false;
        owner->extras_subscribed = true;
    }
    return true;
}

bool ab_ui_episode_scroll_detach(AbUiEpisodeScroll *owner, AbError *error) {
    if (!owner)
        return true;
    if (owner->extras_subscribed) {
        if (!ab_event_manager_remove(owner->host.scroll.events,
                                     owner->host.extras_changed, &owner->listener,
                                     error))
            return false;
        owner->extras_subscribed = false;
    }
    if (owner->free_subscribed) {
        if (!ab_event_manager_remove(owner->host.scroll.events,
                                     owner->host.free_version_unlocked,
                                     &owner->listener, error))
            return false;
        owner->free_subscribed = false;
    }
    return ab_ui_scroll_frame_detach(&owner->scroll, error);
}
