#include "ab/font_wrap.h"
#include "ab/ui_frame_geometry.h"
#include "ab/ui_text_lines.h"
#include "ab/ui_text_metrics.h"
#include "ab/utf8.h"

#include "../tools/oracle/ui_text_lines_cases.h"
#include "../tools/oracle/ui_text_wrap_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Text lines failed at %d: %s\n", __LINE__, #condition);    \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    uint32_t kind, value;
    AbSpan arguments[2];
} Event;

typedef struct {
    AbBitmapFont font;
    AbFontGlyph glyphs[7];
    AbUiFrameGeometry frames[4];
    AbSpan line_spans[128];
    uint8_t line_bytes[128][128];
    Event events[256];
    uint8_t event_bytes[256][2][128];
    size_t count, cumulative_queries, fail_at;
} Boundaries;

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

static AbUiPointerValue numeric(float value) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER, .number_bits = bits(value)};
}

static bool record(Boundaries *context, uint32_t kind, uint32_t value, AbSpan first,
                   AbSpan second) {
    CHECK(context->count < 256);
    Event *event = context->events + context->count;
    *event = (Event){.kind = kind, .value = value};
    AbSpan arguments[] = {first, second};
    for (size_t index = 0; index < 2; ++index) {
        CHECK(arguments[index].size <= 128 &&
              (!arguments[index].size || arguments[index].data));
        uint8_t *bytes = context->event_bytes[context->count][index];
        if (arguments[index].size) {
            memcpy(bytes, arguments[index].data, arguments[index].size);
        }
        event->arguments[index] = (AbSpan){bytes, arguments[index].size};
    }
    ++context->count;
    return !context->fail_at || context->count != context->fail_at;
}

static bool cumulative_scale(void *context, AbVec2 *output, AbError *error) {
    (void)error;
    Boundaries *data = context;
    ++data->cumulative_queries;
    return ab_ui_frame_cumulative_scale(data->frames, 4, output);
}

static bool set_font(void *context, AbUiPointerValue font, AbError *error) {
    (void)error;
    CHECK(font.kind == AB_UI_POINTER_TEXT);
    return record(context, 1, 0, font.text, (AbSpan){0});
}

static bool wrap(void *context, AbUiPointerValue group, AbUiPointerValue label,
                 float width, AbUiWrappedText *output, AbError *error) {
    Boundaries *data = context;
    CHECK(group.kind == AB_UI_POINTER_TEXT && label.kind == AB_UI_POINTER_TEXT);
    CHECK(record(data, 2, bits(width), group.text, label.text));
    uint32_t codes[128];
    size_t count = 0;
    AbFontWrappedLine lines[128];
    AbFontWrap wrapped = {.lines = lines, .capacity = 128};
    CHECK(ab_utf8_to_codes(label.text, codes, 128, &count) &&
          ab_font_wrap_codes(&data->font, codes, count, width, &wrapped, error));
    for (size_t index = 0; index < wrapped.count; ++index) {
        size_t length = 0;
        CHECK(ab_utf8_from_codes(codes + lines[index].start, lines[index].count,
                                 data->line_bytes[index], 128, &length));
        data->line_spans[index] = (AbSpan){data->line_bytes[index], length};
    }
    *output =
        (AbUiWrappedText){data->line_spans, wrapped.count, (float)wrapped.widest_line};
    return true;
}

static bool font_leading(void *context, float *output, AbError *error) {
    (void)error;
    Boundaries *data = context;
    *output = (float)data->font.leading;
    return record(data, 3, bits(*output), (AbSpan){0}, (AbSpan){0});
}

static bool default_name(void *context, AbUiPointerValue *output, AbError *error) {
    (void)error;
    *output = text(span("generated-name"));
    return record(context, 4, 0, output->text, (AbSpan){0});
}

static bool localize(void *context, AbUiPointerValue group, AbUiPointerValue label,
                     AbUiPointerValue *output, AbError *error) {
    (void)error;
    CHECK(group.kind == AB_UI_POINTER_TEXT && label.kind == AB_UI_POINTER_TEXT);
    *output = label;
    return record(context, 5, 0, group.text, label.text);
}

