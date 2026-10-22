#include "ab/font_metrics.h"
#include "ab/ui_text_bounds.h"
#include "ab/utf8.h"

#include "../tools/oracle/ui_text_lines_cases.h"
#include "../tools/oracle/ui_text_wrap_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Text bounds failed at %d: %s\n", __LINE__, #condition);   \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    uint32_t input[24], events[8][8];
    AbBitmapFont font;
    AbFontGlyph glyphs[7];
    AbUiTextState *state;
    size_t count, fail_at;
} Boundaries;

typedef struct {
    Boundaries *boundaries;
    size_t index;
} Child;

typedef struct {
    size_t events, child_calls, increased, leaf_hits, clipped_hits, absent;
    size_t corner_cases, corner_hits;
} Counts;

static AbSpan span(const char *bytes) {
    return (AbSpan){(const uint8_t *)bytes, strlen(bytes)};
}

static AbUiPointerValue text(AbSpan bytes) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = bytes};
}

static uint32_t bits(float value) {
    uint32_t output;
    memcpy(&output, &value, sizeof(output));
    return output;
}

static float number(uint32_t value) {
    float output;
    memcpy(&output, &value, sizeof(output));
    return output;
}

static AbUiPointerValue numeric(uint32_t value) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER, .number_bits = value};
}

static AbSpan source(const Boundaries *context) {
    const UiTextWrapCase *value = ui_text_wrap_cases + context->input[0];
    return (AbSpan){(const uint8_t *)value->bytes, value->size};
}

static bool same_span(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || memcmp(first.data, second.data, first.size) == 0);
}

static bool record(Boundaries *context, const uint32_t event[8]) {
    CHECK(context->count < 8);
    memcpy(context->events[context->count++], event, sizeof(context->events[0]));
    return !context->fail_at || context->count != context->fail_at;
}

static bool localize(void *context, AbUiPointerValue group, AbUiPointerValue label,
                     AbUiPointerValue *output, AbError *error) {
    (void)error;
    Boundaries *data = context;
    CHECK(group.kind == AB_UI_POINTER_TEXT && label.kind == AB_UI_POINTER_TEXT &&
          same_span(group.text, span(ui_text_lines_groups[data->input[1]])) &&
          same_span(label.text, source(data)));
    *output = label;
    uint32_t event[8] = {1, data->input[1], data->input[0]};
    return record(data, event);
}

static bool measure(void *context, AbUiPointerValue label, AbUiPointerValue *output,
                    AbError *error) {
    (void)error;
    Boundaries *data = context;
    CHECK(label.kind == AB_UI_POINTER_TEXT && same_span(label.text, source(data)));
    uint32_t codes[128];
    size_t count = 0;
    AbFontMeasure result;
    CHECK(ab_utf8_to_codes(label.text, codes, 128, &count) &&
          ab_font_measure_codes(&data->font, codes, count, 0, count, &result));
    *output = numeric(bits((float)result.width));
    uint32_t event[8] = {2, data->input[0], output->number_bits};
    return record(data, event);
}

static bool leading(void *context, float *output, AbError *error) {
    (void)error;
    Boundaries *data = context;
    *output = (float)data->font.leading;
    int32_t next;
    memcpy(&next, data->input + 12, sizeof(next));
    CHECK(next >= -60 && next <= 60);
    data->font.leading = (int16_t)next;
    uint32_t event[8] = {3, bits(*output)};
    return record(data, event);
}

static AbUiPointerValue reply(uint32_t kind, uint32_t payload) {
    AbUiPointerValue result = {.kind = (AbUiPointerKind)kind};
    if (kind == AB_UI_POINTER_BOOLEAN) {
        result.boolean = payload != 0;
    } else if (kind == AB_UI_POINTER_NUMBER) {
        result.number_bits = payload;
    } else if (kind == AB_UI_POINTER_TEXT) {
        result.text = span(payload ? "truthy" : "");
    } else if (kind == AB_UI_POINTER_OBJECT) {
        result.object = payload;
    } else if (kind == AB_UI_POINTER_FUNCTION) {
        result.function = payload;
    }
    return result;
}

static bool child_query(void *context, AbVec2 point, AbUiPointerValue *output,
                        AbError *error) {
    (void)error;
    Child *child = context;
    Boundaries *data = child->boundaries;
    CHECK(child->index < data->input[3] && child->index < 5);
    uint32_t kind = data->input[14 + child->index * 2];
    uint32_t payload = data->input[15 + child->index * 2];
    uint32_t event[8] = {4,
                         (uint32_t)child->index,
                         bits(point.x),
                         bits(point.y),
                         data->state->width.number_bits,
                         kind,
                         payload};
    *output = reply(kind, payload);
    return record(data, event);
}

