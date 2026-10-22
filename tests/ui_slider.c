#include "ab/ui_slider.h"
#include "../tools/oracle/reference_ui_slider.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Slider check failed at %d: %s\n", __LINE__, #condition);  \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbUiSlider slider;
    AbUiWidgetContext context;
    AbUiScaling scaling;
    AbVec2 screen;
    AbUiImageRenderState render;
    AbDrawCommand commands[64];
    AbDrawList draws;
    AbSpriteRegion region;
    AbUiWidget content[3];
    AbUiSliderChild children[3], *items[3];
    AbUiFrameNode *frames[5];
    AbUiDrawChild draw[5];
    AbUiPointerChild pointer[5];
    unsigned sounds, queries, bounds_queries, base_calls;
    bool notification_visible;
} Fixture;

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

static float real(AbUiPointerValue value) {
    float output;
    memcpy(&output, &value.number_bits, sizeof(output));
    return output;
}

static bool localize(void *context, AbSpan name, AbSpan *output, AbError *error) {
    (void)context;
    (void)error;
    *output = name;
    return true;
}

static bool bounds(void *context, AbSpan name, AbVec2 *output, AbError *error) {
    Fixture *fixture = context;
    ++fixture->bounds_queries;
    (void)error;
    *output = name.size ? (AbVec2){40, 40} : (AbVec2){0};
    return true;
}

static bool pivot(void *context, AbSpan name, AbVec2 *output, AbError *error) {
    (void)context;
    (void)name;
    (void)error;
    *output = (AbVec2){20, 20};
    return true;
}

static bool lookup(void *context, AbSpan name, AbUiImageRenderBinding *output,
                   AbError *error) {
    Fixture *fixture = context;
    (void)error;
    *output = name.size ? (AbUiImageRenderBinding){.region = &fixture->region,
                                                   .texture = 7,
                                                   .texture_width = 64,
                                                   .texture_height = 64,
                                                   .alpha = AB_SPRITE_ALPHA_PRESENT}
                        : (AbUiImageRenderBinding){0};
    return true;
}

static bool audio(void *context, AbUiPointerValue sound, float gain, bool loop,
                  AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(sound.kind == AB_UI_POINTER_TEXT && gain == 1 && !loop);
    ++fixture->sounds;
    return true;
}

static bool environment(void *context, AbUiScalingEnvironment *output, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    *output = (AbUiScalingEnvironment){.screen = fixture->screen};
    return true;
}

static bool notifications(void *context, bool *output, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    ++fixture->queries;
    *output = fixture->notification_visible;
    return true;
}

