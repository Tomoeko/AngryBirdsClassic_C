#include "ab/ui_level_button.h"

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

static const AbDataValue *field(const AbDataTable *table, const char *name) {
    return ab_data_get(
        table, (AbDataValue){.kind = AB_DATA_STRING, .string = text(name).text});
}

static bool truthy(const AbDataValue *value) {
    return value && value->kind != AB_DATA_NIL &&
           (value->kind != AB_DATA_BOOLEAN || value->bits != 0);
}

static bool value(const AbDataValue *input, AbUiPointerValue *output, AbError *error) {
    if (!input || input->kind == AB_DATA_NIL)
        *output = (AbUiPointerValue){0};
    else if (input->kind == AB_DATA_BOOLEAN)
        *output = boolean(input->bits != 0);
    else if (input->kind == AB_DATA_NUMBER)
        *output = (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER,
                                     .number_bits = input->bits};
    else if (input->kind == AB_DATA_STRING)
        *output = (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = input->string};
    else
        return ab_fail(error, "Unsupported basic level-button metadata value");
    return ab_ui_pointer_value_valid(output) ||
           ab_fail(error, "Invalid basic level-button metadata value");
}

static bool numeric(const AbDataValue *value, float *output, AbError *error) {
    if (!value || value->kind != AB_DATA_NUMBER)
        return ab_fail(error, "Missing numeric level-button metadata");
    memcpy(output, &value->bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite level-button metadata");
}

static AbDataValue data_number(size_t index) {
    AbUiPointerValue converted = number((float)index);
    return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = converted.number_bits};
}

static bool positive_index(const AbDataValue *value, size_t *output, AbError *error) {
    float index = 0;
    if (!numeric(value, &index, error))
        return false;
    if (index <= 0 || index > 16777216 || floorf(index) != index)
        return ab_fail(error, "Unsupported positive level-button index");
    *output = (size_t)index;
    return true;
}

static bool child(const AbDataTable *parent, AbDataValue key,
                  const AbDataTable **output, AbError *error) {
    const AbDataValue *value = ab_data_get(parent, key);
    if (!value || value->kind != AB_DATA_TABLE || !value->table)
        return ab_fail(error, "Missing actual level-button catalog table");
    *output = value->table;
    return true;
}

static bool valid(AbUiLevelButton *button, AbError *error) {
    return button && button->widget.initialized && button->widget.context &&
                   !button->widget.context->failed
               ? true
               : ab_fail(error, "Invalid or failed basic level button");
}

static bool finish(AbUiLevelButton *button, bool okay) {
    if (!okay && button && button->widget.context)
        button->widget.context->failed = true;
    return okay;
}

static bool set_image(AbUiLevelButton *button, AbUiWidget *image, AbUiPointerValue name,
                      AbError *error) {
    return ab_ui_image_state_set_image(&image->state.image, name,
                                       &button->widget.context->images, error);
}

static bool power_image(AbUiLevelButton *button, const char *property,
                        const char *fallback, AbUiPointerValue *output,
                        AbError *error) {
    const AbDataValue *power = field(button->episode_metadata, "mightyPower");
    const AbDataValue *selected = NULL;
    if (truthy(power)) {
        if (power->kind != AB_DATA_TABLE || !power->table)
            return ab_fail(error, "Invalid level-button mighty-power record");
        selected = field(power->table, property);
    }
    if (!truthy(selected)) {
        *output = text(fallback);
        return true;
    }
    return value(selected, output, error);
}

static bool add_image(AbUiLevelButton *button, AbUiWidget *image, const char *name,
                      AbUiPointerValue resource, AbError *error) {
    AbUiImageArguments arguments = {
        .frame = {.name = text(name), .visible = boolean(false)}};
    return ab_ui_widget_init(image, button->widget.context, AB_UI_WIDGET_IMAGE,
                             (AbUiWidgetChildren){0}, NULL, &arguments, NULL, error) &&
           set_image(button, image, resource, error) &&
           ab_ui_widget_add(&button->widget, image, error);
}

