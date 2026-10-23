#include "ab/ui_level_golden.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static AbDataValue key(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, key(name));
    return value ? *value : (AbDataValue){0};
}

static AbUiPointerValue text(AbSpan value) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = value};
}

static AbUiPointerValue number(float value) {
    AbUiPointerValue output = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&output.number_bits, &value, sizeof(value));
    return output;
}

static AbUiPointerValue boolean(bool value) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = value};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Missing Golden Eggs layout number");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite Golden Eggs layout number");
}

static bool table(AbDataValue value, const AbDataTable **output, AbError *error) {
    if (value.kind != AB_DATA_TABLE || !value.table ||
        value.table->count > value.table->capacity ||
        (value.table->count && !value.table->entries))
        return ab_fail(error, "Missing Golden Eggs catalog table");
    *output = value.table;
    return true;
}

static bool completed(AbUiLevelGolden *owner, AbUiLevelButton *button, AbError *error) {
    AbDataValue name = field(button->metadata, "name");
    const AbDataValue *score =
        ab_data_get(ab_settings_statistics_root(owner->highscores), name);
    AbDataValue finished = {0};
    if (score && truthy(*score)) {
        const AbDataTable *record = NULL;
        if (!table(*score, &record, error))
            return false;
        finished = field(record, "completed");
    }
    button->stars.frame->fields.visible = boolean(truthy(finished));
    return true;
}

bool ab_ui_level_golden_page(AbUiLevelGolden *owner, AbUiWidget *scroll,
                             const AbDataTable *page, AbSettingsStatistics *highscores,
                             AbError *error) {
    if (!owner || !scroll || !scroll->initialized || !page || !highscores ||
        owner->count > AB_UI_LEVEL_GOLDEN_LIMIT ||
        (owner->scroll && (owner->scroll != scroll || owner->highscores != highscores)))
        return ab_fail(error, "Invalid Golden Eggs layout bindings");
    owner->scroll = scroll;
    owner->highscores = highscores;
    const AbDataTable *levels = NULL;
    float page_index = 0;
    if (!table(field(page, "levels"), &levels, error) ||
        !numeric(field(page, "index"), &page_index, error) || page_index < 1 ||
        page_index > 16777216 || floorf(page_index) != page_index ||
        levels->count > AB_UI_LEVEL_GOLDEN_LIMIT - owner->count)
        return ab_fail(error, "Golden Eggs page exceeds its layout storage");
    for (size_t index = 1; index <= levels->count; ++index) {
        const AbDataValue *level = ab_data_get(
            levels, (AbDataValue){.kind = AB_DATA_NUMBER,
                                  .bits = number((float)index).number_bits});
        size_t slot = owner->count;
        if (!level || !table(*level, owner->levels + slot, error))
            return false;
        int length = snprintf(owner->names[slot], sizeof(owner->names[slot]),
                              "starEffect%.0f-%zu", (double)page_index, index);
        if (length < 0 || (size_t)length >= sizeof(owner->names[slot]))
            return ab_fail(error, "Golden Eggs halo name exceeds storage");
        AbUiImageArguments image = {.frame.name = text(span(owner->names[slot])),
                                    .image = text(span("GOLDEN_EGG_STAR_EFFECT"))};
        if (!ab_ui_widget_init(owner->halos + slot, scroll->context, AB_UI_WIDGET_IMAGE,
                               (AbUiWidgetChildren){0}, NULL, &image, NULL, error) ||
            !ab_ui_widget_add(scroll, owner->halos + slot, error))
            return false;
        /* createPage stores halo_scale in an unused field named scale; layout
         * assigns the actual scaleX/scaleY through Scaling.setScaleTo. */
        ++owner->count;
    }
    return true;
}

bool ab_ui_level_golden_button(AbUiLevelButton *button, AbUiWidgetContext *context,
                               const AbDataTable *level, AbUiLevelButtonHost host,
                               AbError *error) {
    if (!ab_ui_level_button_base_init(button, context, level, host, error))
        return false;
    AbDataValue image = field(level, "completed_sprite_override");
    if (!truthy(image)) {
        const AbDataTable *params = NULL;
        if (!table(field(button->page_metadata, "layout_params"), &params, error))
            return false;
        image = field(params, "completed_sprite");
    }
    if (image.kind != AB_DATA_STRING)
        return ab_fail(error, "Missing Golden Eggs completed-star sprite");
    AbUiImageArguments star = {
        .frame = {.name = text(span("star")), .visible = boolean(false)},
        .image = text(image.string)};
    return ab_ui_widget_init(&button->stars, context, AB_UI_WIDGET_IMAGE,
                             (AbUiWidgetChildren){0}, NULL, &star, NULL, error) &&
           ab_ui_widget_add(&button->widget, &button->stars, error);
}

