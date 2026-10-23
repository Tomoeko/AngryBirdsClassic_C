#include "ab/ui_level_rmf.h"
#include "ab/fill_rectangle.h"
#include "ab/game_rectangle.h"

#include <math.h>
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

static bool numeric(AbUiPointerValue value, float *output, AbError *error) {
    if (value.kind != AB_UI_POINTER_NUMBER)
        return ab_fail(error, "Missing RMF decoration geometry");
    memcpy(output, &value.number_bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite RMF decoration geometry");
}

static bool environment(AbUiLevelRmf *owner, AbUiScalingEnvironment *output,
                        AbError *error) {
    AbUiScaling *scaling = owner->levels->host.levels.buttons.scaling;
    return scaling->host.environment(scaling->host.context, output, error);
}

static bool lifecycle(void *context, AbUiWidget *widget, AbUiFrameMethod method,
                      AbError *error) {
    AbUiLevelRmf *owner = context;
    if (!owner->inherited.lifecycle(owner->inherited.context, widget, method, error))
        return false;
    if (method == AB_UI_FRAME_ENTRY) {
        owner->slide_phase = 0.5f;
        owner->tint_phase = 0;
    } else if (method == AB_UI_FRAME_LAYOUT) {
        AbUiScalingEnvironment view;
        AbUiScaling *scaling = owner->levels->host.levels.buttons.scaling;
        if (!environment(owner, &view, error))
            return false;
        owner->left.frame->fields.y = owner->right.frame->fields.y =
            number(view.screen.y);
        return ab_ui_scaling_set_to(scaling, &owner->left.frame->fields, error) &&
               ab_ui_scaling_set_to(scaling, &owner->right.frame->fields, error);
    }
    return true;
}

static bool update(void *context, AbUiWidget *widget, float seconds, float time,
                   AbError *error) {
    AbUiLevelRmf *owner = context;
    if (!owner->inherited.update(owner->inherited.context, widget, seconds, time,
                                 error))
        return false;
    if (owner->levels->current_selection == 2) {
        owner->slide_phase = fminf(1, owner->slide_phase + seconds);
        owner->tint_phase = fminf(1, owner->tint_phase + seconds * 2);
    } else {
        owner->slide_phase = fmaxf(0, owner->slide_phase - seconds);
        owner->tint_phase = fmaxf(0, owner->tint_phase - seconds * 2);
    }
    float first = fminf(owner->slide_phase, 0.5f) * 2;
    float second = (fmaxf(owner->slide_phase, 0.5f) - 0.5f) * 2;
    AbUiScaling *scaling = owner->levels->host.levels.buttons.scaling;
    float scale;
    AbUiScalingEnvironment view;
    if (!ab_ui_scaling_get(scaling, &scale, error) || !environment(owner, &view, error))
        return false;
    AbUiWidget *images[] = {&owner->levels->left_decoration, &owner->left,
                            &owner->levels->right_decoration, &owner->right};
    for (size_t index = 0; index < 4; ++index) {
        float width;
        if (!numeric(images[index]->frame->fields.w, &width, error))
            return false;
        float fraction = index % 2 == 0 ? first : 1 - second;
        float offset = (index < 2 ? -width : width) * fraction;
        offset = offset * scale;
        images[index]->frame->fields.x =
            number(index < 2 ? offset : view.screen.x + offset);
    }
    return true;
}

static bool draw(void *context, AbUiWidget *widget, const AbUiDrawInput *input,
                 AbError *error) {
    AbUiLevelRmf *owner = context;
    return owner->inherited.draw(owner->inherited.context, widget, input, error);
}

static bool pointer(void *context, AbUiWidget *widget, AbSpan event, AbVec2 point,
                    AbUiPointerReply *reply, AbError *error) {
    AbUiLevelRmf *owner = context;
    return owner->inherited.pointer(owner->inherited.context, widget, event, point,
                                    reply, error);
}

static bool image(AbUiLevelRmf *owner, AbUiWidget *widget, const char *name,
                  const char *sprite, AbError *error) {
    AbUiImageArguments args = {.frame.name = text(name), .image = text(sprite)};
    return ab_ui_widget_init(widget, owner->levels->widget.context, AB_UI_WIDGET_IMAGE,
                             (AbUiWidgetChildren){0}, NULL, &args, NULL, error) &&
           ab_ui_widget_insert(&owner->levels->widget, widget, 1, error);
}

bool ab_ui_level_rmf_attach(AbUiLevelRmf *owner, AbUiLevelSelection *levels,
                            uint32_t solid_texture, AbError *error) {
    if (!owner || owner->initialized || !levels || !levels->widget.initialized ||
        !levels->widget.methods.lifecycle || !levels->widget.methods.update ||
        !levels->widget.methods.draw || !levels->widget.methods.pointer ||
        !solid_texture)
        return ab_fail(error, "Invalid RMF level-screen base/renderer bindings");
    owner->levels = levels;
    owner->solid_texture = solid_texture;
    owner->inherited = levels->widget.methods;
    if (!image(owner, &owner->left, "left_decor_alt", "BTN_EGGDEFENDER_TITLE", error) ||
        !image(owner, &owner->right, "right_decor_alt", "LS_THEME_EGGDEFENDER2_RIGHT",
               error) ||
        !ab_ui_image_state_set_image(&levels->left_decoration.state.image,
                                     text("LS_THEME_EGGDEFENDER_LEFT"),
                                     &levels->widget.context->images, error) ||
        !ab_ui_image_state_set_image(&levels->right_decoration.state.image,
                                     text("LS_THEME_EGGDEFENDER_RIGHT"),
                                     &levels->widget.context->images, error))
        return false;
    levels->widget.methods = (AbUiWidgetMethods){.context = owner,
                                                 .lifecycle = lifecycle,
                                                 .update = update,
                                                 .draw = draw,
                                                 .pointer = pointer};
    owner->initialized = true;
    return true;
}

bool ab_ui_level_rmf_background(AbUiLevelRmf *owner, AbError *error) {
    if (!owner || !owner->initialized)
        return ab_fail(error, "Missing active RMF background hook");
    AbUiWidgetContext *context = owner->levels->widget.context;
    AbUiScalingEnvironment view;
    AbGameRectanglePlan plan;
    if (!environment(owner, &view, error) ||
        !ab_game_rectangle_plan(
            &(AbGameRectangle){.color = {0.8f, 0, 0, owner->tint_phase * 0.6f},
                               .right = view.screen.x,
                               .bottom = view.screen.y},
            &plan, error))
        return false;
    AbDrawCommand command;
    bool visible;
    return ab_fill_rectangle_reset_command(
               (AbFillRectangle){plan.argb, plan.left, plan.top, plan.width,
                                 plan.height, AB_FILL_BLEND_AUTO},
               context->render.viewport_width, context->render.viewport_height,
               owner->solid_texture, &command, &visible, error) &&
           (!visible || ab_draw_list_push(context->draw_list, &command, error));
}
