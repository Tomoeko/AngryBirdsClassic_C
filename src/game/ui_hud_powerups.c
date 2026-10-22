#include "ab/ui_hud_powerups.h"

#include <math.h>
#include <stdio.h>
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

static AbUiPointerValue boolean(bool value) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = value};
}

static bool named(AbSpan value, const char *name) {
    return value.size == strlen(name) &&
           (!value.size || (value.data && !memcmp(value.data, name, value.size)));
}

static AbDataValue key(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = text(name).text};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, key(name));
    return value ? *value : (AbDataValue){0};
}

static bool real(AbUiPointerValue value, float *output, AbError *error) {
    if (value.kind == AB_UI_POINTER_NUMBER) {
        memcpy(output, &value.number_bits, sizeof(*output));
        if (isfinite(*output))
            return true;
    }
    return ab_fail(error, "HUD powerup layout requires finite numeric geometry");
}

static bool image(AbUiWidget *widget, AbUiWidgetContext *context,
                  AbUiWidgetChildren children, const char *name,
                  AbUiPointerValue enabled_image, AbUiPointerValue disabled_image,
                  AbError *error) {
    AbUiImageButtonArguments arguments = {.image.frame.name = text(name)};
    return ab_ui_widget_init(widget, context, AB_UI_WIDGET_IMAGE_BUTTON, children, NULL,
                             NULL, &arguments, error) &&
           ab_ui_image_button_state_set_image(&widget->state, enabled_image,
                                              disabled_image, &context->images, error);
}

bool ab_ui_hud_powerup_enable(AbUiHudPowerup *button, AbUiPointerValue enabled,
                              AbError *error) {
    if (!button || !button->button.initialized || !button->button.context ||
        button->button.context->failed)
        return ab_fail(error, "Invalid or stopped PowerupButton");
    AbUiWidget *widget = &button->button;
    bool okay = ab_ui_image_button_state_enable(&widget->state, enabled,
                                                &widget->context->images, error);
    AbUiFrameNode *background = NULL;
    AbSpan name = text("bg").text;
    if (okay)
        okay = ab_ui_frame_find_child(&widget->tree, &name, 1, &background, error);
    if (okay && background) {
        if (background != &button->background.tree)
            okay = ab_fail(error, "PowerupButton has a foreign bg child");
        else
            okay = ab_ui_image_button_state_enable(&button->background.state, enabled,
                                                   &widget->context->images, error);
    }
    if (!okay)
        widget->context->failed = true;
    return okay;
}

static bool metadata(AbUiHudPowerup *button, AbError *error) {
    AbSpan name = button->metadata->name;
    if (!name.data || !name.size || name.size > sizeof(button->settings_name))
        return ab_fail(error, "HUD powerup metadata name exceeds storage");
    for (size_t index = 0; index < name.size; ++index) {
        uint8_t value = name.data[index];
        if (value >= 128)
            return ab_fail(error, "HUD powerup metadata requires target ASCII names");
        button->settings_name[index] =
            value >= 'A' && value <= 'Z' ? (uint8_t)(value + ('a' - 'A')) : value;
    }
    button->meta_fields[0] = (AbDataEntry){
        key("settingsName"),
        {.kind = AB_DATA_STRING, .string = {button->settings_name, name.size}}};
    button->meta_fields[1] =
        (AbDataEntry){key("name"), {.kind = AB_DATA_STRING, .string = name}};
    button->meta = (AbDataTable){button->meta_fields, 2, 2, NULL};
    button->button.state.meta = (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT,
                                                   .object = (uintptr_t)&button->meta};
    return true;
}