static AbUiLevelButton *find_button(AbUiLevelGolden *owner, size_t halo,
                                    AbUiLevelButton *buttons, size_t count,
                                    AbError *error) {
    for (size_t index = 0; index < count; ++index)
        if (buttons[index].metadata == owner->levels[halo])
            return buttons + index;
    ab_fail(error, "Golden Eggs halo has no canonical level button");
    return NULL;
}

bool ab_ui_level_golden_refresh(AbUiLevelGolden *owner, AbUiLevelButton *buttons,
                                size_t count, AbError *error) {
    if (!owner || !buttons || (owner->count && (!owner->scroll || !owner->highscores)))
        return ab_fail(error, "Missing Golden Eggs refresh owner");
    for (size_t index = 0; index < owner->count; ++index) {
        AbUiLevelButton *button = find_button(owner, index, buttons, count, error);
        AbDataValue unlocked, played;
        if (!button)
            return false;
        AbDataValue name = field(button->metadata, "name");
        if (name.kind != AB_DATA_STRING ||
            !ab_settings_golden_egg_unlocked(owner->highscores, name.string, &unlocked,
                                             error))
            return false;
        bool open = truthy(unlocked);
        if (!ab_ui_image_button_state_enable(&button->widget.state, boolean(open),
                                             &button->widget.context->images, error))
            return false;
        button->widget.frame->fields.visible =
            boolean(open || field(button->metadata, "disabled_button_override").kind !=
                                AB_DATA_NIL);
        if (open) {
            if (!ab_settings_golden_egg_played(owner->highscores, name.string, &played,
                                               error))
                return false;
            owner->halos[index].frame->fields.visible = boolean(!truthy(played));
            if (!completed(owner, button, error))
                return false;
        } else {
            owner->halos[index].frame->fields.visible = boolean(false);
        }
    }
    return true;
}

bool ab_ui_level_golden_layout(AbUiLevelGolden *owner, AbUiLevelButton *buttons,
                               size_t count, bool arrows, AbVec2 margin,
                               AbError *error) {
    if (!owner || !buttons || (owner->count && !owner->scroll))
        return ab_fail(error, "Missing Golden Eggs position owner");
    for (size_t index = 0; index < owner->count; ++index) {
        AbUiLevelButton *button = find_button(owner, index, buttons, count, error);
        if (!button)
            return false;
        AbUiScaling *scaling = button->host.scaling;
        AbUiScalingEnvironment view;
        if (!scaling->host.environment(scaling->host.context, &view, error))
            return false;
        float border = margin.x;
        if (arrows) {
            AbVec2 arrow;
            if (!ab_ui_scaling_sprite(scaling, &button->widget.context->images,
                                      span("BUTTON_ARROW_LEFT"), false, &arrow, error))
                return false;
            border = arrow.x + margin.x;
        }
        float right = view.screen.x - border, width = right - border;
        AbDataValue vx = field(button->metadata, "x");
        AbDataValue vy = field(button->metadata, "y");
        float x = 0.5f, y = 0.5f;
        if ((truthy(vx) && !numeric(vx, &x, error)) ||
            (truthy(vy) && !numeric(vy, &y, error)))
            return false;
        x = (((float)button->page - 1) * view.screen.x + x * width) + border;
        y = y * view.screen.y;
        button->widget.frame->fields.x = number(x);
        button->widget.frame->fields.y = number(y);
        if (!completed(owner, button, error))
            return false;
        owner->halos[index].frame->fields.x = number(x);
        owner->halos[index].frame->fields.y = number(y);
        if (!ab_ui_scaling_set_to(scaling, &button->widget.frame->fields, error) ||
            !ab_ui_scaling_set_to(scaling, &owner->halos[index].frame->fields, error))
            return false;
    }
    return true;
}

bool ab_ui_level_golden_update(AbUiLevelGolden *owner, float seconds, AbError *error) {
    if (!owner || !isfinite(seconds))
        return ab_fail(error, "Invalid Golden Eggs update clock");
    for (size_t index = 0; index < owner->count; ++index) {
        AbUiFrameFields *frame = &owner->halos[index].frame->fields;
        float angle = 0;
        AbDataValue current = {.kind = AB_DATA_NUMBER,
                               .bits = frame->angle.number_bits};
        if (frame->angle.kind != AB_UI_POINTER_NUMBER ||
            !numeric(current, &angle, error))
            return false;
        angle += seconds * 0.6f;
        if (!isfinite(angle))
            return ab_fail(error, "Golden Eggs halo angle overflow");
        frame->angle = number(angle);
    }
    return true;
}
