#include "ab/font_draw.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Font layout failed at line %d: %s\n", __LINE__,           \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static int32_t signed_word(uint32_t value) {
    return value <= INT32_MAX ? (int32_t)value
                              : (int32_t)((int64_t)value - INT64_C(4294967296));
}

static int16_t signed_half(uint32_t value) {
    value &= UINT32_C(0xffff);
    return (int16_t)(value < 32768 ? (int32_t)value : (int32_t)value - 65536);
}

static float number(uint32_t bits) {
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

static uint32_t raw(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool edge_checks(void) {
    AbFontGlyph glyph = {.code = 'A', .width = 5, .height = 7, .pivot_y = -3};
    AbBitmapFont font = {.glyphs = &glyph,
                         .count = 1,
                         .tracking = 3,
                         .leading = 30,
                         .max_ascending = 8,
                         .max_descending = 3};
    uint32_t text[] = {'?', 'A', 'A'};
    AbFontPlacement placement = {.placement = {.x = 20,
                                               .y = 10,
                                               .scale_x = 1,
                                               .scale_y = 1,
                                               .horizontal = AB_ANCHOR_HCENTER,
                                               .vertical = AB_ANCHOR_TOP},
                                 .rotation_origin_x = 100,
                                 .rotation_origin_y = 200};
    AbFontGlyphPlacement output[3];
    size_t count = 99;
    CHECK(ab_font_layout_codes(&font, text, 3, 0, 3, placement, output, 3, &count));
    CHECK(count == 2 && output[0].character == 1 && output[1].character == 2);
    CHECK(output[0].x == 7 && output[1].x == 15 && output[0].y == 18);
    CHECK(output[0].rotation_origin_x == 113 && output[0].rotation_origin_y == 190);
    AbDrawCommand commands[2];
    AbDrawList list = {.commands = commands, .capacity = 2};
    AbFontDrawStyle style = {.texture = 1,
                             .texture_width = 256,
                             .texture_height = 256,
                             .scale_x = 1,
                             .scale_y = 1,
                             .transform = {.xx = 1, .yy = 1},
                             .color = {1, 1, 1, 1},
                             .opacity = 1};
    CHECK(ab_font_draw_unrotated(output, count, style, &list, NULL));
    CHECK(list.count == 2 && commands[0].quad.vertices[0].x == 7 &&
          commands[0].quad.vertices[0].y == 20 && commands[1].quad.vertices[0].x == 15);
    list.count = 0;
    style.color[0] = number(UINT32_C(0x7fc00000));
    CHECK(!ab_font_draw_unrotated(output, count, style, &list, NULL));
    CHECK(list.count == 0);
    style.color[0] = 1;
    const AbFontGlyph *saved = output[1].glyph;
    output[1].glyph = NULL;
    CHECK(!ab_font_draw_unrotated(output, count, style, &list, NULL));
    CHECK(list.count == 0);
    output[1].glyph = saved;
    count = 99;
    CHECK(!ab_font_layout_codes(&font, text, 3, 0, 3, placement, output, 1, &count));
    CHECK(count == 99);
    CHECK(ab_font_layout_codes(&font, text, 3, 0, 1, placement, NULL, 0, &count));
    CHECK(count == 0);
    CHECK(!ab_font_layout_codes(&font, text, 3, 2, 2, placement, output, 3, &count));
    CHECK(!ab_font_layout_codes(&font, NULL, 3, 0, 0, placement, output, 3, &count));
    CHECK(!ab_font_layout_codes(&font, text, 3, 0, 0, placement, NULL, 1, &count));
    CHECK(!ab_font_layout_codes(&font, text, 3, 0, 0, placement, output, 3, NULL));
    placement.rotation_origin_x = number(UINT32_C(0x7f800000));
    CHECK(!ab_font_layout_codes(&font, text, 3, 0, 0, placement, output, 3, &count));
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, output + field) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool native_image_fields(const AbFontGlyphPlacement *entry,
                                const uint32_t *input, uint32_t glyph,
                                uint32_t actual[15]) {
    const AbFontGlyph *record = entry->glyph;
    AbSpriteRegion region = {
        .width = record->width, .height = record->height, .pivot_y = record->pivot_y};
    AbSpritePlacement placement = {.x = entry->x,
                                   .y = entry->y,
                                   .scale_x = number(input[10]),
                                   .scale_y = number(input[11]),
                                   .vertical = AB_ANCHOR_VPIVOT,
                                   .horizontal = AB_ANCHOR_HPIVOT};
    AbDrawQuad quad;
    CHECK(ab_sprite_quad(&region, 256, 256, placement, &quad, NULL));
    actual[0] = glyph;
    actual[1] = raw(quad.vertices[0].x);
    actual[2] = raw(quad.vertices[0].y);
    actual[3] = raw((float)record->width * placement.scale_x);
    actual[4] = raw((float)record->height * placement.scale_y);
    actual[5] = raw(entry->rotation_origin_x);
    actual[6] = raw(entry->rotation_origin_y);
    for (size_t vertex = 0; vertex < 4; ++vertex) {
        actual[7 + 2 * vertex] = raw(quad.vertices[vertex].u);
        actual[8 + 2 * vertex] = raw(quad.vertices[vertex].v);
    }
    return true;
}

static bool native_checks(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    bool image = strcmp(marker, "AB_ANDROID_FONT_IMAGE_V1 4096\n") == 0;
    CHECK(image || strcmp(marker, "AB_ANDROID_FONT_LAYOUT_V1 4096\n") == 0);
    static const uint32_t codes[] = {32, 48, 65, 66, 97, 0x4e00, 0x1f600};
    size_t total_glyphs = 0, empty_layouts = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[46], expected_count;
        CHECK(read_words(file, input, 46));
        CHECK(input[0] <= 7 && input[5] <= 16 && input[6] <= input[5] &&
              input[7] <= input[5] - input[6] && input[14] <= 4 && input[15] <= 3);
        AbFontGlyph glyphs[7];
        for (size_t glyph = 0; glyph < 7; ++glyph) {
            glyphs[glyph] =
                (AbFontGlyph){.code = codes[glyph],
                              .width = signed_half(input[16 + glyph]),
                              .height = signed_half(input[16 + glyph] >> 16),
                              .pivot_y = signed_half(input[23 + glyph])};
        }
        AbBitmapFont font = {.glyphs = glyphs,
                             .count = input[0],
                             .tracking = signed_half(input[1]),
                             .max_ascending = signed_word(input[2]),
                             .max_descending = signed_word(input[3]),
                             .leading = signed_half(input[4])};
        AbFontPlacement placement = {
            .placement = {.x = number(input[8]),
                          .y = number(input[9]),
                          .scale_x = number(input[10]),
                          .scale_y = number(input[11]),
                          .vertical = (AbVerticalAnchor)input[14],
                          .horizontal = (AbHorizontalAnchor)input[15]},
            .rotation_origin_x = number(input[12]),
            .rotation_origin_y = number(input[13])};
        AbFontGlyphPlacement output[16];
        size_t count = SIZE_MAX;
        CHECK(ab_font_layout_codes(&font, input + 30, input[5], input[6], input[7],
                                   placement, output, 16, &count));
        CHECK(read_words(file, &expected_count, 1) && expected_count == count);
        for (size_t glyph = 0; glyph < count; ++glyph) {
            uint32_t expected[15];
            size_t fields = image ? 15 : 5;
            CHECK(read_words(file, expected, fields));
            const AbFontGlyphPlacement *entry = output + glyph;
            uint32_t actual[15] = {(uint32_t)(entry->glyph - glyphs), raw(entry->x),
                                   raw(entry->y), raw(entry->rotation_origin_x),
                                   raw(entry->rotation_origin_y)};
            if (image) {
                CHECK(native_image_fields(entry, input,
                                          (uint32_t)(entry->glyph - glyphs), actual));
            }
            for (size_t field = 0; field < fields; ++field) {
                if (actual[field] != expected[field]) {
                    fprintf(stderr,
                            "Native font layout %zu glyph %zu field %zu: "
                            "%08" PRIx32 " != %08" PRIx32 "\n",
                            item, glyph, field, actual[field], expected[field]);
                    return false;
                }
            }
        }
        uint32_t restored[2];
        CHECK(read_words(file, restored, 2));
        CHECK(restored[0] == input[12] && restored[1] == input[13]);
        total_glyphs += count;
        empty_layouts += count == 0;
    }
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, image ? "AB font image reference complete\n"
                               : "AB font layout reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 native UTF-32 font %s match: %zu glyph callbacks, "
           "%zu empty layouts; context origins restored.\n",
           image ? "image boundaries" : "layouts", total_glyphs, empty_layouts);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_font_layout_tests [NATIVE_FONT_LAYOUT_TRACE]\n");
        return 2;
    }
    if (!edge_checks()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        perror(argv[1]);
        return 1;
    }
    bool okay = native_checks(file);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
