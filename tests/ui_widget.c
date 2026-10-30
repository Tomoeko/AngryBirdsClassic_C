#include "ab/ui_widget.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Concrete UI check failed at %d: %s\n", __LINE__,          \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbUiPointerValue text(const char *name) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = span(name)};
}

static AbUiPointerValue number(float value) {
    AbUiPointerValue result = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&result.number_bits, &value, sizeof(value));
    return result;
}

static float value(AbUiPointerValue field) {
    float result;
    memcpy(&result, &field.number_bits, sizeof(result));
    return result;
}

static bool named(AbUiPointerValue field, const char *name) {
    return field.kind == AB_UI_POINTER_TEXT && field.text.size == strlen(name) &&
           !memcmp(field.text.data, name, field.text.size);
}

typedef struct {
    AbSpriteRegion sprite;
    unsigned sounds;
    bool mutate, fail_bounds;
} Fixture;

/* Explicit resource/audio controls exercise the production tree composition;
 * the existing native widget corpora establish the component semantics. */
static bool localize(void *context, AbSpan name, AbSpan *output, AbError *error) {
    (void)context;
    (void)error;
    *output = name;
    return true;
}

static bool bounds(void *context, AbSpan name, AbVec2 *output, AbError *error) {
    (void)name;
    if (((Fixture *)context)->fail_bounds)
        return ab_fail(error, "Prepared hit-test resource failure");
    *output = (AbVec2){32, 16};
    return true;
}

static bool pivot(void *context, AbSpan name, AbVec2 *output, AbError *error) {
    (void)context;
    (void)name;
    (void)error;
    *output = (AbVec2){16, 8};
    return true;
}

static bool lookup(void *context, AbSpan name, AbUiImageRenderBinding *output,
                   AbError *error) {
    Fixture *fixture = context;
    (void)name;
    (void)error;
    *output = (AbUiImageRenderBinding){.region = &fixture->sprite,
                                       .texture = 7,
                                       .texture_width = 64,
                                       .texture_height = 64};
    return true;
}

static bool audio(void *context, AbUiPointerValue sound, float gain, bool loop,
                  AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(named(sound, "menu_confirm") && gain == 1 && !loop);
    ++fixture->sounds;
    return true;
}

