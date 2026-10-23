#include "ab/ui_level_scroll.h"

#include <math.h>
#include <string.h>

static AbUiPointerValue text(const char *name) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                              .text = {(const uint8_t *)name, strlen(name)}};
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

static bool named(AbUiPointerValue value, const char *name) {
    return value.kind == AB_UI_POINTER_TEXT && value.text.size == strlen(name) &&
           !memcmp(value.text.data, name, value.text.size);
}

static bool real(AbUiPointerValue value, float *output, AbError *error) {
    if (value.kind != AB_UI_POINTER_NUMBER)
        return ab_fail(error, "Missing numeric level-scroll geometry");
    memcpy(output, &value.number_bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite level-scroll geometry");
}

static AbDataValue number(size_t index) {
    float value = (float)index;
    AbDataValue output = {.kind = AB_DATA_NUMBER};
    memcpy(&output.bits, &value, sizeof(value));
    return output;
}

static bool dense(const AbDataTable *table, size_t *output, AbError *error) {
    if (!table || table->count > table->capacity || table->count > UINT32_C(16777216) ||
        (table->count && !table->entries))
        return ab_fail(error, "Invalid level-scroll source array");
    for (size_t index = 1; index <= table->count; ++index)
        if (!ab_data_get(table, number(index)))
            return ab_fail(error, "Level-scroll source array is not dense");
    *output = table->count;
    return true;
}

static bool child(const AbDataTable *table, AbDataValue index,
                  const AbDataTable **output, AbError *error) {
    const AbDataValue *value = ab_data_get(table, index);
    if (!value || value->kind != AB_DATA_TABLE || !value->table)
        return ab_fail(error, "Missing level-scroll source catalog table");
    *output = value->table;
    return true;
}

static bool valid(AbUiLevelScroll *owner, AbError *error) {
    return owner && owner->scroll.widget.initialized && owner->scroll.widget.context &&
                   owner->count <= AB_UI_LEVEL_SCROLL_BUTTONS &&
                   owner->page_count <= AB_UI_LEVEL_SCROLL_PAGES &&
                   !owner->scroll.widget.context->failed
               ? true
               : ab_fail(error, "Invalid or failed LevelScrollFrame");
}

static bool finish(AbUiLevelScroll *owner, bool okay) {
    if (!okay && owner && owner->scroll.widget.context)
        owner->scroll.widget.context->failed = true;
    return okay;
}

static AbDataValue event_id(const AbUiLevelScroll *owner, const char *name) {
    return field(owner->host.event_ids, name);
}

static bool event_matches(AbDataValue value, AbDataValue expected) {
    AbDataEntry entry = {expected, {.kind = AB_DATA_BOOLEAN, .bits = 1}};
    AbDataTable singleton = {&entry, 1, 1, NULL};
    return ab_data_get(&singleton, value) != NULL;
}

static bool facebook_page(const AbDataTable *page) {
    AbDataValue layout = field(page, "layout");
    return layout.kind == AB_DATA_STRING && layout.string.size == 8 &&
           layout.string.data && !memcmp(layout.string.data, "facebook", 8);
}

static bool golden_page(const AbDataTable *page) {
    AbDataValue layout = field(page, "layout");
    return layout.kind == AB_DATA_STRING && layout.string.size == 10 &&
           layout.string.data && !memcmp(layout.string.data, "goldeneggs", 10);
}

static bool age_filter(AbUiLevelScroll *owner, bool *output, AbError *error) {
    return owner->host.age_filter
               ? owner->host.age_filter(owner->host.context, output, error)
               : ab_fail(error, "Facebook page requires the live settings age filter");
}

static bool visible_image(AbUiLevelScroll *owner, AbUiWidget *image, float screen_width,
                          float current, AbError *error) {
    if (!ab_ui_pointer_value_truthy(&image->frame->fields.visible))
        return true;
    float x, width;
    if (!real(image->frame->fields.x, &x, error) ||
        !real(image->frame->fields.w, &width, error))
        return false;
    float relative = x - current;
    if (relative < -width || screen_width + 0.5f * width < relative)
        return true;
    if (owner->image_count == AB_UI_LEVEL_SCROLL_BUTTONS * 5)
        return ab_fail(error, "Level layout exceeds the visible image cache");
    owner->visible_images[owner->image_count++] = (AbUiLevelVisible){.image = image};
    return true;
}

bool ab_ui_level_scroll_visible(AbUiLevelScroll *owner, AbError *error) {
    if (!valid(owner, error))
        return false;
    AbUiScalingEnvironment view = {0};
    AbUiScaling *scaling = owner->host.buttons.scaling;
    if (!scaling->host.environment(scaling->host.context, &view, error))
        return finish(owner, false);
    float current = 0;
    if (!real(owner->scroll.widget.scroll[0], &current, error))
        return finish(owner, false);
    owner->image_count = owner->text_count = 0;
    bool layout_added[AB_UI_LEVEL_SCROLL_PAGES] = {0};
    for (size_t index = 0; index < owner->count; ++index) {
        AbUiLevelButton *button = owner->buttons + index;
        for (size_t page = 0; page < owner->page_count; ++page)
            if (!layout_added[page] && button->page == page + 1) {
                if (owner->facebook[page].present &&
                    !visible_image(owner, &owner->facebook[page].like, view.screen.x,
                                   current, error))
                    return finish(owner, false);
                for (size_t halo = 0; halo < owner->golden.count; ++halo)
                    if (event_matches(field(owner->golden.levels[halo], "page"),
                                      number(page + 1)) &&
                        !visible_image(owner, owner->golden.halos + halo, view.screen.x,
                                       current, error))
                        return finish(owner, false);
                layout_added[page] = true;
            }
        AbUiFrameFields *frame = &button->widget.frame->fields;
        if (frame->visible.kind != AB_UI_POINTER_BOOLEAN || !frame->visible.boolean)
            continue;
        float x = 0, y = 0, width = 0;
        if (!real(frame->x, &x, error) || !real(frame->y, &y, error) ||
            !real(frame->w, &width, error))
            return finish(owner, false);
        float relative = x - current;
        if (relative < -width || view.screen.x + 0.5f * width < relative)
            continue;
        if (owner->image_count == AB_UI_LEVEL_SCROLL_BUTTONS * 5)
            return finish(owner, ab_fail(error, "Level image cache exceeds storage"));
        owner->visible_images[owner->image_count++] =
            (AbUiLevelVisible){.image = &button->widget};
        for (size_t child_index = 0; child_index < button->widget.frame->child_count;
             ++child_index) {
            AbUiFrameNode *node = button->widget.children.frames[child_index];
            AbUiWidget *child = node ? node->context : NULL;
            if (!child || !child->initialized || &child->tree != node)
                return finish(owner, ab_fail(error, "Foreign level-scroll child"));
            if (!ab_ui_pointer_value_truthy(&child->frame->fields.visible))
                continue;
            AbUiLevelVisible entry = {.parent = &button->widget, .position = {x, y}};
            if (child == &button->number.widget) {
                entry.text = &button->number;
                if (owner->text_count == AB_UI_LEVEL_SCROLL_BUTTONS)
                    return finish(owner,
                                  ab_fail(error, "Level text cache exceeds storage"));
                owner->visible_text[owner->text_count++] = entry;
            } else {
                entry.image = child;
                if (owner->image_count == AB_UI_LEVEL_SCROLL_BUTTONS * 5)
                    return finish(owner,
                                  ab_fail(error, "Level image cache exceeds storage"));
                owner->visible_images[owner->image_count++] = entry;
            }
        }
    }
    return true;
}

bool ab_ui_level_scroll_refresh(AbUiLevelScroll *owner, AbError *error) {
    if (!valid(owner, error))
        return false;
    for (size_t index = 0; index < owner->count; ++index) {
        AbUiLevelButton *button = owner->buttons + index;
        bool open = false;
        if (golden_page(button->page_metadata))
            continue;
        if (facebook_page(button->page_metadata)) {
            bool filtered;
            if (!age_filter(owner, &filtered, error) ||
                !ab_ui_level_facebook_open(button, filtered, error))
                return finish(owner, false);
            continue;
        }
        if (!ab_level_progress_grid_open(owner->host.progress, button->metadata, &open,
                                         error))
            return finish(owner, false);
        open = open && !truthy(field(button->page_metadata, "locked"));
        if (!(open ? ab_ui_level_button_open(button, error)
                   : ab_ui_level_button_close(button, error)))
            return finish(owner, false);
    }
    return ab_ui_level_golden_refresh(&owner->golden, owner->buttons, owner->count,
                                      error) &&
           ab_ui_level_scroll_visible(owner, error);
}

static bool event(void *context, AbDataTable *data, AbError *error) {
    AbUiLevelScroll *owner = context;
    AbDataValue id = field(data, "id");
    bool sync = event_matches(id, event_id(owner, "EID_SYNC_NEW_DATA")) ||
                event_matches(id, event_id(owner, "EID_ROVIO_ID_LOGOUT_SUCCESS"));
    bool purchased = event_matches(id, event_id(owner, "EID_LEVEL_PURCHASED_SUCCEED"));
    if ((sync || purchased) && !ab_ui_level_scroll_refresh(owner, error))
        return false;
    if (sync && owner->host.progress->freemium_china) {
        if (!owner->host.layout_root)
            return finish(owner, ab_fail(error, "Missing reached China root layout"));
        return finish(owner, owner->host.layout_root(owner->host.context, error));
    }
    return true;
}

static bool layout(AbUiLevelScroll *owner, AbError *error) {
    if (!owner->base_methods.lifecycle(owner->base_methods.context,
                                       &owner->scroll.widget, AB_UI_FRAME_LAYOUT,
                                       error) ||
        !ab_ui_scroll_frame_set_current(&owner->scroll, owner->current_selection,
                                        error))
        return false;
    for (size_t index = 0; index < owner->count; ++index)
        if (!facebook_page(owner->buttons[index].page_metadata) &&
            !golden_page(owner->buttons[index].page_metadata) &&
            !ab_ui_level_button_grid_layout(owner->buttons + index, owner->host.arrows,
                                            owner->host.margin, error))
            return false;
    for (size_t page = 0; page < owner->page_count; ++page)
        if (owner->facebook[page].page &&
            !ab_ui_level_facebook_layout(owner->facebook + page, owner->buttons,
                                         owner->count, error))
            return false;
    return ab_ui_level_golden_layout(&owner->golden, owner->buttons, owner->count,
                                     owner->host.arrows, owner->host.margin, error) &&
           ab_ui_level_scroll_visible(owner, error);
}

static bool lifecycle(void *context, AbUiWidget *widget, AbUiFrameMethod method,
                      AbError *error) {
    AbUiScrollFrame *scroll = context;
    AbUiLevelScroll *owner = scroll->derived_context;
    if (method == AB_UI_FRAME_LAYOUT)
        return layout(owner, error);
    if (!owner->base_methods.lifecycle(context, widget, method, error))
        return false;
    if (method == AB_UI_FRAME_ENTRY) {
        for (size_t page = 0; page < owner->page_count; ++page)
            if (owner->facebook[page].page) {
                bool filtered;
                if (!age_filter(owner, &filtered, error))
                    return false;
                ab_ui_level_facebook_entry(owner->facebook + page, filtered);
            }
        if (!ab_ui_level_scroll_refresh(owner, error) ||
            !ab_event_manager_add(owner->host.scroll.events,
                                  event_id(owner, "EID_SYNC_NEW_DATA"),
                                  &owner->listener, error))
            return false;
        owner->sync_subscribed = true;
    } else if (method == AB_UI_FRAME_EXIT && owner->sync_subscribed) {
        if (!ab_event_manager_remove(owner->host.scroll.events,
                                     event_id(owner, "EID_SYNC_NEW_DATA"),
                                     &owner->listener, error))
            return false;
        owner->sync_subscribed = false;
    }
    return true;
}

static bool update(void *context, AbUiWidget *widget, float seconds, float time,
                   AbError *error) {
    AbUiScrollFrame *scroll = context;
    AbUiLevelScroll *owner = scroll->derived_context;
    float previous = 0, current = 0;
    if (!ab_event_timer_update(&owner->timer, seconds, error) ||
        !ab_ui_level_golden_update(&owner->golden, seconds, error) ||
        !real(widget->scroll[0], &previous, error) ||
        !owner->base_methods.update(context, widget, seconds, time, error) ||
        !real(widget->scroll[0], &current, error))
        return false;
    return previous == current || ab_ui_level_scroll_visible(owner, error);
}

static bool pointer(void *context, AbUiWidget *widget, AbSpan action, AbVec2 point,
                    AbUiPointerReply *reply, AbError *error) {
    AbUiScrollFrame *scroll = context;
    AbUiLevelScroll *owner = scroll->derived_context;
    AbUiPointerValue name = {.kind = AB_UI_POINTER_TEXT, .text = action};
    if (!named(name, "LHOLD"))
        return owner->base_methods.pointer(context, widget, action, point, reply,
                                           error);
    float previous = 0, current = 0;
    if (!real(widget->scroll[0], &previous, error) ||
        !owner->base_methods.pointer(context, widget, action, point, reply, error) ||
        !real(widget->scroll[0], &current, error))
        return false;
    return previous == current || ab_ui_level_scroll_visible(owner, error);
}

bool ab_ui_level_scroll_init(AbUiLevelScroll *owner, AbUiWidgetContext *context,
                             AbDataValue episode, AbUiLevelScrollHost host,
                             AbError *error) {
    if (!owner || owner->scroll.widget.initialized || !context || !host.progress ||
        !host.event_ids || !host.buttons.episodes || !host.buttons.scaling ||
        !host.buttons.fonts || !host.buttons.statistics || !host.scroll.events ||
        host.progress->episodes != host.buttons.episodes ||
        host.buttons.statistics->episodes != host.buttons.episodes)
        return ab_fail(error, "Missing fresh LevelScrollFrame storage/host");
    owner->host = host;
    owner->episode = episode;
    owner->current_selection = 1;
    const AbDataTable *record = NULL;
    if (!child(host.buttons.episodes, episode, &record, error) ||
        !child(record, key("pages"), &owner->pages, error) ||
        !dense(owner->pages, &owner->page_count, error))
        return false;
    if (!owner->page_count || owner->page_count > AB_UI_LEVEL_SCROLL_PAGES)
        return ab_fail(error, "LevelScrollFrame exceeds page storage");
    if (!ab_ui_scroll_frame_init(
            &owner->scroll, context,
            (AbUiWidgetChildren){owner->frames, owner->draw, owner->pointer,
                                 AB_UI_LEVEL_SCROLL_CHILDREN},
            owner->anchors, AB_UI_LEVEL_SCROLL_PAGES, NULL, host.scroll, error) ||
        !ab_event_timer_init(&owner->timer, NULL, 0, host.initial_time, error))
        return false;
    owner->base_methods = owner->scroll.widget.methods;
    owner->scroll.derived_context = owner;
    owner->scroll.widget.methods.lifecycle = lifecycle;
    owner->scroll.widget.methods.update = update;
    owner->scroll.widget.methods.pointer = pointer;
    for (size_t page_index = 1; page_index <= owner->page_count; ++page_index) {
        const AbDataTable *page = NULL, *levels = NULL;
        if (!child(owner->pages, number(page_index), &page, error))
            return finish(owner, false);
        AbDataValue selected = field(page, "layout");
        bool facebook = facebook_page(page);
        bool golden = golden_page(page);
        bool grid = selected.kind == AB_DATA_STRING && selected.string.size == 4 &&
                    selected.string.data && !memcmp(selected.string.data, "grid", 4);
        if ((!facebook && !grid && !golden) || truthy(field(page, "locked")) ||
            truthy(field(page, "fill_grid_with_placeholders")))
            return finish(owner,
                          ab_fail(error, "LevelScrollFrame requires a recovered page "
                                         "layout without locked/placeholder signs"));
        if (facebook) {
            bool filtered;
            if (!age_filter(owner, &filtered, error) ||
                !ab_ui_level_facebook_init(owner->facebook + page_index - 1,
                                           &owner->scroll.widget, page,
                                           host.buttons.scaling, filtered, error))
                return finish(owner, false);
        }
        if (golden && !ab_ui_level_golden_page(&owner->golden, &owner->scroll.widget,
                                               page, host.golden_eggs, error))
            return finish(owner, false);
        size_t count = 0;
        if (!child(page, key("levels"), &levels, error) ||
            !dense(levels, &count, error))
            return finish(owner, false);
        if (count > AB_UI_LEVEL_SCROLL_BUTTONS - owner->count)
            return finish(owner,
                          ab_fail(error, "LevelScrollFrame exceeds button storage"));
        for (size_t index = 1; index <= count; ++index) {
            const AbDataTable *level = NULL;
            AbUiLevelButton *button = owner->buttons + owner->count;
            if (!child(levels, number(index), &level, error))
                return finish(owner, false);
            bool initialized = golden ? ab_ui_level_golden_button(
                                            button, context, level, host.buttons, error)
                                      : ab_ui_level_button_init(button, context, level,
                                                                host.buttons, error);
            if (!initialized ||
                !ab_ui_widget_add(&owner->scroll.widget, &button->widget, error))
                return finish(owner, false);
            ++owner->count;
        }
    }
    owner->listener = (AbEventListener){.context = owner, .event_triggered = event};
    if (host.progress->freemium_china) {
        if (!ab_event_manager_add(host.scroll.events,
                                  event_id(owner, "EID_LEVEL_PURCHASED_SUCCEED"),
                                  &owner->listener, error))
            return finish(owner, false);
        owner->purchase_subscribed = true;
    }
    return true;
}

bool ab_ui_level_scroll_detach(AbUiLevelScroll *owner, AbError *error) {
    if (!owner)
        return true;
    if (owner->sync_subscribed) {
        if (!ab_event_manager_remove(owner->host.scroll.events,
                                     event_id(owner, "EID_SYNC_NEW_DATA"),
                                     &owner->listener, error))
            return false;
        owner->sync_subscribed = false;
    }
    if (owner->purchase_subscribed) {
        if (!ab_event_manager_remove(owner->host.scroll.events,
                                     event_id(owner, "EID_LEVEL_PURCHASED_SUCCEED"),
                                     &owner->listener, error))
            return false;
        owner->purchase_subscribed = false;
    }
    return ab_ui_scroll_frame_detach(&owner->scroll, error);
}
