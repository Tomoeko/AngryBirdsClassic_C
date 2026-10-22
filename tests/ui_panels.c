#include "ab/ui_bg_box.h"
#include "ab/ui_scaling.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "UI panel check failed at %d: %s\n", __LINE__,             \
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

typedef struct {
    AbUiScalingEnvironment environment;
    AbSpriteRegion sprites[9];
    unsigned queries[64];
    size_t query_count;
    bool fail_environment;
} Fixture;

/* Controlled metadata/environment inputs test composition. APK-backed rendering
 * is checked separately; this fixture makes no original-pixel fidelity claim. */
static bool environment(void *context, AbUiScalingEnvironment *output, AbError *error) {
    Fixture *fixture = context;
    if (fixture->fail_environment)
        return ab_fail(error, "Prepared environment failure");
    *output = fixture->environment;
    return true;
}

static size_t image_index(Fixture *fixture, AbSpan name) {
    for (size_t index = 0; index < 9; ++index)
        if (name.size == fixture->sprites[index].name.size &&
            !memcmp(name.data, fixture->sprites[index].name.data, name.size))
            return index;
    return 9;
}

static bool bounds(void *context, AbSpan name, AbVec2 *output, AbError *error) {
    Fixture *fixture = context;
    size_t index = image_index(fixture, name);
    if (index == 9 || fixture->query_count == 64)
        return ab_fail(error, "Prepared sprite bounds unavailable");
    fixture->queries[fixture->query_count++] = (unsigned)index;
    *output = (AbVec2){fixture->sprites[index].width, fixture->sprites[index].height};
    return true;
}

static bool lookup(void *context, AbSpan name, AbUiImageRenderBinding *output,
                   AbError *error) {
    Fixture *fixture = context;
    size_t index = image_index(fixture, name);
    if (index == 9)
        return ab_fail(error, "Prepared sprite unavailable");
    *output = (AbUiImageRenderBinding){.region = fixture->sprites + index,
                                       .texture = (uint32_t)index + 1,
                                       .texture_width = 128,
                                       .texture_height = 128};
    return true;
}

static bool panels(void) {
    Fixture fixture = {0};
    AbDrawCommand commands[16];
    AbDrawList list = {commands, 0, 16};
    AbUiImageRenderState render = {.alpha = 0.5f};
    AbUiWidgetContext context = {.images = {.context = &fixture, .bounds = bounds},
                                 .render = {.context = &fixture,
                                            .lookup = lookup,
                                            .viewport_width = 320,
                                            .viewport_height = 200,
                                            .blend = AB_BLEND_PREMULTIPLIED,
                                            .color = {1, 1, 1, 1}},
                                 .render_state = &render,
                                 .draw_list = &list,
                                 .maximum_depth = 8};
    AbUiBgBox box = {0};
    AbUiFrameFields arguments = {
        .x = number(99), .y = number(88), .scale_x = number(2), .scale_y = number(3)};
    CHECK(ab_ui_bg_box_init(&box, &context, (AbUiWidgetChildren){0}, &arguments, NULL));
    CHECK(box.width == 500 && box.height == 500 &&
          box.widget.state.image.frame.fields.x.number_bits == 0 &&
          box.widget.state.image.frame.fields.y.number_bits == 0);
    const int16_t sizes[9][2] = {{16, 12}, {14, 10}, {16, 13}, {15, 13}, {10, 10},
                                 {20, 6},  {7, 20},  {8, 21},  {22, 9}};
    for (size_t index = 0; index < 9; ++index)
        fixture.sprites[index] = (AbSpriteRegion){.name = box.components[index].text,
                                                  .width = sizes[index][0],
                                                  .height = sizes[index][1]};
    box.horizontal_anchor = text("HCENTER");
    box.vertical_anchor = text("BOTTOM");
    ab_ui_bg_box_set_size(&box, 100, 50);
    AbUiDrawInput input = {.present = 3, .transform.position = {100, 100}};
    CHECK(ab_ui_widget_draw(&box.widget, &input, NULL));
    CHECK(list.count == 9 && render.alpha == 0.5f);
    const unsigned queries[] = {0, 1, 2, 3, 5, 6, 7, 8};
    CHECK(fixture.query_count == 8 &&
          !memcmp(fixture.queries, queries, sizeof(queries)));
    const uint32_t textures[] = {6, 9, 7, 8, 1, 2, 3, 4, 5};
    for (size_t index = 0; index < 9; ++index)
        CHECK(commands[index].texture == textures[index]);
    CHECK(commands[0].quad.vertices[0].x == 50.0f / 320 * 2 - 1);
    CHECK(commands[0].quad.vertices[0].y == 1 - 44.0f / 200 * 2);
    float total;
    CHECK(ab_ui_bg_box_total(&box, true, &total, NULL) && total == 115);
    CHECK(ab_ui_bg_box_total(&box, false, &total, NULL) && total == 65);
    ab_ui_bg_box_use_scale(
        &box, (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true});
    CHECK(ab_ui_bg_box_total(&box, true, &total, NULL) && total == 130);
    CHECK(ab_ui_bg_box_total(&box, false, &total, NULL) && total == 95);
    CHECK(ab_ui_bg_box_set_total(&box, true, 80, NULL) && box.width == 50);
    CHECK(ab_ui_bg_box_set_total(&box, false, 80, NULL) && box.height == 35);
    fixture.query_count = 0;
    ab_draw_list_clear(&list);
    box.components[AB_UI_BOX_BOTTOM_LEFT] = (AbUiPointerValue){0};
    box.components[AB_UI_BOX_BOTTOM_RIGHT] = (AbUiPointerValue){0};
    box.components[AB_UI_BOX_BOTTOM] = (AbUiPointerValue){0};
    CHECK(ab_ui_widget_draw(&box.widget, &input, NULL) && list.count == 6);
    CHECK(box.components[AB_UI_BOX_BOTTOM].kind == AB_UI_POINTER_TEXT &&
          box.components[AB_UI_BOX_BOTTOM].text.size == 0);
    CHECK(context.discarded_sprite_bound.number_bits == number(21).number_bits);
    ab_draw_list_clear(&list);
    fixture.query_count = 0;
    CHECK(ab_ui_bg_box_draw_box(&box, (AbVec2){100, -1000}, (AbVec2){50, 35}, NULL, 0,
                                NULL) &&
          list.count == 0 && fixture.query_count == 5);
    fixture.query_count = 0;
    AbUiPointerValue color[4] = {number(0.5f), {0}, number(0), number(0.25f)};
    CHECK(ab_ui_bg_box_draw_box(&box, (AbVec2){100, 100}, (AbVec2){50, 35}, color, 99,
                                NULL));
    CHECK(list.count == 6 && commands[5].texture == 99 &&
          commands[5].blend == AB_BLEND_STRAIGHT && render.alpha == 1);
    CHECK(fabsf(commands[5].color[3] - 63.0f / 255) < 0.0000001f);
    return true;
}