static bool basic_children(AbUiLevelButton *button, AbError *error) {
    AbUiPointerValue label;
    AbSpan converted;
    if (!value(field(button->metadata, "display_number"), &label, error) ||
        !ab_ui_value_text(label, button->number_text, sizeof(button->number_text),
                          &converted))
        return ab_fail(error, "Unsupported actual level display number");
    if (truthy(field(button->metadata, "siege")))
        converted = text("S").text;
    if (!ab_ui_text_widget_init(&button->number, button->widget.context,
                                (AbUiWidgetChildren){0}, button->host.fonts, NULL, NULL,
                                error))
        return false;
    button->number.widget.frame->fields.name = text("number");
    button->number.state.fields.font = text("FONT_BASIC");
    button->number.state.fields.text =
        (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = converted};
    if (!ab_ui_widget_add(&button->widget, &button->number.widget, error) ||
        !add_image(button, &button->stars, "stars", (AbUiPointerValue){0}, error))
        return false;
    button->stars.frame->fields.x = number(0);
    button->stars.frame->fields.y = number(0);
    AbUiPointerValue eagle, feather;
    return power_image(button, "levelSelectionSilhouette", "LS_CAN_UI", &eagle,
                       error) &&
           add_image(button, &button->eagle, "eagle", eagle, error) &&
           power_image(button, "collectableReward", "LS_FEATHER_UI", &feather, error) &&
           add_image(button, &button->feather, "feather", feather, error) &&
           ab_ui_scaling_set_to(button->host.scaling, &button->widget.frame->fields,
                                error);
}

bool ab_ui_level_button_base_init(AbUiLevelButton *button, AbUiWidgetContext *context,
                                  const AbDataTable *level, AbUiLevelButtonHost host,
                                  AbError *error) {
    if (!button || button->widget.initialized || !context || !level || !host.episodes ||
        !host.scaling || !host.fonts)
        return ab_fail(error, "Missing fresh basic level-button storage/host");
    button->host = host;
    button->metadata = level;
    const AbDataValue *id = field(level, "episode");
    const AbDataTable *pages = NULL;
    if (!id || !positive_index(field(level, "page"), &button->page, error) ||
        !positive_index(field(level, "index"), &button->index, error) ||
        !child(host.episodes, *id, &button->episode_metadata, error) ||
        !child(button->episode_metadata,
               (AbDataValue){.kind = AB_DATA_STRING, .string = text("pages").text},
               &pages, error) ||
        !child(pages, data_number(button->page), &button->page_metadata, error))
        return false;
    button->episode = *id;
    int length = snprintf(button->name, sizeof(button->name), "level%zu-%zu",
                          button->page, button->index);
    if (length < 0 || (size_t)length >= sizeof(button->name))
        return ab_fail(error, "Actual level-button name exceeds storage");
    AbUiImageButtonArguments arguments = {.image.frame = {.name = text(button->name)},
                                          .activate_on_release = boolean(true),
                                          .return_value = text("GOTO_LEVEL"),
                                          .disabled_return_value =
                                              text("DISABLED_LEVEL")};
    if (!ab_ui_widget_init(
            &button->widget, context, AB_UI_WIDGET_IMAGE_BUTTON,
            (AbUiWidgetChildren){button->frames, button->draw, button->pointer, 5},
            NULL, NULL, &arguments, error))
        return false;
    const AbDataValue *image = field(level, "level_button_override");
    const AbDataValue *disabled = field(level, "disabled_button_override");
    if (!truthy(image))
        image = field(button->page_metadata, "level_button");
    if (!truthy(disabled))
        disabled = field(button->page_metadata, "disabled_level_button");
    AbUiPointerValue enabled_value, disabled_value = text("LS_LEVEL_BG_NORMAL_CLOSED");
    if (!value(image, &enabled_value, error) ||
        (truthy(disabled) && !value(disabled, &disabled_value, error)) ||
        !ab_ui_image_button_state_set_image(&button->widget.state, enabled_value,
                                            disabled_value, &context->images, error) ||
        !ab_ui_scaling_set_to(host.scaling, &button->widget.frame->fields, error))
        return finish(button, false);
    const char *const keys[] = {"episode", "page", "level", "levelName",
                                "intro_cutscene"};
    const AbDataValue *source[] = {id, field(level, "page"), field(level, "index"),
                                   field(level, "name"),
                                   field(level, "intro_cutscene")};
    size_t count = 0;
    for (size_t index = 0; index < 5; ++index)
        if (source[index] && source[index]->kind != AB_DATA_NIL)
            button->meta_fields[count++] = (AbDataEntry){
                {.kind = AB_DATA_STRING, .string = text(keys[index]).text},
                *source[index]};
    button->meta = (AbDataTable){button->meta_fields, count, 5, NULL};
    button->widget.state.meta = (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT,
                                                   .object = (uintptr_t)&button->meta};
    return true;
}