static void prepare(Boundaries *context, AbUiTextState *state) {
    const uint32_t codes[] = {32, 48, 65, 66, 97, 0x4e00, 0x1f600};
    for (size_t index = 0; index < 7; ++index) {
        context->glyphs[index] = (AbFontGlyph){.code = codes[index],
                                               .width = (int16_t)(8 + index),
                                               .height = (int16_t)(12 + index)};
    }
    int32_t font_leading, tracking;
    memcpy(&font_leading, context->input + 11, sizeof(font_leading));
    memcpy(&tracking, context->input + 13, sizeof(tracking));
    context->font = (AbBitmapFont){.glyphs = context->glyphs,
                                   .count = 7,
                                   .leading = (int16_t)font_leading,
                                   .tracking = (int16_t)tracking};
    *state = (AbUiTextState){
        .frame.fields = {.x = numeric(context->input[7]),
                         .y = numeric(context->input[8]),
                         .scale_x = numeric(0x7fc12345),
                         .scale_y = numeric(0x7fc12345),
                         .visible = {.kind = AB_UI_POINTER_BOOLEAN}},
        .fields = {.text = text(source(context)),
                   .group = text(span(ui_text_lines_groups[context->input[1]])),
                   .horizontal_anchor =
                       text(span(ui_text_lines_horizontal[context->input[4]])),
                   .vertical_anchor =
                       text(span(ui_text_lines_vertical[context->input[5]]))},
        .width = numeric(context->input[6]),
        .leading_scale = numeric(0x7fc12345),
        .font_getter = {.kind = AB_UI_POINTER_FUNCTION, .function = 17}};
    context->state = state;
}

static AbUiTextMetricBoundaries provider(Boundaries *context) {
    return (AbUiTextMetricBoundaries){
        .resources = {.context = context, .localize = localize, .measure = measure},
        .leading = leading};
}