static bool prepare(Fixture *fixture) {
    *fixture =
        (Fixture){.screen = {1200, 800},
                  .region = {.width = 40, .height = 40, .pivot_x = 20, .pivot_y = 20}};
    fixture->draws = (AbDrawList){fixture->commands, 0, 64};
    fixture->render.alpha = 1;
    fixture->scaling =
        (AbUiScaling){.host = {.context = fixture, .environment = environment},
                      .default_scaling = 1,
                      .initialized = true};
    fixture->context = (AbUiWidgetContext){.images = {fixture, localize, bounds, pivot},
                                           .render = {.context = fixture,
                                                      .lookup = lookup,
                                                      .viewport_width = 1200,
                                                      .viewport_height = 800,
                                                      .blend = AB_BLEND_PREMULTIPLIED,
                                                      .color = {1, 1, 1, 1}},
                                           .render_state = &fixture->render,
                                           .draw_list = &fixture->draws,
                                           .maximum_depth = 32,
                                           .animation_installed = true,
                                           .audio_context = fixture,
                                           .play_audio = audio};
    AbUiPointerValue padding[4] = {number(1), number(2), number(3), number(4)};
    CHECK(ab_ui_slider_init(
        &fixture->slider, &fixture->context,
        (AbUiWidgetChildren){fixture->frames, fixture->draw, fixture->pointer, 5},
        fixture->items, 3, NULL, padding,
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN},
        (AbUiSliderHost){&fixture->scaling, &fixture->screen, fixture, notifications},
        NULL));
    AbUiSlider *slider = &fixture->slider;
    CHECK(slider->enabled.kind == AB_UI_POINTER_BOOLEAN && slider->enabled.boolean &&
          real(slider->toggle.padding[2]) == 3 && slider->box.scale_it.boolean &&
          slider->extend_adjusted == 1 &&
          slider->box.components[0].kind == AB_UI_POINTER_NONE);
    CHECK(ab_ui_slider_images(slider, text("OPEN"), text("CLOSED"), false, NULL) &&
          ab_ui_slider_images(slider, text("INDICATOR"), text(""), true, NULL) &&
          ab_ui_slider_direction(slider, span("RIGHT"), NULL));
    slider->box.widget.frame->fields.x = number(100);
    slider->box.widget.frame->fields.y = number(100);
    slider->child_size = 20;
    slider->gap = 2;
    slider->padding.x = 4;
    slider->toggle.state.return_value = text("TOGGLE_POWERUPS");
    const char *const names[] = {"first", "second", "third"};
    for (size_t index = 0; index < 3; ++index) {
        AbUiImageArguments image = {.frame.name = text(names[index]),
                                    .image = text("CONTENT")};
        CHECK(ab_ui_widget_init(fixture->content + index, &fixture->context,
                                AB_UI_WIDGET_IMAGE, (AbUiWidgetChildren){0}, NULL,
                                &image, NULL, NULL));
        fixture->children[index].widget = fixture->content + index;
        CHECK(ab_ui_slider_add(slider, fixture->children + index, NULL));
    }
    CHECK(slider->box.widget.frame->child_count == 5 &&
          fixture->frames[0] == &fixture->content[0].tree &&
          fixture->frames[3] == &slider->toggle.tree &&
          fixture->frames[4] == &slider->indicator.tree &&
          ab_ui_widget_lifecycle(&slider->box.widget, AB_UI_FRAME_ENTRY, NULL));
    return true;
}

static bool animation_checks(void) {
    Fixture fixture;
    CHECK(prepare(&fixture));
    AbUiSlider *slider = &fixture.slider;
    AbUiWidget *widget = &slider->box.widget;
    float total, duration;
    float bounds[4];
    CHECK(ab_ui_slider_button_bounds(slider, bounds, NULL) && bounds[0] == 80 &&
          bounds[1] == 80 && bounds[2] == 120 && bounds[3] == 120);
    CHECK(ab_ui_slider_total(slider, &total, NULL) && total == 70 &&
          ab_ui_slider_duration(slider, &duration, NULL) && duration == 70.0f / 640);
    fixture.children[1].override_size = number(0);
    CHECK(ab_ui_slider_total(slider, &total, NULL) && total == 50);
    fixture.children[1].override_size = (AbUiPointerValue){0};
    CHECK(ab_ui_widget_update(widget, 0, 0, NULL) &&
          slider->state == AB_UI_SLIDER_CLOSED &&
          !fixture.content[2].frame->fields.visible.boolean &&
          ab_ui_slider_curve(slider, AB_UI_SLIDER_CUBIC_OUT, NULL) &&
          ab_ui_slider_open(slider, NULL) &&
          ab_ui_widget_update(widget, 0.125f, 0, NULL) &&
          slider->state == AB_UI_SLIDER_OPEN && slider->extend_state == 70 &&
          slider->extend_adjusted == 70 &&
          real(fixture.content[0].frame->fields.x) == 22);
    fixture.draws.count = 0;
    CHECK(ab_ui_widget_draw(widget, &(AbUiDrawInput){0}, NULL) &&
          fixture.draws.count == 5);
    CHECK(ab_ui_slider_close(slider, NULL) &&
          ab_ui_widget_update(widget, 0.0625f, 0, NULL) && slider->extend_state == 30 &&
          slider->extend_adjusted > 30 && slider->state == AB_UI_SLIDER_CLOSING);
    fixture.draws.count = 0;
    CHECK(ab_ui_widget_draw(widget, &(AbUiDrawInput){0}, NULL) &&
          fixture.render.clip.enabled && fixture.render.clip.left == 0 &&
          fixture.render.clip.right == 1200 && fixture.draws.count >= 2);
    CHECK(ab_ui_widget_update(widget, 0.046875f, 0, NULL) &&
          slider->extend_state == 0 && slider->state == AB_UI_SLIDER_CLOSING &&
          ab_ui_widget_update(widget, 0, 0, NULL) &&
          slider->state == AB_UI_SLIDER_CLOSING &&
          ab_ui_widget_update(widget, 0.01f, 0, NULL) &&
          slider->state == AB_UI_SLIDER_CLOSED);
    CHECK(ab_ui_slider_open(slider, NULL) &&
          ab_ui_widget_update(widget, 0.125f, 0, NULL) &&
          ab_ui_slider_enabled(
              slider,
              (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true},
              NULL) &&
          slider->state == AB_UI_SLIDER_CLOSING && ab_ui_slider_open(slider, NULL) &&
          ab_ui_widget_update(widget, 0.125f, 0, NULL) &&
          ab_ui_slider_disappear(slider, NULL) &&
          ab_ui_widget_update(widget, 0.125f, 0, NULL) && slider->extend_state == 0 &&
          slider->disappear_scale == 0.625f &&
          ab_ui_widget_update(widget, 0.25f, 0, NULL) &&
          slider->state == AB_UI_SLIDER_DISAPPEARED &&
          !widget->frame->fields.visible.boolean &&
          real(slider->toggle.frame->fields.scale_x) == 0);
    return true;
}

