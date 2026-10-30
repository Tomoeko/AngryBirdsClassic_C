#include "ab/font_draw.h"
#include "ab/ui_text_draw.h"
#include "ab/utf8.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "UI text drawing failed at %d: %s\n", __LINE__,            \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float number(uint32_t bits) {
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

static uint32_t raw(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static bool edge_checks(void) {
    AbUiTextDraw text = {.visible = true,
                         .position = {5, 7},
                         .scale = {2, 4},
                         .rotation_pivot = {3, 9},
                         .rotation_pivot_present = 1,
                         .alpha = 0.5f,
                         .alpha_present = true};
    float arguments[] = {0, 0, NAN, 4, 0, 1};
    AbUiTextDrawPlan output;
    CHECK(ab_ui_text_draw_plan(&text, arguments, 3, 0.25f, &output) && output.draw &&
          output.scale.x == 2 && output.scale.y == 4 &&
          output.layout.placement.x == 2.5f && output.layout.placement.y == 1.75f &&
          output.layout.rotation_origin_x == 0 &&
          output.layout.rotation_origin_y == 0 && output.draw_alpha == 0.5f &&
          output.after_alpha == 1 && output.alpha_override && output.font.size == 10);
    text.floor_coordinates = true;
    text.rotation_pivot_present = 3;
    text.alpha = 2;
    arguments[2] = -2;
    CHECK(ab_ui_text_draw_plan(&text, arguments, 4, 0.25f, &output) &&
          output.layout.placement.x == -2 && output.layout.placement.y == 0 &&
          output.layout.rotation_origin_x == 3 &&
          output.layout.rotation_origin_y == 9 && output.draw_alpha == 0.25f &&
          output.after_alpha == 0.25f && !output.alpha_override);
    AbUiTextDrawPlan preserved = output;
    CHECK(!ab_ui_text_draw_plan(&text, arguments, 1, 1, &output) &&
          memcmp(&preserved, &output, sizeof(output)) == 0);
    arguments[5] = 0;
    CHECK(!ab_ui_text_draw_plan(&text, arguments, 6, 1, &output));
    text.clipped = true;
    CHECK(!ab_ui_text_draw_plan(&text, arguments, 2, 1, &output));
    text.clipped = false;
    text.color_present = true;
    CHECK(!ab_ui_text_draw_plan(&text, arguments, 2, 1, &output));
    text.visible = false;
    text.position.x = NAN;
    CHECK(ab_ui_text_draw_plan(&text, NULL, SIZE_MAX, NAN, &output) && !output.draw);
    CHECK(!ab_ui_text_draw_plan(NULL, NULL, 0, 1, &output));
    CHECK(!ab_ui_text_draw_plan(&text, NULL, 0, 1, NULL));
    puts("UI text arity/default, ignored third argument, divide/floor order, paired "
         "pivots, alpha restore, hidden gate and unsupported-path checks pass.");
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool native_draw_checks(void) {
    AbFontGlyph glyph = {.width = 10, .height = 12, .pivot_y = -2};
    AbFontGlyphPlacement placements[2] = {{.glyph = &glyph,
                                           .x = 20,
                                           .y = 30,
                                           .rotation_origin_x = 1,
                                           .rotation_origin_y = 2},
                                          {0}};
    AbFontNativeDrawStyle style = {
        .texture = 1,
        .texture_width = 256,
        .texture_height = 256,
        .viewport_width = 640,
        .viewport_height = 480,
        .glyph_scale_x = 1,
        .glyph_scale_y = 1,
        .state = {.scale_x = 1, .scale_y = 1, .direct_vertices = true},
        .color = {1, 1, 1, 1},
        .opacity = 1};
    AbDrawCommand commands[2];
    AbDrawList list = {.commands = commands, .capacity = 2};
    CHECK(!ab_font_draw_native(placements, 2, style, &list, NULL) && !list.count);
    placements[1] = placements[0];
    list.capacity = 1;
    CHECK(!ab_font_draw_native(placements, 2, style, &list, NULL) && !list.count);
    list.capacity = 2;
    CHECK(ab_font_draw_native(placements, 2, style, &list, NULL) && list.count == 2 &&
          list.commands[0].coordinates == AB_DRAW_CLIP &&
          list.commands[0].quad.vertices[0].x == 20 &&
          list.commands[0].quad.vertices[0].y == 31);
    style.color[0] = NAN;
    CHECK(!ab_font_draw_native(placements, 1, style, &list, NULL) && list.count == 2);
    style.color[0] = 1;
    style.texture = 0;
    CHECK(!ab_font_draw_native(placements, 1, style, &list, NULL) && list.count == 2);
    puts("Native font quad pivot, direct coordinates, capacity/partial rollback "
         "and invalid resource/color checks pass.");
    return true;
}

static void draw_state(const AbUiTextDrawPlan *plan, uint32_t state[24]) {
    state[4] = raw(plan->rotation.cosine);
    state[5] = raw(plan->rotation.sine);
    state[6] = raw(-plan->rotation.sine);
    state[7] = raw(plan->rotation.cosine);
    state[10] = raw(plan->scale.x);
    state[11] = raw(plan->scale.y);
    state[12] = raw(plan->layout.rotation_origin_x);
    state[13] = raw(plan->layout.rotation_origin_y);
    state[16] = raw(plan->draw_alpha);
    state[18] = raw(plan->angle);
}

static bool same_words(const uint32_t *actual, const uint32_t *expected, size_t count,
                       size_t test) {
    for (size_t field = 0; field < count; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr,
                    "UI text case %zu field %zu: C %08" PRIx32 " native %08" PRIx32
                    "\n",
                    test, field, actual[field], expected[field]);
            return false;
        }
    }
    return true;
}