static bool edges(void) {
    Boundaries context = {.input = {2, 1, 0, 2, 1, 1, 0}};
    context.input[7] = bits(10);
    context.input[8] = bits(20);
    context.input[11] = 30;
    context.input[12] = 40;
    context.input[13] = 1;
    context.input[14] = 1;
    context.input[16] = 2;
    AbUiTextState state;
    prepare(&context, &state);
    AbUiTextMetricBoundaries boundaries = provider(&context);
    AbUiPointerValue output;
    CHECK(ab_ui_text_check_bounds(&state, (AbUiPointerValue){0}, NULL, SIZE_MAX,
                                  (AbVec2){-1, 0}, &boundaries, &output, NULL) &&
          output.kind == AB_UI_POINTER_BOOLEAN && output.boolean &&
          state.width.number_bits == bits(22) && context.count == 4);
    context.count = 0;
    context.font.leading = 30;
    CHECK(ab_ui_text_check_bounds(&state, (AbUiPointerValue){0}, NULL, 0,
                                  (AbVec2){21, 30}, &boundaries, &output, NULL) &&
          output.boolean);
    Child child_contexts[] = {{&context, 0}, {&context, 1}};
    AbUiTextBoundsChild children[] = {{child_contexts, child_query},
                                      {child_contexts + 1, child_query}};
    context.count = 0;
    CHECK(ab_ui_text_check_bounds(&state, numeric(0), children, 2, (AbVec2){3, 4},
                                  &boundaries, &output, NULL) &&
          output.boolean && context.count == 4 && context.events[2][1] == 0 &&
          context.events[3][1] == 1 && context.events[3][2] == bits(3) &&
          context.events[3][3] == bits(4));
    context.input[16] = 0;
    context.count = 0;
    CHECK(ab_ui_text_check_bounds(&state, numeric(0), children, 2, (AbVec2){3, 4},
                                  &boundaries, &output, NULL) &&
          output.kind == AB_UI_POINTER_NONE);
    state.width = numeric(0);
    context.count = 0;
    context.font.leading = 30;
    context.fail_at = 4;
    output = numeric(0x42f60000);
    CHECK(!ab_ui_text_check_bounds(&state, (AbUiPointerValue){0}, NULL, 0, (AbVec2){0},
                                   &boundaries, &output, NULL) &&
          output.number_bits == 0x42f60000 && state.width.number_bits == bits(22));
    puts("Text bounds inclusive corners, width growth, distinct current leading reads, "
         "forward unchanged child points, nil exhaustion and failed-prefix checks "
         "pass.");
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool compare_events(FILE *file, const Boundaries *context, Counts *counts) {
    for (size_t index = 0; index < context->count; ++index) {
        uint32_t expected[8];
        CHECK(words(file, expected, 8) &&
              memcmp(expected, context->events[index], sizeof(expected)) == 0);
        counts->child_calls += expected[0] == 4;
    }
    counts->events += context->count;
    return true;
}

static bool compare_case(FILE *file, Counts *counts, size_t test) {
    Boundaries context = {0};
    CHECK(words(file, context.input, 24));
    const uint32_t *input = context.input;
    CHECK(input[0] < 36 && input[1] < 3 && input[2] < 7 && input[3] < 6 &&
          input[4] < 5 && input[5] < 5);
    int32_t first, second, tracking;
    memcpy(&first, input + 11, sizeof(first));
    memcpy(&second, input + 12, sizeof(second));
    memcpy(&tracking, input + 13, sizeof(tracking));
    CHECK(first >= -60 && first <= 60 && second >= -60 && second <= 60 &&
          tracking >= -4 && tracking <= 4);
    for (size_t index = 0; index < 5; ++index) {
        CHECK(input[14 + index * 2] < 6);
    }
    AbUiTextState state;
    prepare(&context, &state);
    AbUiTextMetricBoundaries boundaries = provider(&context);
    AbVec2 correction;
    CHECK(ab_ui_text_correction(&state, &boundaries, &correction, NULL));
    uint32_t expected_correction[3];
    CHECK(words(file, expected_correction, 3) &&
          expected_correction[0] == bits(correction.x) &&
          expected_correction[1] == bits(correction.y) &&
          expected_correction[2] == context.count &&
          compare_events(file, &context, counts));
    Child child_contexts[5];
    AbUiTextBoundsChild children[5];
    for (size_t index = 0; index < 5; ++index) {
        child_contexts[index] = (Child){&context, index};
        children[index] = (AbUiTextBoundsChild){child_contexts + index, child_query};
    }
    context.count = 0;
    context.font.leading = (int16_t)first;
    AbUiPointerValue clipped =
                         reply(input[2] < 3 ? (uint32_t)(input[2] != 0) : input[2] - 1,
                               input[2] == 2),
                     output;
    CHECK(ab_ui_text_check_bounds(&state, clipped, children, input[3],
                                  (AbVec2){number(input[9]), number(input[10])},
                                  &boundaries, &output, NULL));
    uint32_t actual[] = {(uint32_t)output.kind,
                         output.kind == AB_UI_POINTER_BOOLEAN ? output.boolean : 0,
                         state.width.number_bits, (uint32_t)context.count};
    uint32_t expected[4];
    CHECK(words(file, expected, 4) && memcmp(actual, expected, sizeof(actual)) == 0 &&
          compare_events(file, &context, counts));
    counts->increased += state.width.number_bits != input[6];
    counts->absent += output.kind == AB_UI_POINTER_NONE;
    counts->clipped_hits += ab_ui_pointer_value_truthy(&clipped) &&
                            output.kind == AB_UI_POINTER_BOOLEAN && output.boolean;
    counts->leaf_hits += !ab_ui_pointer_value_truthy(&clipped) && output.boolean;
    counts->corner_cases += !ab_ui_pointer_value_truthy(&clipped) && test % 9 < 4;
    counts->corner_hits +=
        !ab_ui_pointer_value_truthy(&clipped) && test % 9 < 4 && output.boolean;
    return true;
}

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_UI_TEXT_BOUNDS_V2 4096\n") == 0);
    Counts counts = {0};
    for (size_t test = 0; test < 4096; ++test) {
        if (!compare_case(file, &counts, test)) {
            fprintf(stderr, "Original Text bounds case %zu differs\n", test);
            return false;
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB UI text bounds reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf(
        "4096 original corrections/bounds match: %zu width increases, %zu leaf hits, "
        "%zu clipped hits/%zu nil exits, %zu child calls and %zu ordered boundaries; "
        "%zu/%zu exact leaf corners hit.\n",
        counts.increased, counts.leaf_hits, counts.clipped_hits, counts.absent,
        counts.child_calls, counts.events, counts.corner_hits, counts.corner_cases);
    return true;
}

int main(int argc, char **argv) {
    if (!edges()) {
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
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
