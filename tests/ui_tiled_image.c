#include "ab/ui_tiled_image.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "TiledImage check failed at %d: %s\n", __LINE__,           \
                    #condition);                                                       \
            return 1;                                                                  \
        }                                                                              \
    } while (0)

typedef struct {
    AbSpriteRegion region;
    AbUiDrawTransform child_input;
    size_t child_calls, commands_before_child;
} Fixture;

static AbUiPointerValue number(float value) {
    AbUiPointerValue result = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&result.number_bits, &value, sizeof(value));
    return result;
}

static bool bounds(void *context, AbSpan image, AbVec2 *output, AbError *error) {
    (void)context;
    (void)image;
    (void)error;
    *output = (AbVec2){10, 6};
    return true;
}

static bool lookup(void *context, AbSpan image, AbUiImageRenderBinding *output,
                   AbError *error) {
    (void)image;
    (void)error;
    Fixture *fixture = context;
    *output = (AbUiImageRenderBinding){.region = &fixture->region,
                                       .texture = 1,
                                       .texture_width = 32,
                                       .texture_height = 32};
    return true;
}

static bool child_draw(void *context, AbUiWidget *widget, const AbUiDrawInput *input,
                       AbError *error) {
    (void)error;
    Fixture *fixture = context;
    ++fixture->child_calls;
    fixture->commands_before_child = widget->context->draw_list->count;
    return ab_ui_draw_resolve(input, &fixture->child_input);
}

int main(void) {
    AbError error = {0};
    Fixture fixture = {
        .region = {.width = 10, .height = 6, .pivot_x = 2, .pivot_y = 3}};
    AbDrawCommand commands[8];
    AbDrawList list = {commands, 0, 8};
    AbUiImageRenderState render = {.geometry = {.translation_x = 999,
                                                .scale_x = 8,
                                                .scale_y = 9,
                                                .angle_radians = 1,
                                                .direct_vertices = true},
                                   .alpha = .25f};
    AbUiWidgetContext context = {.images = {.context = &fixture, .bounds = bounds},
                                 .render = {.context = &fixture,
                                            .lookup = lookup,
                                            .viewport_width = 480,
                                            .viewport_height = 320,
                                            .blend = AB_BLEND_PREMULTIPLIED,
                                            .color = {1, 1, 1, 1}},
                                 .render_state = &render,
                                 .draw_list = &list,
                                 .maximum_depth = 16};
    AbUiFrameNode *frames[1];
    AbUiDrawChild draws[1];
    AbUiPointerChild pointers[1];
    AbUiTiledImage tiles = {0};
    AbUiWidget child = {0};
    AbUiFrameFields fields = {.x = number(100),
                              .y = number(200),
                              .scale_x = number(2),
                              .scale_y = number(2),
                              .angle = number(.25f)};
    CHECK(ab_ui_tiled_image_init(&tiles, &context,
                                 (AbUiWidgetChildren){frames, draws, pointers, 1},
                                 &fields, &error));
    CHECK(ab_ui_widget_init(&child, &context, AB_UI_WIDGET_FRAME,
                            (AbUiWidgetChildren){0}, NULL, NULL, NULL, &error));
    child.methods = (AbUiWidgetMethods){.context = &fixture, .draw = child_draw};
    CHECK(ab_ui_widget_add(&tiles.widget, &child, &error));
    CHECK(ab_ui_tiled_image_set_image(&tiles, (AbSpan){(const uint8_t *)"tile", 4},
                                      &error) &&
          ab_ui_tiled_image_repeat(&tiles, 3.9f, 2.9f, &error));
    CHECK(!ab_ui_tiled_image_repeat(&tiles, NAN, 2, NULL) && tiles.repeat_x == 3.9f &&
          !ab_ui_tiled_image_repeat(&tiles, 1, 0x1p24f, NULL));
    tiles.widget.frame->fields.w = number(99);
    CHECK(ab_ui_widget_lifecycle(&tiles.widget, AB_UI_FRAME_LAYOUT, &error));
    AbUiDrawInput input = {.transform = {.position = {7, 11},
                                         .scale = {3, 4},
                                         .angle = .5f,
                                         .local_factor = 5},
                           .present = 63};
    CHECK(ab_ui_widget_draw(&tiles.widget, &input, &error) && list.count == 6 &&
          fixture.child_calls == 1 && fixture.commands_before_child == 6);
    for (size_t index = 0; index < 6; ++index) {
        CHECK(commands[index].quad.vertices[0].x == 105 + (float)(index % 3) * 10 &&
              commands[index].quad.vertices[0].y == 208 + (float)(index / 3) * 6 &&
              commands[index].opacity == .25f);
    }
    CHECK(fixture.child_input.position.x == 107 &&
          fixture.child_input.position.y == 211 && fixture.child_input.scale.x == 6 &&
          fixture.child_input.scale.y == 8 && fixture.child_input.angle == .75f &&
          fixture.child_input.local_factor == 5 && render.geometry.direct_vertices &&
          render.alpha == .25f);
    list.count = 0;
    CHECK(ab_ui_tiled_image_repeat(&tiles, -1, 4, &error) &&
          ab_ui_widget_draw(&tiles.widget, &input, &error) && !list.count &&
          fixture.child_calls == 2);
    list.capacity = 2;
    CHECK(ab_ui_tiled_image_repeat(&tiles, 3, 2, &error) &&
          !ab_ui_widget_draw(&tiles.widget, &input, &error) && list.count == 2 &&
          fixture.child_calls == 2 && context.failed);
    puts("TiledImage: unscaled row-major tiles, pivot/alpha/direct state, fractional "
         "and empty loops, refreshed bounds, full child transform and failure prefix.");
    return 0;
}