static bool measure(void *context, AbUiPointerValue label, AbUiPointerValue *output,
                    AbError *error) {
    (void)error;
    Boundaries *data = context;
    CHECK(label.kind == AB_UI_POINTER_TEXT);
    uint32_t codes[128];
    size_t count = 0;
    AbFontMeasure result;
    CHECK(ab_utf8_to_codes(label.text, codes, 128, &count) &&
          ab_font_measure_codes(&data->font, codes, count, 0, count, &result));
    *output = numeric((float)result.width);
    return record(data, 6, output->number_bits, label.text, (AbSpan){0});
}

static void prepare_font(Boundaries *context) {
    const uint32_t codes[] = {32, 48, 65, 66, 97, 0x4e00, 0x1f600};
    for (size_t index = 0; index < 7; ++index) {
        context->glyphs[index] = (AbFontGlyph){.code = codes[index],
                                               .width = (int16_t)(8 + index),
                                               .height = (int16_t)(12 + index)};
    }
    context->font = (AbBitmapFont){.glyphs = context->glyphs,
                                   .count = 7,
                                   .tracking = 1,
                                   .leading = 30,
                                   .max_ascending = 20,
                                   .max_descending = 5};
}

static AbUiTextLineBoundaries provider(Boundaries *context, float screen_width) {
    return (AbUiTextLineBoundaries){.init = {.context = context,
                                             .default_name = default_name,
                                             .set_font = set_font,
                                             .localize = localize,
                                             .measure = measure,
                                             .screen_width = numeric(screen_width),
                                             .basic_font = text(span("FONT_BASIC")),
                                             .font_setting_available = true},
                                    .cumulative_scale = cumulative_scale,
                                    .wrap = wrap,
                                    .font_leading = font_leading};
}

