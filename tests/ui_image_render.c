#include "ab/ui_image_render.h"

#include "ui_widget_draw_fixture.h"

#include "ab/rotation.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "UI image render failed at %d: %s\n", __LINE__,            \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbUiImageRenderState *state;
    AbUiImageRenderBinding binding;
    float seen_alpha;
    size_t lookups;
    bool fail;
    AbUiImageRenderState seen_state;
} Context;

static bool lookup(void *context, AbSpan image, AbUiImageRenderBinding *output,
                   AbError *error) {
    (void)error;
    Context *provider = context;
    CHECK((image.size == 5 && memcmp(image.data, "image", 5) == 0) ||
          (image.size == 17 && memcmp(image.data, "controlled-sprite", 17) == 0));
    provider->lookups++;
    provider->seen_alpha = provider->state->alpha;
    provider->seen_state = *provider->state;
    *output = provider->binding;
    return !provider->fail;
}

static bool edge_checks(void) {
    AbUiImageDrawPlan plan = {.count = 3};
    plan.calls[0].kind = AB_UI_IMAGE_RENDER_STATE;
    plan.calls[0].state.count = 8;
    const float values[] = {10, 20, 1, 1, 0, 0, 0, 0.5f};
    memcpy(plan.calls[0].state.arguments, values, sizeof(values));
    plan.calls[1] = (AbUiImageDrawCall){
        .kind = AB_UI_IMAGE_SPRITE, .sprite = {{(const uint8_t *)"image", 5}, {8, 6}}};
    plan.calls[2] = (AbUiImageDrawCall){
        .kind = AB_UI_IMAGE_RENDER_STATE,
        .state = {.arguments = {0, 0, 1, 1, 0, 0, 0, 1}, .count = 8}};
    AbSpriteRegion region = {
        .x = 2, .y = 4, .width = 10, .height = 9, .pivot_x = 2, .pivot_y = 3};
    AbUiImageRenderState state = {.alpha = 0.25f};
    Context provider = {.state = &state,
                        .binding = {.region = &region,
                                    .texture = 7,
                                    .texture_width = 32,
                                    .texture_height = 32}};
    AbUiImageRenderBoundaries boundaries = {.context = &provider,
                                            .lookup = lookup,
                                            .viewport_width = 480,
                                            .viewport_height = 320,
                                            .blend = AB_BLEND_PREMULTIPLIED,
                                            .color = {1, 1, 1, 1}};
    AbDrawCommand commands[2];
    AbDrawList list = {commands, 0, 2};
    CHECK(ab_ui_image_render_plan(&plan, &state, &boundaries, &list, NULL) &&
          provider.lookups == 1 && provider.seen_alpha == 0.5f && state.alpha == 1 &&
          list.count == 1 && commands[0].opacity == 0.5f &&
          commands[0].coordinates == AB_DRAW_CLIP && commands[0].texture == 7 &&
          commands[0].quad.vertices[0].u == 2.0f / 32 &&
          commands[0].quad.vertices[3].u == 12.0f / 32 &&
          commands[0].quad.vertices[0].x == -1.0f + (2.0f / 480) * 8 &&
          commands[0].quad.vertices[0].y == 1.0f + (-2.0f / 320) * 17);
    list.count = 0;
    state.alpha = 0.25f;
    state.geometry.direct_vertices = true;
    plan.calls[0].state.count = plan.calls[2].state.count = 7;
    CHECK(ab_ui_image_render_plan(&plan, &state, &boundaries, &list, NULL) &&
          state.alpha == 0.25f && state.geometry.direct_vertices && list.count == 1 &&
          commands[0].opacity == 0.25f && commands[0].quad.vertices[0].x == -2 &&
          commands[0].quad.vertices[0].y == -3 && commands[0].quad.vertices[3].x == 6 &&
          commands[0].quad.vertices[3].y == 3);
    AbUiImageRenderBinding existing = provider.binding;
    provider.binding = (AbUiImageRenderBinding){0};
    CHECK(ab_ui_image_render_plan(&plan, &state, &boundaries, &list, NULL) &&
          list.count == 1 && state.alpha == 0.25f);
    provider.binding.texture = 7;
    CHECK(!ab_ui_image_render_plan(&plan, &state, &boundaries, &list, NULL) &&
          list.count == 1);
    provider.binding = existing;
    provider.fail = true;
    plan.calls[0].state.count = 8;
    CHECK(!ab_ui_image_render_plan(&plan, &state, &boundaries, &list, NULL) &&
          state.alpha == 0.5f && state.geometry.translation_x == 10 && list.count == 1);
    size_t lookups = provider.lookups;
    AbUiImageDrawPlan empty = {.culled = true};
    CHECK(ab_ui_image_render_plan(&empty, &state, &boundaries, &list, NULL) &&
          provider.lookups == lookups && state.alpha == 0.5f && list.count == 1);
    plan.calls[0].state.arguments[0] = NAN;
    CHECK(!ab_ui_image_render_plan(&plan, &state, &boundaries, &list, NULL) &&
          state.geometry.translation_x == 10 && provider.lookups == lookups);
    provider.fail = false;
    state.geometry.direct_vertices = false;
    plan.calls[0].state.arguments[0] = INFINITY;
    plan.calls[0].state.arguments[1] = NAN;
    plan.calls[0].state.arguments[2] = plan.calls[0].state.arguments[3] = 0;
    CHECK(ab_ui_image_render_plan(&plan, &state, &boundaries, &list, NULL) &&
          provider.lookups == lookups + 1 && list.count == 1 &&
          isinf(provider.seen_state.geometry.translation_x) &&
          isnan(provider.seen_state.geometry.translation_y) &&
          state.geometry.translation_x == 0 && state.geometry.scale_x == 1);
    puts("UI image render order, alpha preservation/reset, explicit size, UVs, "
         "direct vertices and failure prefixes pass.");
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static uint32_t bits(float value) {
    uint32_t output;
    memcpy(&output, &value, sizeof(output));
    return output;
}

static float number(uint32_t word) {
    float output;
    memcpy(&output, &word, sizeof(output));
    return output;
}

static bool encode_state(const AbUiImageRenderState *state, uint32_t output[24]) {
    memset(output, 0, 24 * sizeof(*output));
    AbRotation rotation;
    CHECK(ab_rotation_from_angle(state->geometry.angle_radians, &rotation));
    output[4] = output[7] = bits(rotation.cosine);
    output[5] = bits(rotation.sine);
    output[6] = bits(-rotation.sine);
    output[8] = bits(state->geometry.translation_x);
    output[9] = bits(state->geometry.translation_y);
    output[10] = bits(state->geometry.scale_x);
    output[11] = bits(state->geometry.scale_y);
    output[12] = bits(state->geometry.rotation_origin_x);
    output[13] = bits(state->geometry.rotation_origin_y);
    output[16] = bits(state->alpha);
    output[18] = bits(state->geometry.angle_radians);
    output[19] = state->geometry.direct_vertices;
    return true;
}

static bool compare_commands(FILE *file, const AbUiImageDrawPlan *plan) {
    uint32_t count;
    CHECK(words(file, &count, 1) && count == plan->count);
    for (size_t index = 0; index < plan->count; ++index) {
        uint32_t expected[10], actual[10] = {0};
        CHECK(words(file, expected, 10));
        const AbUiImageDrawCall *call = plan->calls + index;
        actual[0] = (uint32_t)call->kind;
        if (call->kind == AB_UI_IMAGE_RENDER_STATE) {
            actual[1] = (uint32_t)call->state.count;
            for (size_t field = 0; field < call->state.count; ++field) {
                actual[2 + field] = bits(call->state.arguments[field]);
            }
        } else {
            actual[1] = 8;
            actual[4] = bits(call->sprite.size.x);
            actual[5] = bits(call->sprite.size.y);
        }
        CHECK(memcmp(actual, expected, sizeof(actual)) == 0);
    }
    return true;
}

static bool compare_image(const uint32_t expected[67], const Context *provider,
                          const AbUiImageDrawPlan *plan, const AbDrawList *list) {
    uint32_t state[24];
    CHECK(provider->lookups == 1 && encode_state(&provider->seen_state, state) &&
          memcmp(state, expected + 4, sizeof(state)) == 0);
    const AbUiImageDrawCall *sprite = NULL;
    for (size_t index = 0; index < plan->count; ++index) {
        if (plan->calls[index].kind == AB_UI_IMAGE_SPRITE) {
            sprite = plan->calls + index;
        }
    }
    CHECK(sprite);
    AbSpriteSizedPlacement placement = {0,
                                        0,
                                        sprite->sprite.size.x,
                                        sprite->sprite.size.y,
                                        AB_ANCHOR_VPIVOT,
                                        AB_ANCHOR_HPIVOT};
    const AbUiImageRenderBinding *binding = &provider->binding;
    AbDrawQuad local;
    CHECK(ab_sprite_sized_quad(binding->region, binding->texture_width,
                               binding->texture_height, placement, &local, NULL));
    CHECK(expected[0] == bits(local.vertices[0].x) &&
          expected[1] == bits(local.vertices[0].y) &&
          expected[2] == bits(sprite->sprite.size.x) &&
          expected[3] == bits(sprite->sprite.size.y));
    for (size_t vertex = 0; vertex < 4; ++vertex) {
        CHECK(expected[28 + vertex * 2] == bits(local.vertices[vertex].u) &&
              expected[29 + vertex * 2] == bits(local.vertices[vertex].v));
    }
    CHECK(expected[36] <= 1 && list->count == expected[36]);
    if (!list->count) {
        return true;
    }
    const size_t order[] = {0, 1, 2, 2, 1, 3};
    const AbDrawCommand *command = list->commands;
    CHECK(command->opacity == provider->seen_state.alpha &&
          command->coordinates == AB_DRAW_CLIP && command->texture == 7);
    for (size_t vertex = 0; vertex < 6; ++vertex) {
        const AbDrawVertex *point = command->quad.vertices + order[vertex];
        CHECK(expected[37 + vertex * 3] == bits(point->x) &&
              expected[38 + vertex * 3] == bits(point->y) &&
              expected[39 + vertex * 3] ==
                  (provider->seen_state.geometry.direct_vertices
                       ? 0
                       : UINT32_C(0x3a83126f)) &&
              expected[55 + vertex * 2] == bits(point->u) &&
              expected[56 + vertex * 2] == bits(point->v));
    }
    return true;
}

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_UI_IMAGE_RENDER_V1 4096\n") == 0);
    size_t images = 0, quads = 0, script_culls = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t setup[25], extra[19];
        CHECK(words(file, setup, 25) && words(file, extra, 19));
        AbSpan sprite = {(const uint8_t *)"controlled-sprite", 17};
        AbUiImageState widget = widget_image(setup, sprite);
        AbUiImageDraw image;
        CHECK(ab_ui_image_state_draw_view(&widget, &image));
        AbUiDrawInput input = {.transform = {{number(setup[7]), number(setup[8])},
                                             {number(setup[9]), number(setup[10])},
                                             number(setup[11]),
                                             number(setup[12])},
                               .present = setup[2]};
        AbUiImageDrawPlan plan;
        CHECK(ab_ui_image_draw_plan(&image, &input,
                                    (AbVec2){number(setup[5]), number(setup[6])},
                                    &plan) &&
              compare_commands(file, &plan));
        AbSpriteRegion region = {.x = (int16_t)extra[0],
                                 .y = (int16_t)extra[1],
                                 .width = (int16_t)extra[2],
                                 .height = (int16_t)extra[3],
                                 .pivot_x = (int16_t)extra[4],
                                 .pivot_y = (int16_t)extra[5]};
        AbUiImageRenderState state = {
            .geometry = {.translation_x = number(extra[12]),
                         .translation_y = number(extra[13]),
                         .scale_x = number(extra[14]),
                         .scale_y = number(extra[15]),
                         .angle_radians = number(extra[16]),
                         .rotation_origin_x = number(extra[17]),
                         .rotation_origin_y = number(extra[18]),
                         .direct_vertices = extra[11] != 0},
            .alpha = number(extra[10])};
        Context provider = {.state = &state,
                            .binding = {.region = &region,
                                        .texture = 7,
                                        .texture_width = extra[6],
                                        .texture_height = extra[7]}};
        AbUiImageRenderBoundaries boundaries = {.context = &provider,
                                                .lookup = lookup,
                                                .viewport_width = extra[8],
                                                .viewport_height = extra[9],
                                                .blend = AB_BLEND_PREMULTIPLIED,
                                                .color = {1, 1, 1, 1}};
        AbDrawCommand command = {0};
        AbDrawList list = {&command, 0, 1};
        CHECK(ab_ui_image_render_plan(&plan, &state, &boundaries, &list, NULL));
        uint32_t count;
        CHECK(words(file, &count, 1) && count <= 1 && provider.lookups == count);
        if (count) {
            uint32_t event[67];
            CHECK(words(file, event, 67));
            if (!compare_image(event, &provider, &plan, &list)) {
                fprintf(stderr, "UI image rendering case %zu differs\n", item);
                return false;
            }
        } else {
            CHECK(!list.count);
        }
        uint32_t final[24], actual[24];
        CHECK(words(file, final, 24) && encode_state(&state, actual));
        if (memcmp(actual, final, sizeof(actual)) != 0) {
            fprintf(stderr, "UI image rendering case %zu final state differs\n", item);
            return false;
        }
        images += count;
        quads += list.count;
        script_culls += plan.culled;
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB UI image render reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 original UI image render paths, %zu Sprite images, %zu native "
           "batch quads, %zu script culls and final render states match.\n",
           images, quads, script_culls);
    return true;
}

