#include "ab/ui_level_hidden_eggs.h"

#include <math.h>
#include <string.h>

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbUiPointerValue text(const char *name) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = span(name)};
}

static AbUiPointerValue number(float value) {
    AbUiPointerValue output = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&output.number_bits, &value, sizeof(value));
    return output;
}

static AbUiPointerValue boolean(bool value) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = value};
}

static bool real(AbUiPointerValue value, float *output, AbError *error) {
    if (value.kind != AB_UI_POINTER_NUMBER)
        return ab_fail(error, "Hidden egg requires numeric widget geometry");
    memcpy(output, &value.number_bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite hidden-egg geometry");
}

static bool unlocked(AbUiLevelHiddenEggs *owner, bool *output, AbError *error) {
    AbSpan name =
        span(owner->kind == AB_UI_LEVEL_DANGER_ABOVE ? "LevelGE_7" : "LevelGE_1");
    AbDataValue value;
    if (!ab_settings_golden_egg_unlocked(owner->highscores, name, &value, error))
        return false;
    *output =
        value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
    return true;
}

static bool create_egg(AbUiLevelHiddenEggs *owner, AbError *error) {
    AbUiWidget *egg = &owner->egg;
    bool danger = owner->kind == AB_UI_LEVEL_DANGER_ABOVE;
    AbUiWidget *parent =
        danger ? &owner->levels->levels.scroll.widget : &owner->levels->widget;
    if (!ab_ui_widget_init(egg, parent->context, AB_UI_WIDGET_IMAGE_BUTTON,
                           (AbUiWidgetChildren){0}, NULL, NULL, NULL, error))
        return false;
    egg->frame->fields.name = text(danger ? "GoldenEggEP3" : "egg");
    if (!ab_ui_image_button_state_set_image(
            &egg->state, text(danger ? "GOLDEN_EGG_5" : "GOLDEN_EGG_1"),
            (AbUiPointerValue){0}, &parent->context->images, error))
        return false;
    if (!danger)
        egg->state.return_value = text("GOLDEN_EGG_8");
    if (!ab_ui_widget_add(parent, egg, error))
        return false;
    owner->present = true;
    return true;
}

static bool egg_layout(AbUiLevelHiddenEggs *owner, AbError *error) {
    if (!owner->present)
        return true;
    AbUiLevelSelection *levels = owner->levels;
    AbUiScaling *scaling = levels->host.levels.buttons.scaling;
    AbUiScalingEnvironment view;
    if (!scaling->host.environment(scaling->host.context, &view, error))
        return false;
    AbUiFrameFields *egg = &owner->egg.frame->fields;
    if (owner->kind == AB_UI_LEVEL_DANGER_ABOVE) {
        AbVec2 size;
        AbUiImageResourceBoundaries *images = &levels->widget.context->images;
        if (!images->bounds(images->context, owner->egg.state.image.image.text, &size,
                            error))
            return false;
        egg->y = number(view.screen.y * 0.5f - size.y / 2);
        egg->x = number(3 * view.screen.x + size.x * 1.5f);
        return true;
    }
    if (!ab_ui_scaling_set_to(scaling, egg, error))
        return false;
    AbUiFrameFields *right = &levels->right_decoration.frame->fields;
    float x, y, width, height, sx, sy, scale;
    if (!real(right->x, &x, error) || !real(right->y, &y, error) ||
        !real(right->w, &width, error) || !real(right->h, &height, error) ||
        !real(right->scale_x, &sx, error) || !real(right->scale_y, &sy, error) ||
        !real(egg->scale_x, &scale, error))
        return false;
    egg->scale_x = egg->scale_y = number(scale * 0.45f);
    egg->x = number(x - (width * 0.725f) * sx);
    egg->y = number(y - (height * 0.475f) * sy);
    /* Represent the recovered math.rad(-90) argument in script precision.
     * Exact native math.rad arithmetic remains unverified. */
    float angle = -90.0f * 3.1415927410125732421875f;
    egg->angle = number(angle / 180.0f);
    owner->egg.state.image.alpha = number(0.3f);
    return true;
}

static bool lifecycle(void *context, AbUiWidget *widget, AbUiFrameMethod method,
                      AbError *error) {
    AbUiLevelHiddenEggs *owner = context;
    if (!owner->inherited.lifecycle(owner->inherited.context, widget, method, error))
        return false;
    if (method == AB_UI_FRAME_LAYOUT)
        return egg_layout(owner, error);
    if (method != AB_UI_FRAME_ENTRY)
        return true;
    bool open;
    if (owner->kind == AB_UI_LEVEL_DANGER_ABOVE && !owner->present) {
        if (!unlocked(owner, &open, error))
            return false;
        if (!open && !create_egg(owner, error))
            return false;
    }
    if (owner->present) {
        if (!unlocked(owner, &open, error))
            return false;
        owner->egg.frame->fields.visible = boolean(!open);
    }
    if (owner->kind == AB_UI_LEVEL_MINE_AND_DINE) {
        owner->egg.frame->fields.scale_x = owner->egg.frame->fields.scale_y = number(1);
        owner->fading = false;
    }
    return true;
}

static bool discover(AbUiLevelHiddenEggs *owner, AbError *error) {
    return ab_event_manager_notify(owner->levels->host.levels.scroll.events,
                                   &owner->event, error);
}

static bool danger_check(AbUiLevelHiddenEggs *owner, AbError *error) {
    if (!owner->present)
        return true;
    bool open;
    if (!unlocked(owner, &open, error))
        return false;
    if (open)
        return true;
    AbUiScaling *scaling = owner->levels->host.levels.buttons.scaling;
    AbUiScalingEnvironment view;
    float current, x;
    AbUiScrollFrame *scroll = &owner->levels->levels.scroll;
    if (!scaling->host.environment(scaling->host.context, &view, error) ||
        !real(scroll->widget.scroll[0], &current, error) ||
        !real(owner->egg.frame->fields.x, &x, error))
        return false;
    if (current < x - view.screen.x * 0.9f)
        return true;
    if (!ab_ui_frame_remove_child(&scroll->widget.tree, &owner->egg.tree, error))
        return false;
    owner->present = false;
    return discover(owner, error) && ab_ui_scroll_frame_release(scroll, error);
}

static bool update(void *context, AbUiWidget *widget, float seconds, float time,
                   AbError *error) {
    AbUiLevelHiddenEggs *owner = context;
    if (owner->kind == AB_UI_LEVEL_DANGER_ABOVE && !danger_check(owner, error))
        return false;
    if (!owner->inherited.update(owner->inherited.context, widget, seconds, time,
                                 error))
        return false;
    if (owner->kind == AB_UI_LEVEL_MINE_AND_DINE && owner->fading) {
        owner->timer -= seconds;
        float scale = fmaxf(0, (owner->start_scale * owner->timer) / 0.1f);
        if (!isfinite(owner->timer) || !isfinite(scale))
            return ab_fail(error, "Hidden-egg disappearance overflow");
        owner->egg.frame->fields.scale_x = owner->egg.frame->fields.scale_y =
            number(scale);
    }
    return true;
}

static bool pointer(void *context, AbUiWidget *widget, AbSpan action, AbVec2 point,
                    AbUiPointerReply *reply, AbError *error) {
    AbUiLevelHiddenEggs *owner = context;
    if (!owner->inherited.pointer(owner->inherited.context, widget, action, point,
                                  reply, error))
        return false;
    AbUiPointerValue value = reply->values[0];
    if (owner->kind == AB_UI_LEVEL_MINE_AND_DINE && !owner->fading &&
        value.kind == AB_UI_POINTER_TEXT && value.text.size == 12 &&
        !memcmp(value.text.data, "GOLDEN_EGG_8", 12)) {
        if (!discover(owner, error))
            return false;
        owner->timer = 0.1f;
        owner->fading = true;
        if (!real(owner->egg.frame->fields.scale_x, &owner->start_scale, error))
            return false;
    }
    return true;
}

static bool draw(void *context, AbUiWidget *widget, const AbUiDrawInput *input,
                 AbError *error) {
    AbUiLevelHiddenEggs *owner = context;
    return owner->inherited.draw(owner->inherited.context, widget, input, error);
}

bool ab_ui_level_hidden_eggs_attach(AbUiLevelHiddenEggs *owner,
                                    AbUiLevelSelection *levels,
                                    AbSettingsStatistics *highscores,
                                    AbUiLevelHiddenEggKind kind, AbError *error) {
    if (!owner || owner->initialized || !levels || !levels->widget.initialized ||
        !highscores || (unsigned)kind > AB_UI_LEVEL_MINE_AND_DINE ||
        ab_ui_pointer_value_truthy(&levels->widget.tree.entered))
        return ab_fail(error, "Invalid hidden-egg level-screen attachment");
    *owner = (AbUiLevelHiddenEggs){.levels = levels,
                                   .highscores = highscores,
                                   .inherited = levels->widget.methods,
                                   .kind = kind};
    bool create = kind == AB_UI_LEVEL_MINE_AND_DINE;
    if (!create && !levels->host.free_version) {
        bool open;
        if (!unlocked(owner, &open, error))
            return false;
        create = !open;
    }
    if (create && !create_egg(owner, error))
        return false;
    AbDataValue event_id;
    const AbDataValue *found =
        ab_data_get(levels->host.levels.event_ids,
                    (AbDataValue){.kind = AB_DATA_STRING,
                                  .string = span("EID_GOLDEN_EGG_FROM_MENU")});
    if (!found)
        return ab_fail(error, "Missing actual hidden-egg event ID");
    event_id = *found;
    owner->event_fields[0] =
        (AbDataEntry){{.kind = AB_DATA_STRING, .string = span("id")}, event_id};
    owner->event_fields[1] = (AbDataEntry){
        {.kind = AB_DATA_STRING, .string = span("levelName")},
        {.kind = AB_DATA_STRING,
         .string = span(kind == AB_UI_LEVEL_DANGER_ABOVE ? "LevelGE_7" : "LevelGE_1")}};
    owner->event = (AbDataTable){owner->event_fields, 2, 2, NULL};
    levels->widget.methods = (AbUiWidgetMethods){.context = owner,
                                                 .lifecycle = lifecycle,
                                                 .update = update,
                                                 .pointer = pointer,
                                                 .draw = draw};
    owner->initialized = true;
    return true;
}
