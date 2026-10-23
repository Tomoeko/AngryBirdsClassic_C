#include "ab/ui_level_eggs.h"

#include <math.h>
#include <string.h>

static AbDataValue key(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbUiPointerValue text(const char *value) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = key(value).string};
}

static AbUiPointerValue number(float value) {
    AbUiPointerValue output = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&output.number_bits, &value, sizeof(value));
    return output;
}

static AbDataValue data_number(size_t value) {
    return (AbDataValue){.kind = AB_DATA_NUMBER,
                         .bits = number((float)value).number_bits};
}

static AbUiPointerValue boolean(bool value) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = value};
}

static AbDataValue get(const AbDataTable *table, AbDataValue name) {
    const AbDataValue *value = ab_data_get(table, name);
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool table(AbDataValue value, const AbDataTable **output, AbError *error) {
    if (value.kind != AB_DATA_TABLE || !value.table ||
        value.table->count > value.table->capacity ||
        (value.table->count && !value.table->entries))
        return ab_fail(error, "Missing Golden Eggs screen catalog table");
    *output = value.table;
    return true;
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Missing King Pig highscore/threshold number");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite King Pig score");
}

static bool gold_level(const AbEpisodeStatistics *statistics, const AbDataTable *level,
                       bool *gold, AbError *error) {
    AbDataValue name = get(level, key("name"));
    AbDataValue score = get(statistics->highscores, name);
    if (!truthy(score)) {
        *gold = false;
        return true;
    }
    const AbDataTable *record = NULL, *limits = NULL;
    float points = 0, threshold = 0;
    if (!table(score, &record, error) ||
        !table(get(statistics->star_table, name), &limits, error) ||
        !numeric(get(record, key("score")), &points, error) ||
        !numeric(get(limits, key("goldScore")), &threshold, error))
        return false;
    if (points < threshold)
        *gold = false;
    return true;
}

static bool king_pig_available(AbUiLevelEggs *owner, bool *output, AbError *error) {
    const AbEpisodeStatistics *statistics = owner->host.statistics;
    bool gold = true;
    for (size_t episode = 1; episode <= 7; ++episode) {
        const AbDataTable *record = NULL, *pages = NULL;
        if (!table(get(statistics->episodes, data_number(episode)), &record, error) ||
            !table(get(record, key("pages")), &pages, error))
            return false;
        for (size_t page = 1; page <= pages->count; ++page) {
            const AbDataTable *metadata = NULL, *levels = NULL;
            if (!table(get(pages, data_number(page)), &metadata, error))
                return false;
            if (episode == 7 &&
                !truthy(
                    get(metadata, key("included_in_king_pig_mode_three_star_count"))))
                continue;
            if (!table(get(metadata, key("levels")), &levels, error))
                return false;
            for (size_t index = 1; index <= levels->count; ++index) {
                const AbDataTable *level = NULL;
                if (!table(get(levels, data_number(index)), &level, error) ||
                    !gold_level(statistics, level, &gold, error))
                    return false;
            }
        }
    }
    *output = gold || !owner->host.release_build;
    return true;
}

static bool layout(AbUiLevelEggs *owner, AbError *error) {
    AbUiLevelSelection *levels = owner->levels;
    AbUiLevelScroll *scroll = &levels->levels;
    bool second = false;
    for (size_t index = 0; index < scroll->count; ++index) {
        AbUiLevelButton *button = scroll->buttons + index;
        if (button->page != 2)
            continue;
        AbDataValue unlocked, name = get(button->metadata, key("name"));
        if (name.kind != AB_DATA_STRING ||
            !ab_settings_golden_egg_unlocked(owner->host.highscores, name.string,
                                             &unlocked, error))
            return false;
        if (truthy(unlocked))
            second = true;
    }
    if (!second) {
        scroll->current_selection = 1;
        if (!ab_ui_widget_lifecycle(&scroll->scroll.widget, AB_UI_FRAME_LAYOUT, error))
            return false;
        levels->right.frame->fields.visible = boolean(false);
    }
    levels->dots[0].frame->fields.visible = boolean(second);
    levels->dots[1].frame->fields.visible = boolean(second);
    AbUiScaling *scaling = levels->host.levels.buttons.scaling;
    AbUiScalingEnvironment view;
    if (!scaling->host.environment(scaling->host.context, &view, error))
        return false;
    AbVec2 anchors[] = {{0, 0}, {view.screen.x, 0}};
    if (!ab_ui_scroll_frame_set_anchors(&scroll->scroll, anchors, second ? 2 : 1,
                                        error))
        return false;
    if (owner->host.king_pig_enabled) {
        owner->king_pig.frame->fields.x = number(view.screen.x * 1.15f);
        owner->king_pig.frame->fields.y = number(view.screen.y * 0.19f);
        if (!ab_ui_scaling_set_to(scaling, &owner->king_pig.frame->fields, error))
            return false;
    }
    return true;
}