typedef struct {
    size_t glyphs, visible, alpha_changes, quads, culled, direct, rotated;
} TraceCounts;

static AbFontNativeDrawStyle native_style(const AbUiTextDrawPlan *plan,
                                          const uint32_t state[24]) {
    return (AbFontNativeDrawStyle){.texture = 1,
                                   .texture_width = 256,
                                   .texture_height = 256,
                                   .viewport_width = 640,
                                   .viewport_height = 480,
                                   .glyph_scale_x = 1,
                                   .glyph_scale_y = 1,
                                   .state = {.angle_radians = plan->angle,
                                             .translation_x = number(state[8]),
                                             .translation_y = number(state[9]),
                                             .scale_x = plan->scale.x,
                                             .scale_y = plan->scale.y,
                                             .direct_vertices = state[19] != 0},
                                   .color = {1, 1, 1, 1},
                                   .opacity = plan->draw_alpha};
}

static bool compare_image(FILE *file, const AbFontGlyphPlacement *glyph,
                          uint32_t glyph_id, const uint32_t state[24],
                          const AbDrawList *list, size_t *emitted, size_t test,
                          TraceCounts *totals) {
    uint32_t native[68], actual[37];
    CHECK(words(file, native, 68) && native[0] < 7 && native[37] < 2);
    const AbFontGlyph *region = glyph->glyph;
    AbSpriteRegion sprite = {.x = region->x,
                             .y = region->y,
                             .width = region->width,
                             .height = region->height,
                             .pivot_y = region->pivot_y};
    AbDrawQuad local;
    AbSpritePlacement placement = {glyph->x, glyph->y,         1,
                                   1,        AB_ANCHOR_VPIVOT, AB_ANCHOR_HPIVOT};
    CHECK(ab_sprite_quad(&sprite, 256, 256, placement, &local, NULL));
    actual[0] = glyph_id;
    actual[1] = raw(local.vertices[0].x);
    actual[2] = raw(local.vertices[0].y);
    actual[3] = raw((float)region->width);
    actual[4] = raw((float)region->height);
    memcpy(actual + 5, state, 24 * sizeof(*state));
    actual[17] = raw(glyph->rotation_origin_x);
    actual[18] = raw(glyph->rotation_origin_y);
    for (size_t vertex = 0; vertex < 4; ++vertex) {
        actual[29 + vertex * 2] = raw(local.vertices[vertex].u);
        actual[30 + vertex * 2] = raw(local.vertices[vertex].v);
    }
    CHECK(same_words(actual, native, 37, test));
    if (!native[37]) {
        ++totals->culled;
        return true;
    }
    CHECK(*emitted < list->count);
    const AbDrawCommand *command = list->commands + (*emitted)++;
    CHECK(command->coordinates == AB_DRAW_CLIP && command->texture == 1 &&
          raw(command->opacity) == state[16]);
    const size_t indices[] = {0, 1, 2, 2, 1, 3};
    for (size_t vertex = 0; vertex < 6; ++vertex) {
        const AbDrawVertex *point = command->quad.vertices + indices[vertex];
        uint32_t position[] = {raw(point->x), raw(point->y),
                               state[19] ? 0 : UINT32_C(0x3a83126f)};
        uint32_t uv[] = {raw(point->u), raw(point->v)};
        CHECK(same_words(position, native + 38 + vertex * 3, 3, test) &&
              same_words(uv, native + 56 + vertex * 2, 2, test));
    }
    ++totals->quads;
    totals->direct += state[19] != 0;
    totals->rotated += !state[19] && number(state[18]) != 0;
    return true;
}