static bool counter(AbUiHudPowerup *button, AbUiFontResources *fonts,
                    AbSpan device_model, AbError *error) {
    AbUiWidgetContext *context = button->button.context;
    AbUiWidgetChildren background_children = {button->background_frames,
                                              button->background_draw,
                                              button->background_pointer, 1};
    if (!image(&button->background, context, background_children, "bg",
               text("INGAME_BUTTON_SCOREBAR"), (AbUiPointerValue){0}, error) ||
        !ab_ui_widget_add(&button->button, &button->background, error) ||
        !ab_ui_text_widget_init(&button->counter, context, (AbUiWidgetChildren){0},
                                fonts, NULL, NULL, error))
        return false;
    /* Source assigns these after Text.new; its initial font/measurement effects
     * must therefore precede this font field replacement. */
    button->counter.state.fields.font =
        text(named(device_model, "iphone") ? "FONT_POWERUP_AMOUNT" : "FONT_LS_SMALL");
    button->counter.widget.frame->fields.name = text("counter");
    button->counter.state.fields.text = text("0");
    button->counter.state.fields.horizontal_anchor = text("HCENTER");
    button->counter.state.fields.vertical_anchor = text("VCENTER");
    if (!ab_ui_widget_add(&button->background, &button->counter.widget, error) ||
        !image(&button->tag, context, (AbUiWidgetChildren){0}, "tag",
               text("SHOP_BUTTON_SMALL"), (AbUiPointerValue){0}, error))
        return false;
    button->tag.frame->fields.visible = boolean(false);
    button->tag.state.return_value = text("ACTIVATE_POWERUP");
    button->tag.state.activate_on_release = boolean(true);
    memcpy(button->tag_meta_fields, button->meta_fields, sizeof(button->meta_fields));
    button->tag_meta = (AbDataTable){button->tag_meta_fields, 2, 2, NULL};
    button->tag.state.meta = (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT,
                                                .object = (uintptr_t)&button->tag_meta};
    if (!ab_ui_widget_add(&button->button, &button->tag, error))
        return false;
    button->has_counter = true;
    return true;
}

bool ab_ui_hud_powerup_add_counter(AbUiHudPowerup *button, AbUiFontResources *fonts,
                                   AbSpan device_model, AbError *error) {
    if (!button || !button->button.initialized || !button->button.context ||
        button->button.context->failed || !fonts || button->has_counter ||
        button->background.initialized || button->tag.initialized ||
        button->meta.entries != button->meta_fields || button->meta.count != 2 ||
        (device_model.size && !device_model.data))
        return ab_fail(error, "Invalid actual HUD powerup counter bindings");
    bool okay = counter(button, fonts, device_model, error);
    if (!okay)
        button->button.context->failed = true;
    return okay;
}

static bool add_powerup(AbUiHudPowerups *owner, size_t index, AbError *error) {
    AbUiHudPowerup *button = owner->powerups + index;
    button->metadata = ab_powerup_catalog_at(owner->host.catalog, index);
    AbDataValue show = field(button->metadata->definition, "showInPowerupBar");
    if (show.kind == AB_DATA_BOOLEAN && !show.bits)
        return true;
    AbDataValue icon = field(button->metadata->definition, "hudIcon");
    if (icon.kind != AB_DATA_STRING || !icon.string.data ||
        icon.string.size > sizeof(button->inactive_image) - sizeof("_INACTIVE"))
        return ab_fail(error, "Invalid target HUD powerup icon");
    memcpy(button->inactive_image, icon.string.data, icon.string.size);
    memcpy(button->inactive_image + icon.string.size, "_INACTIVE", sizeof("_INACTIVE"));
    int length = snprintf(button->name, sizeof(button->name), "powerup%zu", index + 1);
    if (length < 0 || (size_t)length >= sizeof(button->name))
        return ab_fail(error, "HUD powerup button name exceeds storage");
    AbUiWidgetContext *context = owner->slider.box.widget.context;
    AbUiPointerValue resource = {.kind = AB_UI_POINTER_TEXT, .text = icon.string};
    if (!image(&button->button, context,
               (AbUiWidgetChildren){button->frames, button->draw, button->pointer, 2},
               button->name, resource, text(button->inactive_image), error))
        return false;
    button->button.frame->fields.visible = boolean(true);
    button->button.state.activate_on_release = boolean(true);
    button->button.state.return_value = text("ACTIVATE_POWERUP");
    if (!named(owner->host.device_model, "android")) {
        float height;
        button->button.padding[2] = number(50);
        if (!real(button->button.frame->fields.h, &height, error) || height == 0)
            return ab_fail(error, "Invalid non-Android HUD powerup hit height");
        button->button.padding[3] = number(500 / height);
    }
    if (!metadata(button, error))
        return false;
    button->slider.widget = &button->button;
    if (!ab_ui_slider_add(&owner->slider, &button->slider, error))
        return false;
    button->present = true;
    AbUiPointerValue enabled = owner->host.registered_enabled[index];
    if (ab_ui_pointer_value_truthy(&enabled) &&
        !ab_ui_hud_powerup_add_counter(button, owner->host.fonts,
                                       owner->host.device_model, error))
        return false;
    return true;
}