static bool lifecycle(void *context, AbUiWidget *widget, AbUiFrameMethod method,
                      AbError *error) {
    AbUiLevelEggs *owner = context;
    if (method == AB_UI_FRAME_ENTRY &&
        !owner->host.assets(owner->host.context, true, error))
        return false;
    if (!owner->inherited.lifecycle(owner->inherited.context, widget, method, error))
        return false;
    if (method == AB_UI_FRAME_EXIT)
        return owner->host.assets(owner->host.context, false, error);
    if (method == AB_UI_FRAME_LAYOUT)
        return layout(owner, error);
    if (method == AB_UI_FRAME_ENTRY && owner->host.king_pig_enabled) {
        bool available;
        if (!king_pig_available(owner, &available, error))
            return false;
        owner->king_pig.frame->fields.visible = boolean(available);
        return owner->inherited.lifecycle(owner->inherited.context, widget,
                                          AB_UI_FRAME_LAYOUT, error) &&
               layout(owner, error);
    }
    return true;
}

static bool update(void *context, AbUiWidget *widget, float seconds, float time,
                   AbError *error) {
    AbUiLevelEggs *owner = context;
    return owner->inherited.update(owner->inherited.context, widget, seconds, time,
                                   error);
}

static bool draw(void *context, AbUiWidget *widget, const AbUiDrawInput *input,
                 AbError *error) {
    AbUiLevelEggs *owner = context;
    return owner->inherited.draw(owner->inherited.context, widget, input, error);
}

static bool pointer(void *context, AbUiWidget *widget, AbSpan event, AbVec2 point,
                    AbUiPointerReply *reply, AbError *error) {
    AbUiLevelEggs *owner = context;
    return owner->inherited.pointer(owner->inherited.context, widget, event, point,
                                    reply, error);
}

bool ab_ui_level_eggs_attach(AbUiLevelEggs *owner, AbUiLevelSelection *levels,
                             AbUiLevelEggsHost host, AbError *error) {
    if (!owner || owner->initialized || !levels || !levels->widget.initialized ||
        levels->levels.page_count != 2 || !host.highscores || !host.assets ||
        !host.statistics || !levels->widget.methods.lifecycle ||
        !levels->widget.methods.update || !levels->widget.methods.draw ||
        !levels->widget.methods.pointer)
        return ab_fail(error, "Invalid Golden Eggs level-screen bindings");
    owner->levels = levels;
    owner->inherited = levels->widget.methods;
    owner->host = host;
    if (host.king_pig_enabled) {
        AbUiImageButtonArguments args = {
            .image = {.frame = {.name = text("kingpig_button"),
                                .visible = boolean(false)},
                      .image = text("MATTEL_GOLDENEGGS_SELECT")},
            .return_value = text("ACTIVATE_KINGPIG"),
            .activate_on_release = boolean(true)};
        if (!ab_ui_widget_init(&owner->king_pig, levels->widget.context,
                               AB_UI_WIDGET_IMAGE_BUTTON, (AbUiWidgetChildren){0}, NULL,
                               NULL, &args, error) ||
            !ab_ui_widget_add(&levels->levels.scroll.widget, &owner->king_pig, error))
            return false;
    }
    levels->widget.methods = (AbUiWidgetMethods){.context = owner,
                                                 .lifecycle = lifecycle,
                                                 .update = update,
                                                 .draw = draw,
                                                 .pointer = pointer};
    owner->initialized = true;
    return true;
}