typedef struct {
    AbResolvedComposite composite;
    AbUiImageRenderBinding images[2];
    size_t count, fail_at;
} CompositeFixture;

static bool composite_lookup(void *context, AbSpan image,
                             AbUiImageRenderBinding *output, AbError *error) {
    (void)image;
    (void)error;
    CompositeFixture *fixture = context;
    *output = (AbUiImageRenderBinding){.composite = fixture->composite};
    return true;
}

static bool composite_part(void *context, const AbSpriteBinding *binding,
                           AbUiImageRenderBinding *output, AbError *error) {
    (void)error;
    CompositeFixture *fixture = context;
    size_t index = fixture->count++;
    CHECK(index < 2 && binding == fixture->composite.parts + index);
    if (fixture->fail_at == index + 1)
        return false;
    *output = fixture->images[index];
    return true;
}

static bool composite_checks(void) {
    AbSpriteRegion regions[] = {{.width = 4, .height = 2},
                                {.width = 6, .height = 4, .pivot_x = -1}};
    AbSpriteBinding bindings[] = {{.region = regions}, {.region = regions + 1}};
    AbCompositePart parts[] = {{.x = 3, .y = 4}, {.x = -2, .y = -3}};
    AbComposite source = {.parts = parts, .part_count = 2};
    CompositeFixture fixture = {
        .composite = {.source = &source, .parts = bindings, .bounds = {15, 9, 2, 3}},
        .images = {{.region = regions,
                    .alpha = AB_SPRITE_ALPHA_PRESENT,
                    .texture = 8,
                    .texture_width = 32,
                    .texture_height = 32},
                   {.region = regions + 1,
                    .alpha = AB_SPRITE_ALPHA_PRESENT,
                    .texture = 9,
                    .texture_width = 64,
                    .texture_height = 64}}};
    AbUiImageRenderBoundaries boundaries = {.context = &fixture,
                                            .lookup = composite_lookup,
                                            .part = composite_part,
                                            .viewport_width = 480,
                                            .viewport_height = 320,
                                            .blend = AB_BLEND_PREMULTIPLIED,
                                            .color = {1, 1, 1, 1}};
    AbUiImageRenderState state = {.geometry = {.translation_x = 10,
                                               .translation_y = 20,
                                               .scale_x = 2,
                                               .scale_y = 3},
                                  .alpha = 0.5f};
    AbUiImageRenderState saved = state;
    AbUiSpriteRender sprite = {.position = {8, 6},
                               .size = {999, 999},
                               .vertical = AB_ANCHOR_VPIVOT,
                               .horizontal = AB_ANCHOR_HPIVOT,
                               .sized = true};
    AbDrawCommand commands[2];
    AbDrawList list = {commands, 0, 2};
    CHECK(ab_ui_sprite_render(&sprite, &state, &boundaries, &list, NULL) &&
          list.count == 2 && fixture.count == 2 &&
          !memcmp(&state, &saved, sizeof(state)) && commands[0].texture == 8 &&
          commands[1].texture == 9 && commands[0].opacity == 0.5f &&
          commands[0].quad.vertices[0].x == -1 + (2.0f / 480) * 42 &&
          commands[0].quad.vertices[0].y == 1 + (-2.0f / 320) * 90);
    AbDrawCommand sized[2];
    memcpy(sized, commands, sizeof(sized));
    list.count = fixture.count = 0;
    sprite.sized = false;
    CHECK(ab_ui_sprite_render(&sprite, &state, &boundaries, &list, NULL) &&
          !memcmp(sized, commands, sizeof(sized)));
    list.count = fixture.count = 0;
    fixture.fail_at = 2;
    CHECK(!ab_ui_sprite_render(&sprite, &state, &boundaries, &list, NULL) &&
          list.count == 1 && fixture.count == 2 &&
          !memcmp(&state, &saved, sizeof(state)));
    fixture.fail_at = 0;
    list.count = fixture.count = 0;
    sprite.horizontal = AB_ANCHOR_HCENTER;
    sprite.vertical = AB_ANCHOR_VCENTER;
    CHECK(ab_ui_sprite_render(&sprite, &state, &boundaries, &list, NULL) &&
          commands[0].quad.vertices[0].x == -1 + (2.0f / 480) * 32 &&
          commands[0].quad.vertices[0].y == 1 + (-2.0f / 320) * 87);
    state.geometry.angle_radians = 1;
    list.count = fixture.count = 0;
    CHECK(!ab_ui_sprite_render(&sprite, &state, &boundaries, &list, NULL) &&
          list.count == 0 && fixture.count == 0);
    puts("UI composite cached order, anchors, natural sizing, scale and failure prefix "
         "passed");
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() && composite_checks() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = compare_native(file);
    return fclose(file) == 0 && okay ? 0 : 1;
}