bool ab_ui_level_button_init(AbUiLevelButton *button, AbUiWidgetContext *context,
                             const AbDataTable *level, AbUiLevelButtonHost host,
                             AbError *error) {
    return ab_ui_level_button_base_init(button, context, level, host, error) &&
           finish(button, basic_children(button, error));
}

static bool eagle_used(AbUiLevelButton *button, const AbDataTable *score, bool *output,
                       AbError *error) {
    *output = false;
    if (!score)
        return true;
    const AbDataValue *points = field(score, "score");
    bool zero = points && points->kind == AB_DATA_NUMBER &&
                !(points->bits & UINT32_C(0x7fffffff));
    if ((truthy(points) && !zero) || !truthy(field(score, "completed")))
        return true;
    const AbDataValue *power = field(button->episode_metadata, "mightyPower");
    AbDataValue free_power = power ? *power : (AbDataValue){0};
    if (truthy(power)) {
        if (power->kind != AB_DATA_TABLE || !power->table)
            return ab_fail(error, "Invalid eagle-use mighty-power metadata");
        const AbDataValue *free = field(power->table, "free");
        free_power = free ? *free : (AbDataValue){0};
    }
    if (!button->host.eagle_used_in)
        return ab_fail(error, "Missing reached settingsWrapper.getEagleUsedIn service");
    const AbDataTable *used = NULL;
    if (!button->host.eagle_used_in(button->host.context, free_power, &used, error))
        return false;
    if (!used || used->count > used->capacity || (used->count && !used->entries))
        return ab_fail(error, "Invalid actual eagle-use collection");
    const AbDataValue *name = field(button->metadata, "name");
    if (!name || name->kind != AB_DATA_STRING)
        return ab_fail(error, "Missing eagle-use level name");
    for (size_t index = 1; index <= used->count; ++index) {
        const AbDataValue *item = ab_data_get(used, data_number(index));
        if (!item || item->kind != AB_DATA_TABLE || !item->table)
            return ab_fail(error, "Eagle-use collection requires dense records");
        const AbDataValue *entry = field(item->table, "level");
        if (entry && entry->kind == AB_DATA_STRING &&
            entry->string.size == name->string.size &&
            !memcmp(entry->string.data, name->string.data, name->string.size))
            *output = true;
    }
    return true;
}

static bool star_image(AbUiLevelButton *button, float stars, AbUiPointerValue *output,
                       AbError *error) {
    AbSpan digits;
    uint8_t storage[32];
    if (!ab_ui_value_text(number(stars), storage, sizeof(storage), &digits) ||
        digits.size > sizeof(button->star_image) - 9)
        return ab_fail(error, "Actual star sprite identifier exceeds storage");
    memcpy(button->star_image, "LS_STARS_", 9);
    memcpy(button->star_image + 9, digits.data, digits.size);
    *output = (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                                 .text = {button->star_image, 9 + digits.size}};
    AbDataValue key = {.kind = AB_DATA_NUMBER, .bits = number(stars).number_bits};
    const AbDataTable *sources[] = {button->episode_metadata, button->page_metadata,
                                    button->metadata};
    for (size_t index = 0; index < 3; ++index) {
        const AbDataValue *overrides = field(sources[index], "star_sprite_override");
        if (!truthy(overrides))
            continue;
        if (overrides->kind != AB_DATA_TABLE || !overrides->table)
            return ab_fail(error, "Invalid actual level star-sprite overrides");
        const AbDataValue *selected = ab_data_get(overrides->table, key);
        if (truthy(selected) && !value(selected, output, error))
            return false;
    }
    return true;
}