static bool shop(AbUiHudPowerups *owner, AbError *error) {
    AbUiWidget *button = &owner->shop;
    if (!image(button, owner->slider.box.widget.context, (AbUiWidgetChildren){0},
               "powerup_shop_button", text("SHOP_BUTTON"), text("SHOP_BUTTON"), error))
        return false;
    button->frame->fields.visible = boolean(true);
    button->state.activate_on_release = boolean(true);
    button->state.return_value = text("OPEN_SHOP_FROM_PLUS_SIGN");
    owner->shop_meta_fields[0] = (AbDataEntry){key("settingsName"), key("shop_button")};
    owner->shop_meta_fields[1] = (AbDataEntry){key("name"), key("shop_button")};
    owner->shop_meta = (AbDataTable){owner->shop_meta_fields, 2, 2, NULL};
    button->state.meta = (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT,
                                            .object = (uintptr_t)&owner->shop_meta};
    button->padding[0] = number(10);
    button->padding[1] = number(10);
    button->padding[2] = number(15);
    button->padding[3] = number(10);
    owner->shop_item.widget = button;
    if (!ab_ui_slider_add(&owner->slider, &owner->shop_item, error))
        return false;
    owner->shop_present = true;
    return true;
}

static bool build(AbUiHudPowerups *owner, AbUiWidget *hud, AbUiHudPowerupsSource source,
                  AbError *error) {
    AbUiPointerValue padding[4] = {0};
    padding[2] = number(named(owner->host.device_model, "android") ? 0 : 50);
    AbUiSlider *slider = &owner->slider;
    if (!ab_ui_slider_init(slider, hud->context,
                           (AbUiWidgetChildren){owner->frames, owner->draw,
                                                owner->pointer,
                                                AB_UI_HUD_POWERUP_CHILDREN + 2},
                           owner->items, AB_UI_HUD_POWERUP_CHILDREN, NULL, padding,
                           owner->host.global_enabled, owner->host.slider, error))
        return false;
    slider->box.widget.frame->fields.name = text("powerup_button");
    if (!ab_ui_slider_images(slider, text("INGAME_BUTTON_EMPTY"),
                             text("INGAME_BUTTON_POWERUP"), false, error) ||
        !ab_ui_slider_images(slider, text("INGAME_BUTTON_POWERUPANIM"), text(""), true,
                             error))
        return false;
    slider->toggle.state.return_value = text("TOGGLE_POWERUPS");
    slider->toggle.state.disabled_return_value = text("TOGGLE_POWERUPS_DISABLED");
    if (!ab_ui_slider_direction(slider, text("RIGHT").text, error))
        return false;
    slider->box.components[AB_UI_BOX_CENTER] = text("INGAME_POWERUP_OVERLAYMIDDLE");
    slider->box.components[AB_UI_BOX_RIGHT] = text("INGAME_POWERUP_OVERLAYEND");
    if (!ab_ui_slider_curve(slider, AB_UI_SLIDER_CUBIC_OUT, error))
        return false;
    slider->toggle.state.click_sound = text("menu_confirm");
    if (!ab_ui_widget_add(hud, &slider->box.widget, error))
        return false;
    slider->box.widget.frame->fields.visible = source.visible;
    for (size_t index = 0; index < owner->count; ++index)
        if (!add_powerup(owner, index, error))
            return false;
    if (source.plus_signs && !source.trial && !shop(owner, error))
        return false;
    if (source.birds_shot == source.bird_count) {
        bool hide = true;
        for (size_t index = 0; index < owner->count; ++index) {
            AbUiHudPowerup *button = owner->powerups + index;
            if (!button->present)
                continue;
            if (named(button->metadata->name, "birdquake"))
                hide = false;
            else if (!ab_ui_hud_powerup_enable(button, boolean(false), error))
                return false;
        }
        if (hide) {
            slider->box.widget.frame->fields.visible = boolean(false);
            owner->hide_powerups = true;
        }
    }
    return true;
}

bool ab_ui_hud_powerups_init(AbUiHudPowerups *owner, AbUiWidget *hud,
                             AbUiHudPowerupsHost host, AbUiHudPowerupsSource source,
                             AbError *error) {
    size_t count = ab_powerup_catalog_count(host.catalog);
    if (!owner || owner->initialized || !hud || !hud->initialized || !hud->context ||
        hud->context->failed || !host.fonts || !host.slider.scaling ||
        !host.slider.screen || !host.catalog || !count || count > AB_UI_HUD_POWERUPS ||
        !host.registered_enabled ||
        (host.device_model.size && !host.device_model.data) ||
        !isfinite(source.birds_shot) || !isfinite(source.bird_count))
        return ab_fail(error, "Invalid actual HUD powerup constructor bindings");
    owner->initialized = true;
    owner->host = host;
    owner->count = count;
    bool okay = build(owner, hud, source, error);
    if (!okay)
        hud->context->failed = true;
    return okay;
}

