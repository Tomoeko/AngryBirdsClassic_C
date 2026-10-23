#include "ab/ui_level_facebook.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static AbUiPointerValue text(const char *value) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                              .text = {(const uint8_t *)value, strlen(value)}};
}

static AbUiPointerValue number(float value) {
    AbUiPointerValue output = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&output.number_bits, &value, sizeof(value));
    return output;
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(
        table, (AbDataValue){.kind = AB_DATA_STRING, .string = text(name).text});
    return value ? *value : (AbDataValue){0};
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Missing Facebook level-page geometry");
    memcpy(output, &value.bits, sizeof(*output));
    return (isfinite(*output) && *output >= 1 && *output == floorf(*output)) ||
           ab_fail(error, "Invalid Facebook level-page geometry");
}

bool ab_ui_level_facebook_init(AbUiLevelFacebook *owner, AbUiWidget *scroll,
                               const AbDataTable *page, AbUiScaling *scaling,
                               bool age_filter, AbError *error) {
    if (!owner || owner->page || !scroll || !scroll->initialized || !page || !scaling)
        return ab_fail(error, "Invalid Facebook level-page bindings");
    owner->page = page;
    owner->scaling = scaling;
    if (age_filter)
        return true;
    float index;
    if (!numeric(field(page, "index"), &index, error))
        return false;
    int count =
        snprintf(owner->name, sizeof(owner->name), "fbButton%.0f", (double)index);
    if (count < 1 || (size_t)count >= sizeof(owner->name))
        return ab_fail(error, "Facebook page name exceeds its storage");
    AbUiImageButtonArguments args = {
        .image.frame.name = text(owner->name),
        .return_value = text("GOTO_FACEBOOK_CONNECT"),
        .activate_on_release = {.kind = AB_UI_POINTER_BOOLEAN, .boolean = true},
        .click_sound = text("menu_confirm")};
    if (!ab_ui_widget_init(&owner->like, scroll->context, AB_UI_WIDGET_IMAGE_BUTTON,
                           (AbUiWidgetChildren){0}, NULL, NULL, &args, error) ||
        !ab_ui_image_button_state_set_image(&owner->like.state, text("FB_LIKE_BUTTON"),
                                            (AbUiPointerValue){0},
                                            &scroll->context->images, error) ||
        !ab_ui_widget_add(scroll, &owner->like, error))
        return false;
    owner->present = true;
    return true;
}

void ab_ui_level_facebook_entry(AbUiLevelFacebook *owner, bool age_filter) {
    if (owner && owner->present)
        owner->like.frame->fields.visible =
            (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = !age_filter};
}

bool ab_ui_level_facebook_layout(AbUiLevelFacebook *owner, AbUiLevelButton *buttons,
                                 size_t count, AbError *error) {
    if (!owner || !owner->page || !buttons || !count)
        return ab_fail(error, "Missing Facebook page layout owner");
    AbUiWidgetContext *context = buttons[0].widget.context;
    AbUiScalingEnvironment view;
    float page, amount;
    AbDataValue params = field(owner->page, "layout_params");
    AbDataValue sprite = field(owner->page, "level_button");
    if (!context || params.kind != AB_DATA_TABLE || sprite.kind != AB_DATA_STRING ||
        !numeric(field(owner->page, "index"), &page, error) ||
        !numeric(field(params.table, "levels"), &amount, error) ||
        !owner->scaling->host.environment(owner->scaling->host.context, &view, error))
        return ab_fail(error, "Invalid Facebook page layout metadata");
    AbVec2 button;
    if (!ab_ui_scaling_sprite(owner->scaling, &context->images, sprite.string, false,
                              &button, error))
        return false;
    float x = (page - 1) * view.screen.x + 0.5f * view.screen.x;
    float y = view.screen.y * 0.5f;
    if (owner->present) {
        AbVec2 like;
        if (!ab_ui_scaling_sprite(owner->scaling, &context->images,
                                  owner->like.state.image.image.text, false, &like,
                                  error))
            return false;
        owner->like.frame->fields.x = number(x);
        float top = view.screen.y / 2.75f;
        owner->like.frame->fields.y = number(top);
        if (!ab_ui_scaling_set_to(owner->scaling, &owner->like.frame->fields, error))
            return false;
        y = (top + like.y / 2.28f) + button.y * 0.5f;
    }
    for (size_t index = 0; index < count; ++index) {
        AbUiLevelButton *level = buttons + index;
        if (level->page_metadata != owner->page || (float)level->index > amount)
            continue;
        float left = (amount - 1) * 0.5f;
        left = left * 1.5f;
        left = left * button.x;
        float offset = ((float)level->index - 1) * 1.5f;
        offset = offset * button.x;
        level->widget.frame->fields.x = number((x - left) + offset);
        level->widget.frame->fields.y = number(y);
        if (!ab_ui_scaling_set_to(owner->scaling, &level->widget.frame->fields, error))
            return false;
    }
    return true;
}

bool ab_ui_level_facebook_open(AbUiLevelButton *button, bool age_filter,
                               AbError *error) {
    if (!ab_ui_level_button_open(button, error))
        return false;
    if (!age_filter)
        return true;
    AbDataValue image = field(button->page_metadata, "level_button");
    if (image.kind != AB_DATA_STRING)
        return ab_fail(error, "Missing Facebook page's open level image");
    AbUiPointerValue name = {.kind = AB_UI_POINTER_TEXT, .text = image.string};
    if (!ab_ui_image_button_state_set_image(&button->widget.state, name, name,
                                            &button->widget.context->images, error))
        return false;
    button->widget.state.return_value = text("GOTO_LEVEL");
    return true;
}