static bool edges(void) {
    Boundaries context = {0};
    prepare_font(&context);
    context.frames[0].scale = (AbVec2){2, 3};
    AbUiTextLineBoundaries boundaries = provider(&context, 480);
    AbUiTextState lines[8];
    AbUiTextLines state = {
        .lines = lines,
        .capacity = 8,
        .text = {.frame.fields = {.scale_x = numeric(2), .scale_y = numeric(3)},
                 .fields = {.text = text(span("A\nB")),
                            .group = text(span("OTHER")),
                            .box_size = numeric(64),
                            .font = text(span("FONT_BASIC")),
                            .horizontal_anchor = text(span("HCENTER")),
                            .vertical_anchor = text(span("VCENTER"))},
                 .leading_scale = numeric(0.5f)}};
    AbUiPointerValue own = {.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    CHECK(ab_ui_text_lines_build(&state, own, numeric(0), &boundaries, NULL) &&
          context.cumulative_queries == 1 && state.count == 2 && state.clipped &&
          state.text_block_height == 0 &&
          state.lines[0].frame.fields.y.number_bits == 0 &&
          state.lines[1].frame.fields.scale_y.number_bits == bits(1) &&
          state.lines[1].frame.fields.h.number_bits == 0 &&
          state.lines[1].frame.fields.visible.boolean && context.count == 11);
    context.count = context.cumulative_queries = 0;
    context.fail_at = 8;
    state.clipped = false;
    CHECK(!ab_ui_text_lines_build(&state, own, numeric(1), &boundaries, NULL) &&
          context.cumulative_queries == 1 && state.count == 1 && !state.clipped &&
          state.text_block_height == 90 && context.count == 8);
    context.count = context.fail_at = 0;
    state.capacity = 1;
    state.count = 7;
    CHECK(!ab_ui_text_lines_build(&state, own, numeric(1), &boundaries, NULL) &&
          state.count == 7 && context.count == 3);
    CHECK(!ab_ui_text_lines_build(NULL, own, numeric(1), &boundaries, NULL));
    puts("Text line composition, zero spacing, unconditional scale query, constructor "
         "defaults, capacity and failed-constructor prefix checks pass.");
    return true;
}

static bool metric_edges(void) {
    Boundaries context = {0};
    prepare_font(&context);
    AbUiTextState state = {
        .frame.fields = {.scale_x = numeric(-1),
                         .scale_y = {.kind = AB_UI_POINTER_BOOLEAN}},
        .fields = {.text = text(span("")),
                   .group = text(span("")),
                   .font = text(span("FONT_BASIC"))},
        .leading_scale = numeric(0.5f),
        .font_getter = {.kind = AB_UI_POINTER_FUNCTION, .function = 17}};
    AbUiTextMetricBoundaries boundaries = {.resources = provider(&context, 480).init,
                                           .leading = font_leading};
    float output = 123;
    CHECK(ab_ui_text_get_width(&state, &boundaries, &output, NULL) &&
          bits(output) == 0 && context.count == 1 && context.events[0].kind == 1);
    state.fields.text = text(span("a0"));
    context.count = 0;
    CHECK(ab_ui_text_get_width(&state, &boundaries, &output, NULL) && output == -22 &&
          context.count == 3 && context.events[1].kind == 5 &&
          !context.events[1].arguments[0].size);
    context.count = 0;
    CHECK(ab_ui_text_get_height(&state, (AbUiPointerValue){0}, SIZE_MAX, &boundaries,
                                &output, NULL) &&
          output == 15 && context.count == 2);
    context.count = 0;
    CHECK(ab_ui_text_get_height(&state, numeric(0), 0, &boundaries, &output, NULL) &&
          bits(output) == 0 && context.count == 2);
    context.count = 0;
    context.fail_at = 3;
    output = 123;
    CHECK(!ab_ui_text_get_width(&state, &boundaries, &output, NULL) && output == 123 &&
          context.count == 3);
    context.count = context.fail_at = 0;
    state.fields.text = (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT};
    CHECK(!ab_ui_text_get_width(&state, &boundaries, &output, NULL) && output == 123 &&
          context.count == 1);
    CHECK(!ab_ui_text_get_font_leading(NULL, &boundaries, &output, NULL));
    CHECK(!ab_ui_text_get_height(&state, numeric(0), UINT32_C(0x1000001), &boundaries,
                                 &output, NULL));
    puts("Text metric font order, empty-text positive zero, empty-group lookup, "
         "ignored stored getter, line counts/default scales and failure output checks "
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

static bool compare_bytes(FILE *file, AbSpan actual) {
    uint32_t size;
    CHECK(words(file, &size, 1) && size == actual.size);
    for (size_t index = 0; index < actual.size; ++index) {
        unsigned byte;
        CHECK(fscanf(file, "%2x", &byte) == 1 && byte == actual.data[index]);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool compare_line(FILE *file, const AbUiTextState *line) {
    const AbUiFrameFields *frame = &line->frame.fields;
    const AbUiTextArguments *text_fields = &line->fields;
    const AbUiPointerValue numeric_values[] = {
        frame->x, frame->y, frame->scale_x, frame->scale_y,        frame->angle,
        frame->w, frame->h, line->width,    text_fields->box_size, line->leading_scale};
    uint32_t expected[18], actual[18] = {0};
    CHECK(words(file, expected, 18));
    for (size_t index = 0; index < 10; ++index) {
        CHECK(numeric_values[index].kind == AB_UI_POINTER_NUMBER);
        actual[index] = numeric_values[index].number_bits;
    }
    CHECK(frame->visible.kind == AB_UI_POINTER_BOOLEAN &&
          frame->active.kind == AB_UI_POINTER_BOOLEAN &&
          line->floor_coordinates.kind == AB_UI_POINTER_BOOLEAN &&
          line->font_getter.kind == AB_UI_POINTER_NONE &&
          line->frame.child_count <= UINT32_MAX &&
          line->frame.lookup_count <= UINT32_MAX);
    actual[10] = frame->visible.boolean;
    actual[11] = frame->active.boolean;
    actual[12] = line->floor_coordinates.boolean;
    actual[15] = (uint32_t)line->frame.child_count;
    actual[16] = (uint32_t)line->frame.lookup_count;
    AbSpan name = span("generated-name");
    actual[17] = frame->name.kind == AB_UI_POINTER_TEXT &&
                 frame->name.text.size == name.size &&
                 memcmp(frame->name.text.data, name.data, name.size) == 0;
    CHECK(memcmp(expected, actual, sizeof(expected)) == 0);
    const AbUiPointerValue strings[] = {
        text_fields->font, text_fields->group, text_fields->horizontal_anchor,
        text_fields->vertical_anchor, text_fields->text};
    for (size_t index = 0; index < 5; ++index) {
        CHECK(strings[index].kind == AB_UI_POINTER_TEXT &&
              compare_bytes(file, strings[index].text));
    }
    return true;
}

static void prepare_state(Boundaries *context, const uint32_t input[19],
                          AbUiTextLines *state) {
    for (size_t index = 0; index < 4; ++index) {
        context->frames[index] = (AbUiFrameGeometry){
            .scale = {number(input[9 + index * 2]), number(input[10 + index * 2])},
            .parent = index < input[1] ? context->frames + index + 1 : NULL};
    }
    const UiTextWrapCase *source = ui_text_wrap_cases + input[0];
    state->text = (AbUiTextState){
        .frame.fields = {.scale_x = numeric(number(input[9])),
                         .scale_y = numeric(number(input[10]))},
        .fields = {.text = text((AbSpan){(const uint8_t *)source->bytes, source->size}),
                   .group = text(span(ui_text_lines_groups[input[18]])),
                   .font = text(span("FONT_BASIC")),
                   .box_size = numeric(number(input[5])),
                   .horizontal_anchor = text(span(ui_text_lines_horizontal[input[8]])),
                   .vertical_anchor = text(span(ui_text_lines_vertical[input[7]]))},
        .leading_scale = numeric(number(input[6]))};
}

static AbUiPointerValue own_scale(uint32_t kind) {
    if (!kind) {
        return (AbUiPointerValue){0};
    }
    if (kind < 3) {
        return (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = kind == 2};
    }
    if (kind < 5) {
        return numeric((float)(kind - 3));
    }
    return text(span(kind == 5 ? "" : "truthy"));
}

static bool compare_case(FILE *file, size_t *total_lines, size_t *total_events) {
    uint32_t input[19];
    CHECK(words(file, input, 19) && input[0] < 36 && input[1] < 4 && input[2] < 7 &&
          input[3] < 3 && input[7] < 5 && input[8] < 5 && input[18] < 3);
    Boundaries context = {0};
    prepare_font(&context);
    AbUiTextState lines[128];
    AbUiTextLines state = {.lines = lines, .capacity = 128};
    prepare_state(&context, input, &state);
    AbUiTextLineBoundaries boundaries = provider(&context, number(input[17]));
    AbUiPointerValue spacing = input[3] == 2 ? numeric(number(input[4]))
                               : input[3] == 1
                                   ? (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN}
                                   : (AbUiPointerValue){0};
    CHECK(ab_ui_text_lines_build(&state, own_scale(input[2]), spacing, &boundaries,
                                 NULL) &&
          context.cumulative_queries == 1);
    uint32_t expected[5];
    uint32_t actual[] = {bits(state.text_block_height), bits(state.widest_line),
                         (uint32_t)state.count, state.clipped, (uint32_t)context.count};
    CHECK(words(file, expected, 5) && memcmp(expected, actual, sizeof(expected)) == 0);
    for (size_t index = 0; index < state.count; ++index) {
        CHECK(compare_line(file, state.lines + index));
    }
    for (size_t index = 0; index < context.count; ++index) {
        const Event *event = context.events + index;
        uint32_t metadata[2];
        CHECK(words(file, metadata, 2) && metadata[0] == event->kind &&
              metadata[1] == event->value && compare_bytes(file, event->arguments[0]) &&
              compare_bytes(file, event->arguments[1]));
    }
    *total_lines += state.count;
    *total_events += context.count;
    return true;
}

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_UI_TEXT_LINES_V1 4096\n") == 0);
    size_t total_lines = 0, total_events = 0;
    for (size_t test = 0; test < 4096; ++test) {
        if (!compare_case(file, &total_lines, &total_events)) {
            fprintf(stderr, "Original Text.clip composition case %zu differs\n", test);
            return false;
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB UI text lines reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("4096 original Text.clip compositions, %zu constructed lines, every field/"
           "UTF8 byte and %zu ordered boundaries match exactly.\n",
           total_lines, total_events);
    return true;
}

static AbUiPointerValue metric_scale(uint32_t kind, uint32_t value) {
    if (!kind) {
        return (AbUiPointerValue){0};
    }
    return kind == 1 ? (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN}
                     : numeric(number(value));
}

static AbUiPointerValue metric_clipped(uint32_t kind) {
    if (kind < 4) {
        return own_scale(kind);
    }
    return kind == 4 ? text(span(""))
                     : (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT, .object = 1};
}

static bool compare_metric_case(FILE *file, size_t *total_events) {
    uint32_t input[12];
    CHECK(words(file, input, 12) && input[0] < 4 && input[2] < 3 && input[4] < 3 &&
          input[7] < 6 && input[8] < 9 && input[10] < 3);
    Boundaries context = {0};
    prepare_font(&context);
    int32_t leading, tracking;
    memcpy(&leading, input + 9, sizeof(leading));
    memcpy(&tracking, input + 11, sizeof(tracking));
    CHECK(leading >= -60 && leading <= 60 && tracking >= -4 && tracking <= 4);
    context.font.leading = (int16_t)leading;
    context.font.tracking = (int16_t)tracking;
    AbUiPointerValue label = {.kind = (AbUiPointerKind)input[0]};
    if (input[0] == 1) {
        CHECK(input[1] < 2);
        label.boolean = input[1] != 0;
    } else if (input[0] == 2) {
        label.number_bits = input[1];
    } else if (input[0] == 3) {
        CHECK(input[1] < 36);
        const UiTextWrapCase *source = ui_text_wrap_cases + input[1];
        label.text = (AbSpan){(const uint8_t *)source->bytes, source->size};
    }
    AbUiTextState state = {
        .frame.fields = {.scale_x = metric_scale(input[2], input[3]),
                         .scale_y = metric_scale(input[4], input[5])},
        .fields = {.text = label,
                   .group = text(span(ui_text_lines_groups[input[10]])),
                   .font = text(span("FONT_BASIC"))},
        .leading_scale = numeric(number(input[6])),
        .width = numeric(number(0x7fc12345)),
        .font_getter = {.kind = AB_UI_POINTER_FUNCTION, .function = 17}};
    AbUiTextMetricBoundaries boundaries = {.resources = provider(&context, 480).init,
                                           .leading = font_leading};
    for (size_t method = 0; method < 3; ++method) {
        context.count = 0;
        float result;
        bool okay =
            method == 0 ? ab_ui_text_get_width(&state, &boundaries, &result, NULL)
            : method == 1
                ? ab_ui_text_get_height(&state, metric_clipped(input[7]), input[8],
                                        &boundaries, &result, NULL)
                : ab_ui_text_get_font_leading(&state, &boundaries, &result, NULL);
        CHECK(okay);
        uint32_t expected[2];
        CHECK(words(file, expected, 2) && expected[0] == bits(result) &&
              expected[1] == context.count);
        for (size_t event_index = 0; event_index < context.count; ++event_index) {
            const Event *event = context.events + event_index;
            CHECK(words(file, expected, 2) && expected[0] == event->kind &&
                  expected[1] == event->value &&
                  compare_bytes(file, event->arguments[0]) &&
                  compare_bytes(file, event->arguments[1]));
        }
        *total_events += context.count;
    }
    return true;
}

static bool compare_metrics_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_UI_TEXT_METRICS_V1 4096 3\n") == 0);
    size_t total_events = 0;
    for (size_t test = 0; test < 4096; ++test) {
        if (!compare_metric_case(file, &total_events)) {
            fprintf(stderr, "Original Text metric case %zu differs\n", test);
            return false;
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB UI text metrics reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 12288 original Text width/height/leading results and %zu ordered "
           "font/UTF/localization/measurement boundaries match exactly.\n",
           total_events);
    return true;
}

int main(int argc, char **argv) {
    if (!edges() || !metric_edges()) {
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
    char header[80];
    bool okay =
        fgets(header, sizeof(header), file) != NULL && fseek(file, 0, SEEK_SET) == 0;
    if (okay) {
        okay = strcmp(header, "AB_ANDROID_UI_TEXT_METRICS_V1 4096 3\n") == 0
                   ? compare_metrics_native(file)
                   : compare_native(file);
    }
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
