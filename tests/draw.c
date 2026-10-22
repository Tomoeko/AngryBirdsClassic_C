#include "ab/draw.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expression)                                                              \
    do {                                                                               \
        if (!(expression)) {                                                           \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression);           \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool sprite_anchors_and_uv(void) {
    AbError error = {0};
    AbSpriteRegion region = {
        .x = 32, .y = -1, .width = 63, .height = 31, .pivot_x = -51, .pivot_y = -4};
    AbSpritePlacement placement = {
        100, 200, 2, 0.5f, AB_ANCHOR_VPIVOT, AB_ANCHOR_HPIVOT};
    AbDrawQuad quad;
    CHECK(ab_sprite_quad(&region, 256, 128, placement, &quad, &error));
    CHECK(quad.vertices[0].x == 200 && quad.vertices[0].y == 201.5f);
    CHECK(quad.vertices[1].x == 326 && quad.vertices[1].y == 201.5f);
    CHECK(quad.vertices[2].x == 200 && quad.vertices[2].y == 217);
    CHECK(quad.vertices[3].x == 326 && quad.vertices[3].y == 217);
    CHECK(quad.vertices[0].u == 0.125f && quad.vertices[0].v == -0.0078125f);
    CHECK(quad.vertices[3].u == 0.37109375f && quad.vertices[3].v == 0.234375f);
    AbDrawQuad baseline;
    placement.vertical = AB_ANCHOR_BASELINE;
    CHECK(ab_sprite_quad(&region, 256, 128, placement, &baseline, &error));
    CHECK(memcmp(&baseline, &quad, sizeof(quad)) == 0);
    placement.vertical = AB_ANCHOR_VCENTER;
    placement.horizontal = AB_ANCHOR_HCENTER;
    CHECK(ab_sprite_quad(&region, 256, 128, placement, &quad, &error));
    CHECK(quad.vertices[0].x == 38 && quad.vertices[0].y == 192.5f);
    CHECK(quad.vertices[3].x == 164 && quad.vertices[3].y == 208);
    placement.vertical = AB_ANCHOR_BOTTOM;
    placement.horizontal = AB_ANCHOR_RIGHT;
    CHECK(ab_sprite_quad(&region, 256, 128, placement, &quad, &error));
    CHECK(quad.vertices[0].x == -26 && quad.vertices[0].y == 184.5f);
    CHECK(quad.vertices[3].x == 100 && quad.vertices[3].y == 200);
    placement.vertical = AB_ANCHOR_TOP;
    placement.horizontal = AB_ANCHOR_LEFT;
    placement.scale_x = -1;
    CHECK(ab_sprite_quad(&region, 256, 128, placement, &quad, &error));
    CHECK(quad.vertices[0].x == 100 && quad.vertices[0].y == 200);
    CHECK(quad.vertices[3].x == 37 && quad.vertices[3].y == 215.5f);
    CHECK(quad.vertices[0].u < quad.vertices[3].u);
    region.width = -3;
    region.height = -5;
    placement = (AbSpritePlacement){10, 10, 1, 1, AB_ANCHOR_VCENTER, AB_ANCHOR_HCENTER};
    CHECK(ab_sprite_quad(&region, 256, 128, placement, &quad, &error));
    CHECK(quad.vertices[0].x == 12 && quad.vertices[0].y == 13);
    CHECK(quad.vertices[3].x == 9 && quad.vertices[3].y == 8);
    region.width = 0;
    region.height = 0;
    CHECK(ab_sprite_quad(&region, 256, 128, placement, &quad, &error));
    CHECK(quad.vertices[0].x == quad.vertices[3].x);
    CHECK(quad.vertices[0].y == quad.vertices[3].y);
    CHECK(!ab_sprite_quad(&region, 0, 128, placement, &quad, &error));
    CHECK(!ab_sprite_quad(&region, UINT32_MAX, 128, placement, &quad, &error));
    CHECK(!ab_sprite_quad(NULL, 256, 128, placement, &quad, &error));
    CHECK(!ab_sprite_quad(&region, 256, 128, placement, NULL, &error));
    placement.vertical = (AbVerticalAnchor)99;
    CHECK(!ab_sprite_quad(&region, 256, 128, placement, &quad, &error));
    placement.vertical = AB_ANCHOR_TOP;
    placement.x = NAN;
    CHECK(!ab_sprite_quad(&region, 256, 128, placement, &quad, &error));
    placement.x = FLT_MAX;
    placement.scale_x = FLT_MAX;
    region.width = 2;
    CHECK(!ab_sprite_quad(&region, 256, 128, placement, &quad, &error));
    return true;
}

static bool transforms_and_order(void) {
    AbError error = {0};
    AbDrawQuad input = {{{2, 3, 0, 0}, {4, 3, 1, 0}, {2, 5, 0, 1}, {4, 5, 1, 1}}};
    AbDrawQuad quad;
    AbTransform2 transform = {0, -1, 10, 1, 0, 20};
    CHECK(ab_quad_transform(&input, transform, &quad, &error));
    CHECK(quad.vertices[0].x == 7 && quad.vertices[0].y == 22);
    CHECK(quad.vertices[3].x == 5 && quad.vertices[3].y == 24);
    CHECK(quad.vertices[3].u == 1 && quad.vertices[3].v == 1);
    CHECK(ab_quad_transform(&quad, transform, &quad, &error));
    CHECK(quad.vertices[0].x == -12 && quad.vertices[0].y == 27);
    input.vertices[0].x = 16777216.0f;
    input.vertices[0].y = -16777216.0f;
    transform = (AbTransform2){1, 1, 1, 0, 1, 0};
    CHECK(ab_quad_transform(&input, transform, &quad, &error));
    CHECK(quad.vertices[0].x == 1);
    transform.xx = NAN;
    CHECK(!ab_quad_transform(&input, transform, &quad, &error));
    transform.xx = FLT_MAX;
    CHECK(!ab_quad_transform(&input, transform, &quad, &error));
    CHECK(!ab_quad_transform(NULL, transform, &quad, &error));
    return true;
}

static bool draw_list_capacity_and_lifetime(void) {
    AbDrawList list = {0};
    AbError error = {0};
    CHECK(!ab_draw_list_init(2, 2 * sizeof(AbDrawCommand) - 1, &list, &error));
    CHECK(!list.commands && !list.capacity);
    CHECK(!ab_draw_list_init(SIZE_MAX, SIZE_MAX, &list, &error));
    CHECK(ab_draw_list_init(2, 2 * sizeof(AbDrawCommand), &list, &error));
    AbDrawCommand command = {.texture = 7, .color = {1, 0.5f, 0.25f, 1}, .opacity = 1};
    CHECK(ab_draw_list_push(&list, &command, &error));
    command.texture = 3;
    command.quad.vertices[0].x = 5;
    CHECK(ab_draw_list_push(&list, &command, &error));
    CHECK(list.count == 2 && list.commands[0].texture == 7 &&
          list.commands[1].texture == 3);
    CHECK(list.commands[0].quad.vertices[0].x == 0);
    CHECK(!ab_draw_list_push(&list, &command, &error));
    CHECK(list.count == 2);
    AbDrawCommand *allocation = list.commands;
    ab_draw_list_clear(&list);
    CHECK(list.count == 0 && list.commands == allocation && list.capacity == 2);
    command.color[0] = NAN;
    CHECK(!ab_draw_list_push(&list, &command, &error));
    command.color[0] = 1;
    command.quad.vertices[0].u = INFINITY;
    CHECK(!ab_draw_list_push(&list, &command, &error));
    CHECK(list.count == 0);
    ab_draw_list_free(&list);
    CHECK(!list.commands && !list.count && !list.capacity);
    CHECK(ab_draw_list_init(0, 0, &list, &error));
    CHECK(!ab_draw_list_push(&list, &command, &error));
    ab_draw_list_free(&list);
    return true;
}

int main(void) {
    if (!sprite_anchors_and_uv() || !transforms_and_order() ||
        !draw_list_capacity_and_lifetime()) {
        return 1;
    }
    puts("Sprite geometry, affine ordering, and draw-list checks passed.");
    return 0;
}