static bool pointer_checks(void) {
    Fixture fixture;
    CHECK(prepare(&fixture));
    AbUiSlider *slider = &fixture.slider;
    AbUiPointerReply reply;
    for (unsigned sample = 0; sample < 2; ++sample) {
        fixture.notification_visible = sample == 0;
        CHECK(ab_ui_widget_clear_pointer_flags(&slider->box.widget, NULL) &&
              ab_ui_widget_pointer(&slider->box.widget, span("LPRESS"),
                                   (AbVec2){100, 100}, &reply, NULL) &&
              ab_ui_widget_pointer(&slider->box.widget, span("LRELEASE"),
                                   (AbVec2){100, 100}, &reply, NULL) &&
              reply.values[2].object == (uintptr_t)&slider->toggle &&
              fixture.queries == sample + 1 &&
              slider->state == (sample ? AB_UI_SLIDER_OPENING : AB_UI_SLIDER_CLOSED));
    }
    fixture.children[0].offset_x = text("bad");
    CHECK(!ab_ui_widget_update(&slider->box.widget, 0.01f, 0, NULL) &&
          fixture.context.failed && !ab_ui_slider_open(slider, NULL));
    return true;
}

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool base_update(void *context, AbUiWidget *widget, float seconds, float time,
                        AbError *error) {
    Fixture *fixture = context;
    ++fixture->base_calls;
    return ab_ui_widget_update_base(widget, seconds, time, error);
}

