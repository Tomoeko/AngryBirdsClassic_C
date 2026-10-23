#include "ab/ui_level_pigs.h"

#include "ab/camera_mover.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static AbUiPointerValue text(const char *value) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = span(value)};
}

static AbUiPointerValue number(float value) {
    AbUiPointerValue output = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&output.number_bits, &value, sizeof(value));
    return output;
}

static bool real(AbUiPointerValue value, float *output, AbError *error) {
    if (value.kind != AB_UI_POINTER_NUMBER)
        return ab_fail(error, "Missing Bad Piggies decoration geometry");
    memcpy(output, &value.number_bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite Bad Piggies geometry");
}

static bool environment(AbUiLevelPigs *owner, AbUiScalingEnvironment *output,
                        AbError *error) {
    AbUiScaling *scaling = owner->levels->host.levels.buttons.scaling;
    return scaling->host.environment(scaling->host.context, output, error);
}

static bool random_delay(AbUiLevelPigs *owner, AbError *error) {
    float unit;
    if (ab_script_random(owner->host.random, 0, 0, 0, &unit) != AB_SCRIPT_RANDOM_OK)
        return ab_fail(error, "Bad Piggies animation RNG failed");
    owner->animation_timer = unit * 3.5f + 2.5f;
    return true;
}

static bool lifecycle(void *context, AbUiWidget *widget, AbUiFrameMethod method,
                      AbError *error) {
    AbUiLevelPigs *owner = context;
    if (!owner->inherited.lifecycle(owner->inherited.context, widget, method, error))
        return false;
    if (!owner->present)
        return true;
    if (method == AB_UI_FRAME_ENTRY) {
        owner->timer = 0;
        if (!random_delay(owner, error))
            return false;
        owner->animation = 1;
        owner->particle_timer = 0;
        return owner->host.assets(owner->host.context, true, error);
    }
    if (method == AB_UI_FRAME_EXIT)
        return owner->host.assets(owner->host.context, false, error);
    if (method == AB_UI_FRAME_LAYOUT) {
        AbUiScalingEnvironment view;
        if (!environment(owner, &view, error))
            return false;
        owner->origin = (AbVec2){view.screen.x * 0.5f, view.screen.y * 0.85f};
        float scale = 0.8f * (view.screen.x / 1024);
        owner->pig.frame->fields.scale_x = owner->pig.frame->fields.scale_y =
            number(scale);
    }
    return true;
}

static bool set_image(AbUiLevelPigs *owner, size_t frame, AbError *error) {
    return ab_ui_image_button_state_set_image(
        &owner->pig.state, text(owner->sprites[owner->animation - 1][frame]),
        (AbUiPointerValue){0}, &owner->levels->widget.context->images, error);
}

static bool animate(AbUiLevelPigs *owner, float seconds, AbError *error) {
    owner->animation_timer -= seconds;
    if (owner->animation_timer < 0) {
        if (!random_delay(owner, error))
            return false;
        size_t previous = owner->animation;
        do {
            float selected;
            if (ab_script_random(owner->host.random, 2, 1, 3, &selected) !=
                    AB_SCRIPT_RANDOM_OK ||
                selected < 1 || selected > 3 || truncf(selected) != selected)
                return ab_fail(error, "Invalid Bad Piggies animation selection");
            owner->animation = (size_t)selected;
        } while (owner->animation == previous);
    }
    owner->timer += seconds;
    return isfinite(owner->timer) ||
           ab_fail(error, "Bad Piggies animation clock overflow");
}

static bool animation_frame(AbUiLevelPigs *owner, AbError *error) {
    float frame = floorf(owner->timer / 0.05f);
    frame = fmodf(frame, 4);
    if (frame < 0)
        frame += 4;
    if (!isfinite(owner->timer) || !isfinite(frame))
        return ab_fail(error, "Bad Piggies animation clock overflow");
    return set_image(owner, (size_t)frame, error);
}

static bool pose(AbUiLevelPigs *owner, AbError *error) {
    AbUiScalingEnvironment view;
    if (!environment(owner, &view, error))
        return false;
    /* Lua rounds each arithmetic operation and each libm result to binary32. */
    float first = owner->timer * 0.84f, second = owner->timer * 0.32f;
    float x = ((float)cos((double)first) * view.screen.x) * 0.01f;
    x = x + ((float)cos((double)second) * view.screen.x) * 0.005f;
    float y = ((float)sin((double)first) * view.screen.x) * 0.01f;
    y = y + ((float)cos((double)second) * view.screen.x) * 0.005f;
    float angle = (float)sin((double)(owner->timer * 0.16f)) * 0.1f;
    if (owner->timer < 4) {
        float offset;
        if (!ab_camera_tween(AB_CAMERA_TWEEN_CUBIC_OUT, owner->timer,
                             view.screen.x * 1.5f, view.screen.x * -1.5f, 4, &offset))
            return ab_fail(error, "Bad Piggies entrance position tween failed");
        x -= offset;
        const float pi = 3.1415927410125732421875f;
        if (!ab_camera_tween(AB_CAMERA_TWEEN_CUBIC_OUT, owner->timer, pi * 0.25f,
                             pi * -0.25f, 4, &offset))
            return ab_fail(error, "Bad Piggies entrance rotation tween failed");
        angle -= offset;
    }
    owner->pig.frame->fields.x = number(owner->origin.x + x);
    owner->pig.frame->fields.y = number(owner->origin.y + y);
    owner->pig.frame->fields.angle = number(angle);
    return true;
}

static bool rocket(AbUiLevelPigs *owner, float seconds, AbError *error) {
    owner->particle_timer -= seconds;
    if (owner->particle_timer >= 0)
        return true;
    owner->particle_timer = 0.04f;
    const AbUiFrameFields *pig = &owner->pig.frame->fields;
    float w, h, sx, sy, x, y, angle;
    if (!real(pig->w, &w, error) || !real(pig->h, &h, error) ||
        !real(pig->scale_x, &sx, error) || !real(pig->scale_y, &sy, error) ||
        !real(pig->x, &x, error) || !real(pig->y, &y, error) ||
        !real(pig->angle, &angle, error))
        return false;
    float dx = (-w * 0.33f) * sx, dy = (-h * 0.38f) * sy;
    angle = (float)atan2((double)dy, (double)dx) + angle;
    /* The source passes dx*dx and dy*dy as separate sqrt arguments. Lua's
     * numeric sqrt consumes only the first; preserve that authored behavior. */
    float radius = (float)sqrt((double)(dx * dx));
    AbParticleEmission emission = {.definition = span("green_rocket"),
                                   .amount = 1,
                                   .position = {x + radius * (float)cos((double)angle),
                                                y + radius * (float)sin((double)angle)},
                                   .alternate = true};
    return owner->host.emit(owner->host.context, &emission, error);
}

static bool update(void *context, AbUiWidget *widget, float seconds, float time,
                   AbError *error) {
    AbUiLevelPigs *owner = context;
    if (!owner->inherited.update(owner->inherited.context, widget, seconds, time,
                                 error))
        return false;
    return !owner->present ||
           (animate(owner, seconds, error) && pose(owner, error) &&
            animation_frame(owner, error) && rocket(owner, seconds, error));
}

static bool draw(void *context, AbUiWidget *widget, const AbUiDrawInput *input,
                 AbError *error) {
    AbUiLevelPigs *owner = context;
    return owner->inherited.draw(owner->inherited.context, widget, input, error);
}

static bool draw_particles(void *context, AbUiWidget *widget,
                           const AbUiDrawInput *input, AbError *error) {
    AbUiLevelPigs *owner = context;
    return ab_ui_widget_draw_base(widget, input, error) &&
           owner->host.draw_particles(owner->host.context, error);
}

static bool pointer(void *context, AbUiWidget *widget, AbSpan event, AbVec2 point,
                    AbUiPointerReply *reply, AbError *error) {
    AbUiLevelPigs *owner = context;
    if (!owner->inherited.pointer(owner->inherited.context, widget, event, point, reply,
                                  error))
        return false;
    AbUiPointerValue value = reply->values[0];
    AbSpan target = span("GOTO_BAD_PIGGIES");
    return value.kind != AB_UI_POINTER_TEXT || value.text.size != target.size ||
           memcmp(value.text.data, target.data, target.size) ||
           ab_event_manager_notify(owner->levels->host.levels.scroll.events,
                                   &owner->event, error);
}

static bool children(AbUiLevelPigs *owner, AbError *error) {
    AbUiWidget *root = &owner->levels->widget;
    if (!ab_ui_widget_init(&owner->renderer, root->context, AB_UI_WIDGET_FRAME,
                           (AbUiWidgetChildren){0}, NULL, NULL, NULL, error))
        return false;
    owner->renderer.frame->fields.name = text("particleRenderer");
    owner->renderer.methods =
        (AbUiWidgetMethods){.context = owner, .draw = draw_particles};
    if (!ab_ui_widget_insert(root, &owner->renderer, 1, error) ||
        !ab_ui_widget_init(&owner->pig, root->context, AB_UI_WIDGET_IMAGE_BUTTON,
                           (AbUiWidgetChildren){0}, NULL, NULL, NULL, error))
        return false;
    owner->pig.frame->fields.name = text("pig_button");
    bool available = (owner->host.has_web_browser && !owner->host.sugotoku &&
                      !owner->host.disable_cross_sell) ||
                     owner->host.talkweb_allowed;
    if (!owner->host.freemium_china && available) {
        if (!set_image(owner, 0, error))
            return false;
        owner->pig.state.return_value = text("GOTO_BAD_PIGGIES");
        owner->pig.state.scale_on_touch =
            (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    }
    /* The source writes the unused misspelling floorCoodinates=false. Keep the
     * real Image.floorCoordinates field at its inherited true default. */
    owner->pig.state.activate_on_release =
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    owner->pig.state.hover_enabled = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    return ab_ui_widget_insert(root, &owner->pig, 2, error);
}

bool ab_ui_level_pigs_attach(AbUiLevelPigs *owner, AbUiLevelSelection *levels,
                             AbUiLevelPigsHost host, AbError *error) {
    if (!owner || owner->initialized || !levels || !levels->widget.initialized ||
        !levels->widget.methods.lifecycle || !levels->widget.methods.update ||
        !levels->widget.methods.draw || !levels->widget.methods.pointer ||
        (!host.cross_promotion_disabled &&
         (!host.random || !host.assets || !host.emit || !host.draw_particles)))
        return ab_fail(error, "Invalid Bad Piggies level-screen bindings");
    owner->levels = levels;
    owner->inherited = levels->widget.methods;
    owner->host = host;
    owner->animation = 1;
    owner->present = !host.cross_promotion_disabled;
    const char *const groups[] = {"ANIM", "BLINK", "SMILE"};
    bool promo =
        !host.cross_promotion_disabled &&
        ((host.has_web_browser && !host.sugotoku && !host.disable_cross_sell) ||
         host.talkweb_allowed);
    for (size_t animation = 0; animation < 3; ++animation)
        for (size_t frame = 0; frame < 4; ++frame) {
            char *destination = owner->sprites[animation][frame];
            int count = snprintf(destination, sizeof(owner->sprites[0][0]),
                                 "BUTTON_GREEN_UPSELL_%s_%zu%s", groups[animation],
                                 frame + 1, promo ? "" : "_0");
            if (count < 0 || (size_t)count >= sizeof(owner->sprites[0][0]))
                return ab_fail(error, "Bad Piggies sprite name exceeds storage");
        }
    AbDataValue id = {0};
    const AbDataValue *found = ab_data_get(
        levels->host.levels.event_ids,
        (AbDataValue){.kind = AB_DATA_STRING, .string = span("EID_PIGGIES_CLICKED")});
    if (found)
        id = *found;
    if (owner->present && id.kind == AB_DATA_NIL)
        return ab_fail(error, "Missing Bad Piggies source event id");
    owner->event_field =
        (AbDataEntry){{.kind = AB_DATA_STRING, .string = span("id")}, id};
    owner->event = (AbDataTable){&owner->event_field, 1, 1, NULL};
    if (owner->present && !children(owner, error))
        return false;
    levels->widget.methods = (AbUiWidgetMethods){.context = owner,
                                                 .lifecycle = lifecycle,
                                                 .update = update,
                                                 .draw = draw,
                                                 .pointer = pointer};
    owner->initialized = true;
    return true;
}