static bool open_button(AbUiLevelButton *button, AbError *error) {
    AbUiWidgetContext *context = button->widget.context;
    if (!ab_ui_image_button_state_enable(&button->widget.state, boolean(true),
                                         &context->images, error))
        return false;
    const AbEpisodeStatistics *statistics = button->host.statistics;
    const AbDataValue *name = field(button->metadata, "name");
    if (!statistics || !statistics->highscores || !name || name->kind != AB_DATA_STRING)
        return ab_fail(error, "Missing actual level-button statistics/name");
    const AbDataValue *entry = ab_data_get(statistics->highscores, *name);
    const AbDataTable *score = NULL;
    if (truthy(entry)) {
        if (entry->kind != AB_DATA_TABLE || !entry->table)
            return ab_fail(error, "Invalid actual level-button highscore");
        score = entry->table;
    }
    if (!ab_ui_pointer_value_truthy(&button->has_video_button))
        button->number.widget.frame->fields.visible = boolean(true);
    button->eagle.frame->fields.visible = boolean(false);
    button->stars.frame->fields.visible = boolean(false);
    button->feather.frame->fields.visible = boolean(false);
    bool special = truthy(field(button->metadata, "stars_required")) ||
                   truthy(field(button->metadata, "feathers_required"));
    if (button->stars_empty)
        button->stars_empty->frame->fields.visible = boolean(true);
    float stars;
    bool used;
    if (!ab_episode_statistics_level_stars(statistics, button->episode, button->page,
                                           button->metadata, &stars, error) ||
        !eagle_used(button, score, &used, error))
        return false;
    if (stars != 0) {
        AbUiPointerValue image;
        if (!star_image(button, stars, &image, error) ||
            !set_image(button, &button->stars, image, error))
            return false;
        button->stars.frame->fields.visible = boolean(true);
    } else if (used) {
        button->number.widget.frame->fields.visible = boolean(!special);
        button->stars.frame->fields.visible = boolean(false);
        button->eagle.frame->fields.visible = boolean(!special);
    }
    const AbDataValue *eagle_score = field(score, "eagleScore");
    if (truthy(eagle_score)) {
        float earned;
        if (!numeric(eagle_score, &earned, error))
            return false;
        if (earned >= 100)
            button->feather.frame->fields.visible = boolean(true);
    }
    return true;
}

bool ab_ui_level_button_open(AbUiLevelButton *button, AbError *error) {
    return valid(button, error) && finish(button, open_button(button, error));
}

bool ab_ui_level_button_close(AbUiLevelButton *button, AbError *error) {
    if (!valid(button, error))
        return false;
    if (!ab_ui_image_button_state_enable(&button->widget.state, boolean(false),
                                         &button->widget.context->images, error))
        return finish(button, false);
    AbUiWidget *children[] = {&button->number.widget, &button->stars, &button->eagle,
                              &button->feather, button->stars_empty};
    for (size_t index = 0; index < 5; ++index)
        if (children[index])
            children[index]->frame->fields.visible = boolean(false);
    return !button->host.on_close ||
           finish(button, button->host.on_close(button->host.context, button, error));
}

bool ab_ui_level_button_grid_layout(AbUiLevelButton *button, bool arrows, AbVec2 margin,
                                    AbError *error) {
    if (!valid(button, error))
        return false;
    const AbDataTable *parameters = NULL;
    size_t cols = 0, rows = 0, page_index = 0;
    if (!child(
            button->page_metadata,
            (AbDataValue){.kind = AB_DATA_STRING, .string = text("layout_params").text},
            &parameters, error) ||
        !positive_index(field(parameters, "cols"), &cols, error) ||
        !positive_index(field(parameters, "rows"), &rows, error) ||
        !positive_index(field(button->page_metadata, "index"), &page_index, error))
        return finish(button, false);
    if (cols < 2 || rows > SIZE_MAX / cols || !isfinite(margin.x) ||
        !isfinite(margin.y))
        return finish(button, ab_fail(error, "Unsupported basic level-grid geometry"));
    if (button->index > rows * cols)
        return true;
    AbUiScalingEnvironment view;
    if (!button->host.scaling->host.environment(button->host.scaling->host.context,
                                                &view, error))
        return finish(button, false);
    float border = margin.x;
    if (arrows) {
        AbVec2 size;
        if (!ab_ui_scaling_sprite(button->host.scaling, &button->widget.context->images,
                                  text("BUTTON_ARROW_LEFT").text, false, &size, error))
            return finish(button, false);
        border = size.x + margin.x;
    }
    float right = view.screen.x - border;
    float width = right - border;
    float factor = 0.83f;
    float x = ((float)page_index - 1) * view.screen.x;
    x = x + (0.5f * width) * (1 - factor);
    x = x + border;
    float step = (factor * width) / ((float)cols - 1);
    x = x + step * (float)((button->index - 1) % cols);
    float base = -0.1f * view.screen.y;
    float row = (float)((button->index - 1) / cols + 1);
    float y = view.screen.y * ((row / ((float)rows + 2)) * 1.2f);
    button->widget.frame->fields.x = number(x);
    button->widget.frame->fields.y = number(base + y);
    return finish(button, ab_ui_scaling_set_to(button->host.scaling,
                                               &button->widget.frame->fields, error));
}