static bool reference_case(unsigned index, uint32_t output[32]) {
    Fixture fixture;
    CHECK(prepare(&fixture));
    ReferenceUiSlider input = reference_ui_slider(index);
    AbUiSlider *slider = &fixture.slider;
    slider->count = input.count;
    slider->state = (AbUiSliderState)input.state;
    slider->open_curve = (AbUiSliderCurve)input.curve;
    slider->rotate_indicator = index % 8 != 0;
    const AbVec2 directions[] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
    slider->direction = directions[input.direction];
    slider->padding = (AbVec2){input.padding_x, input.padding_y};
    slider->gap = input.gap;
    slider->child_size = input.child_size;
    slider->extend_state = input.extend;
    slider->extend_adjusted = input.adjusted;
    slider->disappear_scale = input.disappear;
    slider->box.widget.frame->fields.scale_x = number(input.scale_x);
    slider->box.widget.frame->fields.scale_y = number(input.scale_y);
    fixture.screen.y = input.screen_height;
    fixture.scaling.default_scaling = input.scale;
    fixture.content[0].methods =
        (AbUiWidgetMethods){.context = &fixture, .update = base_update};
    for (unsigned child = 0; child < input.count; ++child) {
        fixture.children[child].offset_x = number(input.offsets[child][0]);
        fixture.children[child].offset_y = number(input.offsets[child][1]);
        fixture.children[child].override_size =
            input.override_kinds[child] == 2
                ? number(input.overrides[child])
                : (AbUiPointerValue){.kind = input.override_kinds[child] == 1
                                                 ? AB_UI_POINTER_BOOLEAN
                                                 : AB_UI_POINTER_NONE};
    }
    fixture.bounds_queries = 0;
    AbError error = {0};
    bool okay = ab_ui_widget_update(&slider->box.widget, input.seconds, 0, &error);
    if (!okay || fixture.bounds_queries % 3)
        fprintf(stderr, "Slider reference case %u, bounds %u: %s\n", index,
                fixture.bounds_queries, error.message);
    /* The real enabled ImageButton setter refreshes twice, then Image once. */
    CHECK(okay && fixture.bounds_queries % 3 == 0);
    memset(output, 0, 32 * sizeof(*output));
    output[0] = index;
    output[1] = (uint32_t)slider->state;
    output[2] = bits(slider->extend_state);
    output[3] = bits(slider->extend_adjusted);
    output[4] = bits(slider->disappear_scale);
    output[5] = slider->indicator.frame->fields.angle.number_bits;
    output[6] = slider->toggle.frame->fields.scale_x.number_bits;
    output[7] = slider->toggle.frame->fields.scale_y.number_bits;
    output[8] = slider->indicator.frame->fields.scale_x.number_bits;
    output[9] = slider->indicator.frame->fields.scale_y.number_bits;
    output[10] = slider->box.widget.frame->fields.visible.boolean;
    output[11] = bits(slider->box.width);
    output[12] = bits(slider->box.height);
    output[13] = fixture.bounds_queries / 3;
    output[14] = fixture.base_calls;
    output[15] = input.count;
    for (unsigned child = 0; child < input.count; ++child) {
        output[16 + child * 4] = fixture.content[child].frame->fields.x.number_bits;
        output[17 + child * 4] = fixture.content[child].frame->fields.y.number_bits;
        output[18 + child * 4] = fixture.content[child].frame->fields.visible.boolean;
        output[19 + child * 4] = (uint32_t)fixture.children[child].override_size.kind;
    }
    return true;
}

static bool reference(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB_ANDROID_UI_SLIDER_V1 512\n"));
    for (unsigned index = 0; index < REFERENCE_UI_SLIDER_CASES; ++index) {
        uint32_t output[32];
        CHECK(reference_case(index, output));
        for (size_t field = 0; field < 32; ++field) {
            uint32_t expected;
            CHECK(fscanf(file, "%" SCNx32, &expected) == 1);
            if (output[field] != expected) {
                fprintf(stderr,
                        "Slider case %u field %zu: expected %08" PRIx32
                        ", got %08" PRIx32 "\n",
                        index, field, expected, output[field]);
                return false;
            }
        }
    }
    CHECK(fscanf(file, " %79[^\n]", marker) == 1 &&
          !strcmp(marker, "AB UI Slider reference complete"));
    int byte;
    do {
        byte = fgetc(file);
    } while (byte == '\r' || byte == '\n');
    CHECK(byte == EOF && !ferror(file));
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2)
        return 2;
    if (!animation_checks() || !pointer_checks())
        return 1;
    if (argc == 2) {
        FILE *file = fopen(argv[1], "rb");
        if (!file)
            return 2;
        bool okay = reference(file);
        okay = fclose(file) == 0 && okay;
        if (!okay)
            return 1;
        puts("Slider update matches all 512 original method/curve snapshots.");
        return 0;
    }
    puts("Slider composition preserves animation boundaries, live child fields, "
         "clipping and notification-gated pointer routing; native whole-widget "
         "comparison remains open.");
    return 0;
}
