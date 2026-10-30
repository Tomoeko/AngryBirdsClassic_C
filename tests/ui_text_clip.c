#include "ab/ui_text_draw.h"
#include "ab/utf8.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "UI text clip failed at %d: %s\n", __LINE__, #condition);  \
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
    AbUiTextDraw root = {.position = {5, 7},
                         .scale = {2, 4},
                         .group = span("root"),
                         .visible = true,
                         .clipped = true,
                         .alpha = NAN,
                         .alpha_present = true,
                         .color_present = true,
                         .text = {NULL, 1},
                         .font = {NULL, 1},
                         .font_present = true};
    float arguments[] = {10, 20, 2, 3, 0, 0.5f};
    AbUiTextClipFrame frame;
    CHECK(ab_ui_text_clip_begin(&root, arguments, 6, &frame));
    AbBitmapFont font = {.max_ascending = 20, .max_descending = 5};
    AbUiTextDraw line = {.position = {8, 10},
                         .scale = {3, 2},
                         .group = {NULL, 1},
                         .text = span("AB"),
                         .visible = false};
    AbUiTextDrawPlan plan;
    CHECK(ab_ui_text_clip_line_plan(&frame, &line, &font, 320, 0.3f, &plan) &&
          plan.draw && plan.scale.x == 6 && plan.scale.y == 12 &&
          plan.layout.placement.x == 2.75f && plan.layout.placement.y == 2.375f &&
          plan.group.data == root.group.data);
    line.position.y = 1000;
    line.scale.x = NAN;
    line.clipped = line.color_present = true;
    CHECK(ab_ui_text_clip_line_plan(&frame, &line, &font, 320, NAN, &plan) &&
          !plan.draw);
    frame = (AbUiTextClipFrame){.visible = true,
                                .scale = {1, 1},
                                .factor = 1,
                                .parent_position = {-0.0f, -0.0f},
                                .position = {-0.0f, -0.0f}};
    line = (AbUiTextDraw){
        .scale = {1, 1}, .position = {-0.0f, -0.0f}, .vertical = AB_ANCHOR_BASELINE};
    CHECK(ab_ui_text_clip_line_plan(&frame, &line, &font, 320, 1, &plan) && plan.draw &&
          raw(plan.layout.placement.x) == UINT32_C(0x80000000) &&
          raw(plan.layout.placement.y) == UINT32_C(0x80000000));
    AbUiTextDrawPlan preserved = plan;
    CHECK(!ab_ui_text_clip_line_plan(&frame, &line, NULL, 320, 1, &plan) &&
          memcmp(&preserved, &plan, sizeof(plan)) == 0);
    root.visible = false;
    CHECK(ab_ui_text_clip_begin(&root, NULL, SIZE_MAX, &frame) && !frame.visible);
    CHECK(ab_ui_text_clip_line_plan(&frame, &line, NULL, 320, NAN, &plan) &&
          !plan.draw);
    CHECK(!ab_ui_text_clip_begin(NULL, arguments, 6, &frame));
    CHECK(!ab_ui_text_clip_line_plan(NULL, &line, &font, 320, 1, &plan));
    puts("UI text clip root-field skips, line visibility/group overrides, paired "
         "scales, pre-scale culling, signed zero and failure-output checks pass.");
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

typedef struct {
    size_t lines, drawn, hidden_drawn, glyphs;
} Counts;

static bool line_from_input(const uint32_t input[15], AbUiTextDraw *output) {
    const char *const texts[] = {"",     "AB",      "a0 B",
                                 "A\nB", "missing", "\xe4\xb8\x80\xf0\x9f\x98\x80",
                                 "  ",   "A0aB"};
    CHECK(input[0] < 2 && input[1] < 2 && input[2] < 2 && input[3] < 4 &&
          input[4] < 4 && input[5] < 5 && input[6] < 8 && input[7] < 3);
    *output =
        (AbUiTextDraw){.position = {number(input[8]), number(input[9])},
                       .scale = {number(input[10]), number(input[11])},
                       .rotation_pivot = {number(input[13]), number(input[14])},
                       .rotation_pivot_present = input[3],
                       .alpha = number(input[12]),
                       .alpha_present = input[2] != 0,
                       .visible = input[0] != 0,
                       .floor_coordinates = input[1] != 0,
                       .horizontal = (AbHorizontalAnchor)input[4],
                       .vertical = (AbVerticalAnchor)input[5],
                       .font = span(input[7] == 1 ? "FONT_BASIC" : "MISSING_FONT"),
                       .font_present = input[7] != 0,
                       .text = span(texts[input[6]]),
                       .group = span("child-group-ignored")};
    return true;
}