static bool child_update(void *context, AbUiWidget *widget, float seconds, float time,
                         AbError *error) {
    Fixture *fixture = context;
    if (fixture->mutate) {
        AbUiWidget *parent = widget->tree.parent->context;
        parent->state.hover_scale = number(0.25f);
        parent->state.hover_state = text("ENTER");
        parent->state.clear_pointer_flags =
            (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    }
    return ab_ui_widget_update_base(widget, seconds, time, error);
}

static bool tree_checks(void) {
    Fixture fixture = {
        .sprite = {.width = 32, .height = 16, .pivot_x = 16, .pivot_y = 8}};
    AbDrawCommand commands[8];
    AbDrawList list = {commands, 0, 8};
    AbUiImageRenderState render = {.alpha = 1};
    AbUiWidgetContext context = {.images = {&fixture, localize, bounds, pivot},
                                 .render = {.context = &fixture,
                                            .lookup = lookup,
                                            .viewport_width = 480,
                                            .viewport_height = 320,
                                            .blend = AB_BLEND_PREMULTIPLIED,
                                            .color = {1, 1, 1, 1}},
                                 .render_state = &render,
                                 .draw_list = &list,
                                 .maximum_depth = 32,
                                 .animation_installed = true,
                                 .scale_buttons_on_touch = true,
                                 .audio_context = &fixture,
                                 .play_audio = audio};
    AbUiWidget root = {0}, button = {0}, icon = {0};
    AbUiFrameNode *root_frames[2], *button_frames[2];
    AbUiDrawChild root_draw[2], button_draw[2];
    AbUiPointerChild root_pointer[2], button_pointer[2];
    AbUiFrameFields frame = {
        .name = text("mainMenu"), .x = number(10), .y = number(20)};
    AbUiImageButtonArguments arguments = {
        .image.frame = {.name = text("playButton"), .x = number(100), .y = number(60)},
        .return_value = text("PLAY"),
        .meta = text("episodeSelection")};
    CHECK(
        ab_ui_widget_init(&root, &context, AB_UI_WIDGET_FRAME,
                          (AbUiWidgetChildren){root_frames, root_draw, root_pointer, 2},
                          &frame, NULL, NULL, NULL) &&
        ab_ui_widget_init(
            &button, &context, AB_UI_WIDGET_IMAGE_BUTTON,
            (AbUiWidgetChildren){button_frames, button_draw, button_pointer, 2}, NULL,
            NULL, &arguments, NULL) &&
        ab_ui_image_button_state_set_image(&button.state, text("BUTTON"),
                                           (AbUiPointerValue){0}, &context.images,
                                           NULL));
    arguments.image.frame =
        (AbUiFrameFields){.name = text("icon"), .x = number(4), .y = number(5)};
    arguments.return_value = text("ICON");
    CHECK(ab_ui_widget_init(&icon, &context, AB_UI_WIDGET_IMAGE_BUTTON,
                            (AbUiWidgetChildren){0}, NULL, NULL, &arguments, NULL) &&
          ab_ui_image_button_state_set_image(&icon.state, text("ICON"),
                                             (AbUiPointerValue){0}, &context.images,
                                             NULL) &&
          ab_ui_widget_add(&root, &button, NULL) &&
          ab_ui_widget_add(&button, &icon, NULL) &&
          ab_ui_widget_lifecycle_base(&root, AB_UI_FRAME_ENTRY, NULL) &&
          value(context.lifecycle.entry_counts) == 3 && root.tree.entered.boolean &&
          button.tree.entered.boolean && icon.tree.entered.boolean);
    AbUiDrawInput input = {0};
    CHECK(ab_ui_widget_draw(&root, &input, NULL) && list.count == 2 &&
          list.commands[0].texture == 7 && list.commands[1].texture == 7 &&
          fabsf(list.commands[0].quad.vertices[0].x - (94.0f / 240 - 1)) < 0.000001f &&
          fabsf(list.commands[0].quad.vertices[0].y - (1 - 72.0f / 160)) < 0.000001f);
    AbUiPointerReply reply = {0};
    CHECK(
        ab_ui_widget_pointer(&root, span("LPRESS"), (AbVec2){110, 80}, &reply, NULL) &&
        named(reply.values[0], "CLICK") && button.state.click_started.boolean &&
        !fixture.sounds &&
        ab_ui_widget_pointer(&root, span("LRELEASE"), (AbVec2){110, 80}, &reply,
                             NULL) &&
        named(reply.values[0], "PLAY") && named(reply.values[1], "episodeSelection") &&
        reply.values[2].object == (uintptr_t)&button && fixture.sounds == 1 &&
        button.state.clear_pointer_flags.boolean &&
        ab_ui_widget_update(&root, 0, 4, NULL) && !button.state.click_started.boolean &&
        !button.state.clear_pointer_flags.boolean);
    icon.state.click_started =
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    CHECK(ab_ui_widget_clear_pointer_flags(&root, NULL) &&
          icon.state.click_started.boolean);
    icon.state.click_started.boolean = false;
    root.scroll[0] = number(3);
    root.scroll[1] = number(4);
    context.cursor = (AbVec2){107, 76};
    context.left_button_held = true;
    CHECK(
        ab_ui_widget_pointer(&root, span("LPRESS"), (AbVec2){110, 80}, &reply, NULL) &&
        ab_ui_widget_update(&root, 0.025f, 4, NULL) &&
        value(button.state.hover_scale) == -0.5f &&
        value(button.state.image.frame.fields.x) == 100 &&
        value(button.state.image.frame.fields.y) == 60);
    fixture.mutate = true;
    context.scale_buttons_on_touch = false;
    icon.methods.context = &fixture;
    icon.methods.update = child_update;
    CHECK(ab_ui_widget_update(&root, 0.1f, 4, NULL) &&
          value(button.state.hover_scale) == 0.25f + 0.1f * 6.66f &&
          named(button.state.hover_state, "ENTER") &&
          !button.state.click_started.boolean &&
          !button.state.clear_pointer_flags.boolean);
    fixture.mutate = false;
    CHECK(ab_ui_widget_lifecycle_base(&root, AB_UI_FRAME_EXIT, NULL) &&
          value(context.lifecycle.entry_counts) == 0);
    context.play_audio = NULL;
    reply.values[0] = number(123);
    CHECK(ab_ui_widget_pointer(&root, span("LPRESS"), (AbVec2){110, 80}, &reply, NULL));
    reply.values[0] = number(123);
    CHECK(!ab_ui_widget_pointer(&root, span("LRELEASE"), (AbVec2){110, 80}, &reply,
                                NULL) &&
          context.failed && button.state.click_started.boolean &&
          value(reply.values[0]) == 123 && fixture.sounds == 1 &&
          !ab_ui_widget_draw(&root, &input, NULL));
    return true;
}

static bool image_hit_checks(void) {
    Fixture fixture = {0};
    AbUiWidgetContext context = {.images = {&fixture, localize, bounds, pivot},
                                 .maximum_depth = 16};
    AbUiWidget image = {0};
    AbUiImageArguments arguments = {.frame = {.x = number(10),
                                              .y = number(20),
                                              .scale_x = number(2),
                                              .scale_y = number(-1)},
                                    .image = text("image")};
    CHECK(ab_ui_widget_init(&image, &context, AB_UI_WIDGET_IMAGE,
                            (AbUiWidgetChildren){0}, NULL, &arguments, NULL, NULL));
    image.frame->fields.w = image.frame->fields.h = number(0);
    image.state.image.resource_pivot_x = image.state.image.resource_pivot_y =
        number(999);
    bool hit = false;
    CHECK(ab_ui_widget_image_hit(&image, (AbVec2){10, 20}, &hit, NULL) && hit);
    CHECK(ab_ui_widget_image_hit(&image, (AbVec2){42, 20}, &hit, NULL) && !hit);
    image.state.padding = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    CHECK(ab_ui_widget_image_hit(&image, (AbVec2){10, 20}, &hit, NULL) && hit);
    image.state.padding = (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT, .object = 1};
    CHECK(!ab_ui_widget_image_hit(&image, (AbVec2){10, 20}, &hit, NULL));
    fixture.fail_bounds = true;
    image.frame->fields.scale_x = number(0);
    image.frame->fields.angle = number(NAN);
    CHECK(ab_ui_widget_image_hit(&image, (AbVec2){10, 20}, &hit, NULL) && !hit);
    puts("Image-derived hit tests: actual resources, absent/false padding, strict "
         "edges and zero-scale early return.");
    return true;
}

int main(void) {
    return tree_checks() && image_hit_checks() ? 0 : 1;
}