static bool compare_case(FILE *file, const AbBitmapFont *font, size_t test,
                         bool image_mode, TraceCounts *totals) {
    const char *const texts[] = {"",     "AB",      "a0 B",
                                 "A\nB", "missing", "\xe4\xb8\x80\xf0\x9f\x98\x80",
                                 "  ",   "A0aB"};
    uint32_t input[46], native_count = 0;
    CHECK(words(file, input, 46) && input[0] < 7 && input[1] < 2 && input[2] < 2 &&
          input[3] < 4 && input[4] >= 2 && input[4] <= 6 && input[5] < 4 &&
          input[6] < 5 && input[7] < 8 && input[8] < 3);
    AbUiTextDraw text = {.position = {number(input[9]), number(input[10])},
                         .scale = {number(input[11]), number(input[12])},
                         .rotation_pivot = {number(input[14]), number(input[15])},
                         .vertical = (AbVerticalAnchor)input[6],
                         .horizontal = (AbHorizontalAnchor)input[5],
                         .text = span(texts[input[7]]),
                         .font = span(input[8] == 1 ? "FONT_BASIC" : "MISSING_FONT"),
                         .font_present = input[8] != 0,
                         .visible = input[0] >= 2,
                         .floor_coordinates = input[1] != 0,
                         .alpha_present = input[2] != 0,
                         .alpha = number(input[13]),
                         .rotation_pivot_present = input[3]};
    float arguments[6];
    for (size_t field = 0; field < 6; ++field) {
        arguments[field] = number(input[16 + field]);
    }
    AbUiTextDrawPlan plan;
    CHECK(ab_ui_text_draw_plan(&text, arguments, input[4], number(input[38]), &plan) &&
          words(file, &native_count, 1) && native_count <= 32);
    uint32_t state[24];
    memcpy(state, input + 22, sizeof(state));
    AbFontGlyphPlacement glyphs[32];
    size_t count = 0;
    if (plan.draw) {
        ++totals->visible;
        totals->alpha_changes += plan.alpha_override;
        draw_state(&plan, state);
        uint32_t codes[32];
        size_t code_count;
        CHECK(ab_utf8_to_codes(plan.text, codes, 32, &code_count) &&
              ab_font_layout_codes(font, codes, code_count, 0, code_count, plan.layout,
                                   glyphs, 32, &count));
    }
    CHECK(count == native_count);
    AbDrawCommand commands[32];
    AbDrawList list = {.commands = commands, .capacity = 32};
    size_t emitted = 0;
    if (image_mode && plan.draw) {
        CHECK(ab_font_draw_native(glyphs, count, native_style(&plan, state), &list,
                                  NULL));
    }
    for (size_t glyph = 0; glyph < count; ++glyph) {
        uint32_t glyph_id = (uint32_t)(glyphs[glyph].glyph - font->glyphs);
        if (image_mode) {
            CHECK(compare_image(file, glyphs + glyph, glyph_id, state, &list, &emitted,
                                test, totals));
            continue;
        }
        uint32_t native[27], actual[27];
        CHECK(words(file, native, 27));
        actual[0] = glyph_id;
        actual[1] = raw(glyphs[glyph].x);
        actual[2] = raw(glyphs[glyph].y);
        memcpy(actual + 3, state, sizeof(state));
        actual[15] = raw(glyphs[glyph].rotation_origin_x);
        actual[16] = raw(glyphs[glyph].rotation_origin_y);
        CHECK(same_words(actual, native, 27, test));
    }
    CHECK(emitted == list.count);
    if (plan.draw) {
        state[16] = raw(plan.after_alpha);
    }
    uint32_t native[24];
    CHECK(words(file, native, 24) && same_words(state, native, 24, test));
    totals->glyphs += count;
    return true;
}

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file));
    bool image_mode = strcmp(header, "AB_ANDROID_UI_TEXT_IMAGE_V1 4096\n") == 0;
    CHECK(image_mode || strcmp(header, "AB_ANDROID_UI_TEXT_V1 4096\n") == 0);
    const uint32_t codes[] = {32, 48, 65, 66, 97, 0x4e00, 0x1f600};
    AbFontGlyph glyphs[7];
    for (size_t index = 0; index < 7; ++index) {
        glyphs[index] = (AbFontGlyph){.code = codes[index],
                                      .x = image_mode ? (int16_t)(index * 10) : 0,
                                      .y = image_mode ? (int16_t)(index * 3) : 0,
                                      .width = (int16_t)(8 + index),
                                      .height = (int16_t)(12 + index),
                                      .pivot_y = (int16_t)((int)index - 2)};
    }
    AbBitmapFont font = {.glyphs = glyphs,
                         .count = 7,
                         .tracking = 1,
                         .leading = 30,
                         .max_ascending = 20,
                         .max_descending = 5};
    TraceCounts totals = {0};
    for (size_t test = 0; test < 4096; ++test) {
        CHECK(compare_case(file, &font, test, image_mode, &totals));
    }
    const char *completion = image_mode ? "AB UI text image reference complete\n"
                                        : "AB UI text reference complete\n";
    CHECK(fgets(header, sizeof(header), file) && strcmp(header, completion) == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 original native UI text calls match: %zu visible calls, "
           "%zu alpha overrides, %zu ordered glyphs and every context-state word.\n",
           totals.visible, totals.alpha_changes, totals.glyphs);
    if (image_mode) {
        printf("Native image batch geometry matches: %zu quads, %zu culls, "
               "%zu rotated, %zu direct; all vertices, depth and UV words.\n",
               totals.quads, totals.culled, totals.rotated, totals.direct);
    }
    return true;
}

int main(int argc, char **argv) {
    if (!edge_checks() || !native_draw_checks()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = compare_native(file);
    fclose(file);
    return okay ? 0 : 1;
}
