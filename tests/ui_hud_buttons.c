#include "ab/ui_hud_buttons.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "HUD button check failed at %d: %s\n", __LINE__,           \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbUiWidgetContext context;
    AbUiImageRenderState render;
    AbSpriteRenderState submitted;
    AbSpriteRegion region;
    AbDrawCommand commands[16];
    AbDrawList draws;
    AbUiPointerValue gesture, ending;
    AbUiHudSkipButton *constructing;
    unsigned queries;
} Fixture;

static AbUiPointerValue text(const char *name) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                              .text = {(const uint8_t *)name, strlen(name)}};
}

static AbUiPointerValue number(float value) {
    AbUiPointerValue output = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&output.number_bits, &value, sizeof(value));
    return output;
}

static float real(AbUiPointerValue value) {
    float output;
    memcpy(&output, &value.number_bits, sizeof(output));
    return output;
}

static bool bounds(void *context, AbSpan image, AbVec2 *output, AbError *error) {
    (void)context;
    (void)image;
    (void)error;
    *output = (AbVec2){32, 16};
    return true;
}

static bool pivot(void *context, AbSpan image, AbVec2 *output, AbError *error) {
    (void)context;
    (void)image;
    (void)error;
    *output = (AbVec2){16, 8};
    return true;
}

static bool localize(void *context, AbSpan image, AbSpan *output, AbError *error) {
    (void)context;
    (void)error;
    *output = image;
    return true;
}

static bool lookup(void *context, AbSpan image, AbUiImageRenderBinding *output,
                   AbError *error) {
    Fixture *fixture = context;
    (void)image;
    (void)error;
    fixture->submitted = fixture->render.geometry;
    *output = (AbUiImageRenderBinding){.region = &fixture->region,
                                       .texture = 1,
                                       .texture_width = 64,
                                       .texture_height = 64};
    return true;
}

static void prepare(Fixture *fixture) {
    *fixture =
        (Fixture){.region = {.width = 32, .height = 16, .pivot_x = 16, .pivot_y = 8}};
    fixture->draws = (AbDrawList){fixture->commands, 0, 16};
    fixture->render.alpha = 1;
    fixture->context = (AbUiWidgetContext){.images = {fixture, localize, bounds, pivot},
                                           .render = {.context = fixture,
                                                      .lookup = lookup,
                                                      .viewport_width = 480,
                                                      .viewport_height = 320,
                                                      .blend = AB_BLEND_PREMULTIPLIED,
                                                      .color = {1, 1, 1, 1}},
                                           .render_state = &fixture->render,
                                           .draw_list = &fixture->draws,
                                           .maximum_depth = 16,
                                           .animation_installed = true};
}

static bool ending(void *context, AbUiPointerValue *output, AbError *error) {
    Fixture *fixture = context;
    AbUiHudSkipButton *button = fixture->constructing;
    if (!button || !button->widget.initialized || button->scale_multiplier != 0 ||
        button->state != AB_UI_SKIP_HIDDEN ||
        !ab_ui_pointer_value_truthy(&button->widget.state.enabled))
        return ab_fail(error, "SkipButton query preceded the real base constructor");
    ++fixture->queries;
    *output = fixture->ending;
    return true;
}

static bool pause_checks(void) {
    for (unsigned osx = 0; osx < 2; ++osx) {
        Fixture fixture;
        prepare(&fixture);
        AbUiHudPauseButton button = {0};
        AbUiImageButtonArguments args = {
            .image = {.frame = {.x = number(100), .y = number(60)},
                      .image = text("PAUSE")}};
        CHECK(ab_ui_hud_pause_button_init(&button, &fixture.context,
                                          (AbUiWidgetChildren){0}, &args, osx,
                                          &fixture.gesture, NULL));
        AbUiDrawInput input = {.present = 63,
                               .transform = {.position = {5, 10},
                                             .scale = {2, 3},
                                             .angle = 0.25f,
                                             .local_factor = 4}};
        CHECK(ab_ui_widget_draw(&button.widget, &input, NULL) &&
              fixture.draws.count == 1);
        CHECK(fixture.submitted.scale_x == (osx ? 1 : 2) &&
              fixture.submitted.scale_y == (osx ? 1 : 3) &&
              fixture.submitted.angle_radians == (osx ? 0 : 0.25f));
        fixture.gesture = number(0); /* Lua zero is true. */
        fixture.draws.count = 0;
        CHECK(ab_ui_widget_draw(&button.widget, &input, NULL) &&
              fixture.draws.count == (osx ? 0u : 1u) &&
              ab_ui_pointer_value_truthy(&button.widget.state.enabled) == !osx);
        fixture.gesture = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
        CHECK(ab_ui_widget_draw(&button.widget, &input, NULL) &&
              ab_ui_pointer_value_truthy(&button.widget.state.enabled));
    }
    return true;
}