static bool layout_counter(AbUiHudPowerups *owner, AbUiHudPowerup *button,
                           float pause_height, float scale, AbError *error) {
    if (!button->has_counter)
        return ab_fail(error, "Enabled HUD powerup has no constructed source counter");
    AbUiFrameFields *background = &button->background.frame->fields;
    background->x = number(0);
    background->y = number((-pause_height * 0.05f) * scale);
    AbUiScaling *scaling = owner->host.slider.scaling;
    AbUiImageResourceBoundaries *images = &button->button.context->images;
    AbVec2 size, pivot;
    AbUiPointerValue image = button->background.state.image.image;
    if (image.kind != AB_UI_POINTER_TEXT)
        return ab_fail(error, "Invalid live HUD powerup background image");
    if (!ab_ui_scaling_sprite(scaling, images, image.text, false, &size, error) ||
        !images->pivot(images->context, image.text, &pivot, error))
        return false;
    pivot.x = pivot.x * scale;
    pivot.y = pivot.y * scale;
    button->counter.widget.frame->fields.x = number(0);
    button->counter.widget.frame->fields.y = number(size.y * 0.52f - pivot.y);
    float height;
    button->tag.frame->fields.x = number(0);
    if (!real(button->tag.frame->fields.h, &height, error))
        return false;
    button->tag.frame->fields.y = number((height * 0.65f) * scale);
    button->tag.frame->fields.scale_x = number(1);
    button->tag.frame->fields.scale_y = number(1);
    return true;
}

bool ab_ui_hud_powerups_layout(AbUiHudPowerups *owner, AbUiWidget *pause, AbVec2 margin,
                               bool eagle_visible, AbError *error) {
    if (!owner || !owner->initialized || !pause || !pause->initialized ||
        owner->slider.box.widget.context != pause->context || pause->context->failed)
        return ab_fail(error, "Invalid HUD powerup layout owner/pause button");
    float scale, pivot_x, width, height, pause_y;
    if (!isfinite(margin.x) || !isfinite(margin.y)) {
        ab_fail(error, "Nonfinite HUD layout margin");
        goto failed;
    }
    bool okay = ab_ui_scaling_get(owner->host.slider.scaling, &scale, error) &&
                real(pause->state.image.resource_pivot_x, &pivot_x, error) &&
                real(pause->frame->fields.w, &width, error) &&
                real(pause->frame->fields.h, &height, error) &&
                real(pause->frame->fields.y, &pause_y, error);
    if (!okay)
        goto failed;
    AbUiSlider *slider = &owner->slider;
    AbVec2 unused_size;
    if (!ab_ui_scaling_sprite(owner->host.slider.scaling, &pause->context->images,
                              text("INGAME_BUTTON_EMPTY").text, false, &unused_size,
                              error))
        goto failed;
    float x = floorf((pivot_x * (eagle_visible ? 5.3f : 3.2f)) * scale + margin.x);
    if (!isfinite(x)) {
        ab_fail(error, "HUD powerup layout position overflow");
        goto failed;
    }
    slider->box.widget.frame->fields.x = number(x);
    slider->box.widget.frame->fields.y = number(pause_y);
    if (!ab_ui_scaling_set_to(owner->host.slider.scaling,
                              &slider->box.widget.frame->fields, error))
        goto failed;
    slider->child_size = floorf(width * 0.97f);
    slider->gap = (-pivot_x * 0.1f) * scale;
    slider->padding = (AbVec2){(pivot_x * 0.2f) * scale, (-0.033f * height) * scale};
    if (owner->shop_present) {
        float shop_width;
        if (!real(owner->shop.frame->fields.w, &shop_width, error))
            goto failed;
        owner->shop_item.offset_x = number(-shop_width * 0.1f);
        owner->shop_item.override_size = number(shop_width * 1.03f);
    }
    for (size_t index = 0; index < owner->count; ++index) {
        AbUiHudPowerup *button = owner->powerups + index;
        if (button->present &&
            ab_ui_pointer_value_truthy(owner->host.registered_enabled + index) &&
            !layout_counter(owner, button, height, scale, error))
            goto failed;
    }
    return true;
failed:
    pause->context->failed = true;
    return false;
}