static bool append_line(const AbUiTextDrawPlan *plan, const AbBitmapFont *font,
                        uint32_t state[24], uint32_t expected[32][27], size_t *count) {
    state[4] = state[7] = raw(plan->rotation.cosine);
    state[5] = raw(plan->rotation.sine);
    state[6] = raw(-plan->rotation.sine);
    state[10] = raw(plan->scale.x);
    state[11] = raw(plan->scale.y);
    state[12] = raw(plan->layout.rotation_origin_x);
    state[13] = raw(plan->layout.rotation_origin_y);
    state[16] = raw(plan->draw_alpha);
    state[18] = raw(plan->angle);
    uint32_t codes[32];
    AbFontGlyphPlacement glyphs[32];
    size_t length, written = 0;
    CHECK(ab_utf8_to_codes(plan->text, codes, 32, &length) &&
          ab_font_layout_codes(font, codes, length, 0, length, plan->layout, glyphs, 32,
                               &written) &&
          written <= 32 - *count);
    for (size_t index = 0; index < written; ++index) {
        uint32_t *event = expected[(*count)++];
        event[0] = (uint32_t)(glyphs[index].glyph - font->glyphs);
        event[1] = raw(glyphs[index].x);
        event[2] = raw(glyphs[index].y);
        memcpy(event + 3, state, 24 * sizeof(*state));
        event[15] = raw(glyphs[index].rotation_origin_x);
        event[16] = raw(glyphs[index].rotation_origin_y);
    }
    state[16] = raw(plan->after_alpha);
    return true;
}

static bool same_words(const uint32_t *actual, const uint32_t *expected, size_t count,
                       size_t test) {
    for (size_t field = 0; field < count; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr,
                    "UI text clip case %zu field %zu: C %08" PRIx32 " native %08" PRIx32
                    "\n",
                    test, field, actual[field], expected[field]);
            return false;
        }
    }
    return true;
}

static bool compare_case(FILE *file, const AbBitmapFont *font, size_t test,
                         Counts *totals) {
    uint32_t input[46], line_count = 0, data[4][15];
    CHECK(words(file, input, 46) && input[0] < 7 && input[4] >= 2 && input[4] <= 6 &&
          words(file, &line_count, 1) && line_count <= 4);
    for (uint32_t line = 0; line < line_count; ++line) {
        CHECK(words(file, data[line], 15));
    }
    AbUiTextDraw root = {.position = {number(input[9]), number(input[10])},
                         .scale = {number(input[11]), number(input[12])},
                         .visible = input[0] >= 2,
                         .clipped = true};
    float arguments[6];
    for (size_t index = 0; index < 6; ++index) {
        arguments[index] = number(input[16 + index]);
    }
    AbUiTextClipFrame frame;
    CHECK(ab_ui_text_clip_begin(&root, arguments, input[4], &frame));
    uint32_t state[24], expected[32][27];
    memcpy(state, input + 22, sizeof(state));
    size_t count = 0, drawn = 0;
    for (uint32_t index = 0; index < line_count; ++index) {
        AbUiTextDraw line;
        AbUiTextDrawPlan plan;
        CHECK(line_from_input(data[index], &line) &&
              ab_ui_text_clip_line_plan(&frame, &line, font, 320, number(state[16]),
                                        &plan));
        if (plan.draw) {
            ++drawn;
            totals->hidden_drawn += !line.visible;
            CHECK(plan.group.size == 0 &&
                  append_line(&plan, font, state, expected, &count));
        }
    }
    uint32_t native_leaves, native_count = 0;
    CHECK(words(file, &native_leaves, 1) && words(file, &native_count, 1));
    if (native_leaves != drawn || native_count != count) {
        fprintf(stderr,
                "UI text clip case %zu: leaves C %zu native %" PRIu32
                ", glyphs C %zu native %" PRIu32 "\n",
                test, drawn, native_leaves, count, native_count);
        return false;
    }
    for (size_t index = 0; index < count; ++index) {
        uint32_t native[27];
        CHECK(words(file, native, 27) && same_words(expected[index], native, 27, test));
    }
    uint32_t native[24];
    CHECK(words(file, native, 24) && same_words(state, native, 24, test));
    totals->lines += line_count;
    totals->drawn += drawn;
    totals->glyphs += count;
    return true;
}

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_UI_TEXT_CLIP_V1 4096\n") == 0);
    const uint32_t codes[] = {32, 48, 65, 66, 97, 0x4e00, 0x1f600};
    AbFontGlyph glyphs[7];
    for (size_t index = 0; index < 7; ++index) {
        glyphs[index] = (AbFontGlyph){.code = codes[index],
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
    Counts totals = {0};
    for (size_t test = 0; test < 4096; ++test) {
        CHECK(compare_case(file, &font, test, &totals));
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB UI text clip reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 native clipped text calls match: %zu prepared lines, %zu native "
           "leaf draws, %zu hidden-line draws and %zu glyphs with all context words.\n",
           totals.lines, totals.drawn, totals.hidden_drawn, totals.glyphs);
    return true;
}

int main(int argc, char **argv) {
    if (!edge_checks()) {
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