typedef struct {
    AbUiHudSkipButton *button;
    unsigned updates;
} Child;

static bool mutate(void *context, AbUiWidget *widget, float seconds, float time,
                   AbError *error) {
    Child *child = context;
    ++child->updates;
    child->button->state = AB_UI_SKIP_DISAPPEAR;
    child->button->scale_multiplier = 0.75f;
    return ab_ui_widget_update_base(widget, seconds, time, error);
}

static bool skip_checks(void) {
    Fixture fixture;
    prepare(&fixture);
    AbUiHudSkipButton button = {0};
    fixture.constructing = &button;
    AbUiFrameNode *frames[1];
    AbUiDrawChild draw[1];
    AbUiPointerChild pointer[1];
    CHECK(ab_ui_hud_skip_button_init(&button, &fixture.context,
                                     (AbUiWidgetChildren){frames, draw, pointer, 1},
                                     &fixture, ending, NULL));
    CHECK(fixture.queries == 1 && button.state == AB_UI_SKIP_HIDDEN &&
          button.scale_multiplier == 0 &&
          ab_ui_pointer_value_truthy(&button.widget.frame->fields.visible));
    button.widget.state.return_value = text("SKIP_TO_LEVEL_END");
    CHECK(ab_ui_image_button_state_set_image(&button.widget.state, text("SKIP"),
                                             (AbUiPointerValue){0},
                                             &fixture.context.images, NULL) &&
          ab_ui_widget_lifecycle(&button.widget, AB_UI_FRAME_ENTRY, NULL));
    button.state = AB_UI_SKIP_APPEAR;
    CHECK(ab_ui_widget_update(&button.widget, 0.25f, 0.25f, NULL) &&
          button.scale_multiplier == 0.75f && button.state == AB_UI_SKIP_APPEAR &&
          ab_ui_widget_update(&button.widget, 0.25f, 0.5f, NULL) &&
          button.scale_multiplier == 1 && button.state == AB_UI_SKIP_ONSCREEN);
    button.widget.frame->fields.x = number(100);
    button.widget.frame->fields.y = number(60);
    button.scale_multiplier = 0.5f;
    AbUiDrawInput input = {.present = 63,
                           .transform = {.scale = {2, 3}, .local_factor = 1}};
    CHECK(ab_ui_widget_draw(&button.widget, &input, NULL) && fixture.draws.count == 1 &&
          fixture.submitted.scale_x == 1 && fixture.submitted.scale_y == 1.5f);
    AbUiWidget child = {0};
    Child mutation = {&button, 0};
    CHECK(ab_ui_widget_init(&child, &fixture.context, AB_UI_WIDGET_FRAME,
                            (AbUiWidgetChildren){0}, NULL, NULL, NULL, NULL) &&
          ab_ui_widget_add(&button.widget, &child, NULL));
    child.methods = (AbUiWidgetMethods){.context = &mutation, .update = mutate};
    CHECK(ab_ui_widget_update(&button.widget, 0.25f, 0.75f, NULL) &&
          mutation.updates == 1 && button.scale_multiplier == 0 &&
          button.state == AB_UI_SKIP_GONE &&
          !ab_ui_pointer_value_truthy(&button.widget.frame->fields.visible) &&
          button.widget.state.return_value.kind == AB_UI_POINTER_NONE);
    child.methods = (AbUiWidgetMethods){0};
    button.state = AB_UI_SKIP_CLICKED;
    button.widget.state.return_value = text("AGAIN");
    CHECK(ab_ui_widget_update(&button.widget, 0, 1, NULL) &&
          button.state == AB_UI_SKIP_CLICKED &&
          button.widget.state.return_value.kind == AB_UI_POINTER_NONE &&
          real(button.widget.state.hover_scale) == 0);
    Fixture other;
    prepare(&other);
    AbUiHudSkipButton initial = {0};
    other.constructing = &initial;
    other.ending = number(0);
    CHECK(ab_ui_hud_skip_button_init(&initial, &other.context, (AbUiWidgetChildren){0},
                                     &other, ending, NULL) &&
          initial.state == AB_UI_SKIP_ONSCREEN && initial.scale_multiplier == 1);
    return true;
}

int main(void) {
    return pause_checks() && skip_checks() ? 0 : 1;
}