static bool scaling(void) {
    Fixture fixture = {.environment = {.screen = {480, 320}, .use_scaling = true}};
    AbEventManager *events = NULL;
    AbUiScaling owner = {0};
    AbDataValue id = {.kind = AB_DATA_STRING, .string = span("resolution-fixture")};
    CHECK(ab_event_manager_open((AbEventManagerLimits){2, 2, 0, 0, 4, 4096},
                                (AbEventManagerHost){0}, &events, NULL));
    CHECK(ab_ui_scaling_init(
        &owner, (AbUiScalingHost){events, id, &fixture, environment}, NULL));
    CHECK(owner.default_profile.x == 480 && owner.default_profile.y == 320);
    AbDataEntry entry = {{.kind = AB_DATA_STRING, .string = span("id")}, id};
    AbDataTable event = {&entry, 1, 1, NULL};
    fixture.environment.screen = (AbVec2){640, 480};
    CHECK(ab_event_manager_notify(events, &event, NULL));
    CHECK(owner.default_scaling == 640.0f / 480 && owner.default_profile.x == 480);
    fixture.environment.use_scaling = false;
    fixture.environment.screen = (AbVec2){960, 640};
    CHECK(ab_event_manager_notify(events, &event, NULL));
    CHECK(owner.default_scaling == 640.0f / 480);
    CHECK(ab_ui_scaling_update(&owner, NULL) && owner.default_scaling == 1);
    fixture.environment.use_scaling = true;
    CHECK(ab_event_manager_notify(events, &event, NULL) && owner.default_scaling == 2);
    AbUiFrameFields frame = {0};
    CHECK(ab_ui_scaling_set_to(&owner, &frame, NULL));
    CHECK(frame.scale_x.number_bits == number(2).number_bits &&
          frame.scale_y.number_bits == number(2).number_bits);
    CHECK(ab_ui_scaling_detach(&owner, NULL));
    fixture.environment.screen = (AbVec2){1440, 900};
    CHECK(ab_ui_scaling_init(
        &owner, (AbUiScalingHost){events, id, &fixture, environment}, NULL));
    CHECK(owner.default_profile.x == 1024 && owner.default_profile.y == 768 &&
          owner.default_scaling == 900.0f / 768);
    CHECK(ab_ui_scaling_detach(&owner, NULL));
    fixture.fail_environment = true;
    CHECK(!ab_ui_scaling_init(
              &owner, (AbUiScalingHost){events, id, &fixture, environment}, NULL) &&
          owner.registered && owner.failed);
    CHECK(ab_ui_scaling_detach(&owner, NULL));
    ab_event_manager_close(events);
    return true;
}

int main(void) {
    if (!panels() || !scaling())
        return 1;
    puts("Concrete BGBox rendering and live Scaling/EventManager composition pass.");
    return 0;
}
