#include "ab/ui_init.h"

#include "../tools/oracle/ui_init_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "UI initialization failed at %d: %s\n", __LINE__,          \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbUiPointerValue localized, measured;
    AbUiPointerValue font_results[3];
    uint32_t events[5][7];
    size_t count, fail_at;
} Boundaries;

static AbUiPointerValue text(size_t index) {
    return (AbUiPointerValue){
        .kind = AB_UI_POINTER_TEXT,
        .text = {(const uint8_t *)ui_init_texts[index], strlen(ui_init_texts[index])}};
}

static bool decode(const uint32_t input[2], AbUiPointerValue *output) {
    CHECK(input[0] <= AB_UI_POINTER_FUNCTION);
    *output = (AbUiPointerValue){.kind = (AbUiPointerKind)input[0]};
    switch (output->kind) {
        case AB_UI_POINTER_NONE:
            CHECK(input[1] == 0);
            break;
        case AB_UI_POINTER_BOOLEAN:
            CHECK(input[1] < 2);
            output->boolean = input[1] != 0;
            break;
        case AB_UI_POINTER_NUMBER:
            output->number_bits = input[1];
            break;
        case AB_UI_POINTER_TEXT:
            CHECK(input[1] < sizeof(ui_init_texts) / sizeof(*ui_init_texts));
            *output = text(input[1]);
            break;
        case AB_UI_POINTER_OBJECT:
            output->object = input[1];
            break;
        case AB_UI_POINTER_FUNCTION:
            CHECK(input[1] < 3);
            output->function = input[1];
            break;
    }
    return true;
}

static bool encode(AbUiPointerValue value, uint32_t output[2]) {
    CHECK(ab_ui_pointer_value_valid(&value));
    output[0] = (uint32_t)value.kind;
    output[1] = 0;
    if (value.kind == AB_UI_POINTER_BOOLEAN) {
        output[1] = value.boolean;
    } else if (value.kind == AB_UI_POINTER_NUMBER) {
        output[1] = value.number_bits;
    } else if (value.kind == AB_UI_POINTER_OBJECT ||
               value.kind == AB_UI_POINTER_FUNCTION) {
        CHECK(value.object <= UINT32_MAX);
        output[1] = (uint32_t)value.object;
    } else if (value.kind == AB_UI_POINTER_TEXT) {
        for (size_t index = 0; index < sizeof(ui_init_texts) / sizeof(*ui_init_texts);
             ++index) {
            AbSpan candidate = text(index).text;
            if (value.text.size == candidate.size &&
                (!candidate.size ||
                 memcmp(value.text.data, candidate.data, candidate.size) == 0)) {
                output[1] = (uint32_t)index;
                return true;
            }
        }
        CHECK(false);
    }
    return true;
}

static bool event(Boundaries *boundaries, uint32_t kind, AbUiPointerValue first,
                  AbUiPointerValue second, AbUiPointerValue result) {
    CHECK(boundaries->count < 5);
    uint32_t *output = boundaries->events[boundaries->count++];
    output[0] = kind;
    CHECK(encode(first, output + 1) && encode(second, output + 3) &&
          encode(result, output + 5));
    return !boundaries->fail_at || boundaries->count != boundaries->fail_at;
}

static bool default_name(void *context, AbUiPointerValue *output, AbError *error) {
    (void)error;
    *output = text(5);
    return event(context, 1, (AbUiPointerValue){0}, (AbUiPointerValue){0}, *output);
}

static bool set_font(void *context, AbUiPointerValue font, AbError *error) {
    (void)error;
    return event(context, 2, font, (AbUiPointerValue){0}, (AbUiPointerValue){0});
}

static bool localize(void *context, AbUiPointerValue group, AbUiPointerValue label,
                     AbUiPointerValue *output, AbError *error) {
    (void)error;
    Boundaries *boundaries = context;
    *output = boundaries->localized;
    return event(boundaries, 3, group, label, *output);
}

static bool measure(void *context, AbUiPointerValue label, AbUiPointerValue *output,
                    AbError *error) {
    (void)error;
    Boundaries *boundaries = context;
    *output = boundaries->measured;
    return event(boundaries, 4, label, (AbUiPointerValue){0}, *output);
}

static bool call_font(void *context, uintptr_t function, AbUiPointerValue *output,
                      AbError *error) {
    (void)error;
    Boundaries *boundaries = context;
    CHECK(function < 3);
    *output = boundaries->font_results[function];
    return event(
        boundaries, 5,
        (AbUiPointerValue){.kind = AB_UI_POINTER_FUNCTION, .function = function},
        (AbUiPointerValue){0}, *output);
}

static AbUiInitBoundaries provider(Boundaries *boundaries) {
    return (AbUiInitBoundaries){
        .context = boundaries,
        .default_name = default_name,
        .set_font = set_font,
        .localize = localize,
        .measure = measure,
        .call_font = call_font,
        .screen_width = {.kind = AB_UI_POINTER_NUMBER, .number_bits = 0x43f00000},
        .basic_font = text(6),
        .font_setting_available = true};
}

static bool edges(void) {
    Boundaries calls = {
        .localized = text(7),
        .measured = {.kind = AB_UI_POINTER_NUMBER, .number_bits = 0x42000000}};
    AbUiInitBoundaries boundaries = provider(&calls);
    AbUiTextState state = {.frame = {.child_count = 8, .lookup_count = 7}};
    AbUiFrameFields frame = {
        .scale_x = {.kind = AB_UI_POINTER_NUMBER, .number_bits = 0x40000000},
        .scale_y = {.kind = AB_UI_POINTER_NUMBER, .number_bits = 0x80000000},
        .visible = {.kind = AB_UI_POINTER_BOOLEAN},
        .active = {.kind = AB_UI_POINTER_BOOLEAN},
    };
    AbUiTextArguments arguments = {.text = text(1)};
    CHECK(ab_ui_text_init(&state, &frame, &arguments, &boundaries, NULL));
    CHECK(state.frame.child_count == 0 && state.frame.lookup_count == 0 &&
          state.frame.fields.scale_x.number_bits == 0x3f800000 &&
          state.frame.fields.scale_y.number_bits == 0x3f800000 &&
          state.frame.fields.visible.boolean && !state.frame.fields.active.boolean &&
          state.width.number_bits == 0x42000000 && calls.count == 4);
    state.fields.font = text(8);
    state.font_getter = (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT, .object = 17};
    arguments.font = text(6);
    arguments.group = text(0);
    calls.count = 0;
    CHECK(ab_ui_text_init(&state, &frame, &arguments, &boundaries, NULL) &&
          state.font_getter.object == 17 && calls.count == 3 &&
          calls.events[1][1] == AB_UI_POINTER_TEXT && calls.events[1][2] == 8 &&
          calls.events[2][0] == 4 && calls.events[2][2] == 1);
    calls.count = 0;
    calls.fail_at = 3;
    arguments.group = text(2);
    state.width = text(8);
    CHECK(!ab_ui_text_init(&state, &frame, &arguments, &boundaries, NULL) &&
          state.width.kind == AB_UI_POINTER_TEXT && calls.count == 3 &&
          state.frame.fields.visible.boolean);
    calls.count = calls.fail_at = 0;
    boundaries.font_setting_available = false;
    boundaries.set_font = NULL;
    CHECK(ab_ui_text_init(&state, NULL, NULL, &boundaries, NULL) &&
          state.width.kind == AB_UI_POINTER_NUMBER && state.width.number_bits == 0 &&
          calls.count == 1);
    CHECK(!ab_ui_frame_init(NULL, &frame, &boundaries, NULL));
    CHECK(!ab_ui_text_init(&state, NULL, NULL, NULL, NULL));
    calls.count = 0;
    arguments.font = (AbUiPointerValue){.kind = AB_UI_POINTER_FUNCTION, .function = 0};
    CHECK(ab_ui_text_init(&state, NULL, &arguments, &boundaries, NULL) &&
          state.fields.font.kind == AB_UI_POINTER_FUNCTION &&
          state.font_getter.kind == AB_UI_POINTER_FUNCTION && calls.count == 2);
    calls.count = 0;
    calls.font_results[0] = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    CHECK(ab_ui_text_layout(&state, &boundaries, NULL) &&
          state.fields.font.kind == AB_UI_POINTER_BOOLEAN &&
          !state.fields.font.boolean && calls.count == 1);
    calls.count = 0;
    CHECK(ab_ui_text_set_font(&state, text(6), &boundaries, NULL) &&
          state.font_getter.kind == AB_UI_POINTER_FUNCTION && !calls.count &&
          ab_ui_text_layout(&state, &boundaries, NULL) && calls.count == 1 &&
          state.fields.font.kind == AB_UI_POINTER_BOOLEAN);
    calls.count = 0;
    calls.fail_at = 1;
    state.fields.font = text(8);
    AbUiPointerValue getter = {.kind = AB_UI_POINTER_FUNCTION, .function = 1};
    CHECK(!ab_ui_text_set_font(&state, getter, &boundaries, NULL) &&
          state.font_getter.function == 1 &&
          state.fields.font.kind == AB_UI_POINTER_TEXT && calls.count == 1);
    calls.count = calls.fail_at = 0;
    state.font_getter = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    CHECK(!ab_ui_text_layout(&state, &boundaries, NULL) && !calls.count);
    puts("UI initialization defaults, Text overrides, font reuse, empty group, "
         "ordered boundaries and failure-prefix checks pass.");
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static AbUiFrameFields frame_arguments(const AbUiPointerValue values[10]) {
    return (AbUiFrameFields){.name = values[0],
                             .x = values[1],
                             .y = values[2],
                             .scale_x = values[3],
                             .scale_y = values[4],
                             .angle = values[5],
                             .w = values[6],
                             .h = values[7],
                             .visible = values[8],
                             .active = values[9]};
}

static AbUiTextArguments text_arguments(const AbUiPointerValue values[6]) {
    return (AbUiTextArguments){.text = values[0],
                               .group = values[1],
                               .box_size = values[2],
                               .horizontal_anchor = values[3],
                               .vertical_anchor = values[4],
                               .font = values[5]};
}

static bool compare_state(const AbUiTextState *state, FILE *file, size_t test) {
    const AbUiFrameFields *frame = &state->frame.fields;
    const AbUiTextArguments *text_fields = &state->fields;
    AbUiPointerValue values[] = {
        frame->name,
        frame->x,
        frame->y,
        frame->scale_x,
        frame->scale_y,
        frame->angle,
        frame->w,
        frame->h,
        frame->visible,
        frame->active,
        text_fields->text,
        text_fields->group,
        text_fields->box_size,
        text_fields->horizontal_anchor,
        text_fields->vertical_anchor,
        text_fields->font,
        state->width,
        state->floor_coordinates,
        state->leading_scale,
        state->font_getter,
    };
    uint32_t expected[40];
    CHECK(words(file, expected, 40));
    for (size_t index = 0; index < 20; ++index) {
        uint32_t actual[2];
        CHECK(encode(values[index], actual));
        if (memcmp(actual, expected + index * 2, sizeof(actual)) != 0) {
            fprintf(stderr, "UI constructor case %zu field %s differs\n", test,
                    ui_init_fields[index]);
            CHECK(false);
        }
    }
    return true;
}

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_UI_INIT_V1 4096\n") == 0);
    size_t total_calls = 0, text_calls = 0, kept_fonts = 0;
    for (size_t test = 0; test < 4096; ++test) {
        uint32_t setup[4], input[44];
        CHECK(words(file, setup, 4) && words(file, input, 44) && setup[0] < 2 &&
              setup[1] < 2 && setup[2] < 3 && setup[3] < 3);
        AbUiPointerValue values[22];
        for (size_t index = 0; index < 22; ++index) {
            CHECK(decode(input + index * 2, values + index));
        }
        AbUiFrameFields frame = frame_arguments(values);
        AbUiTextArguments arguments = text_arguments(values + 10);
        AbUiTextState state = {.frame = {.child_count = 17, .lookup_count = 17}};
        if (!setup[1]) {
            state.fields.font = values[16];
            state.font_getter = values[17];
            state.width =
                (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
            state.floor_coordinates = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
            state.leading_scale = (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER,
                                                     .number_bits = 0xbf800000};
        }
        Boundaries calls = {.localized = values[20], .measured = values[21]};
        AbUiInitBoundaries boundaries = provider(&calls);
        boundaries.screen_width = values[18];
        boundaries.basic_font = values[19];
        boundaries.font_setting_available = setup[3] == 2;
        if (setup[0]) {
            CHECK(ab_ui_text_init(&state, setup[2] == 2 ? &frame : NULL,
                                  setup[2] == 2 ? &arguments : NULL, &boundaries,
                                  NULL));
            ++text_calls;
            kept_fonts += !setup[1] && ab_ui_pointer_value_truthy(values + 16);
        } else {
            CHECK(ab_ui_frame_init(&state.frame, setup[2] == 2 ? &frame : NULL,
                                   &boundaries, NULL));
        }
        CHECK(compare_state(&state, file, test));
        uint32_t metadata[5];
        CHECK(words(file, metadata, 5) && metadata[0] == state.frame.child_count &&
              metadata[1] == state.frame.lookup_count && metadata[2] == 1 &&
              metadata[3] == 1 && metadata[4] == calls.count);
        for (size_t index = 0; index < calls.count; ++index) {
            uint32_t expected[7];
            CHECK(words(file, expected, 7) &&
                  memcmp(expected, calls.events[index], sizeof(expected)) == 0);
        }
        total_calls += calls.count;
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB UI initialization reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("4096 original Frame/Text constructors (%zu Text, %zu retained fonts), "
           "all typed fields and %zu ordered boundaries match exactly.\n",
           text_calls, kept_fonts, total_calls);
    return true;
}

static bool compare_font_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_UI_FONT_V1 64 64\n") == 0);
    size_t total_calls = 0, failed_layouts = 0, getter_calls = 0,
           function_fallbacks = 0;
    for (size_t instance = 0; instance < 64; ++instance) {
        uint32_t setup[4];
        CHECK(words(file, setup, 4));
        AbUiTextState state = {0};
        CHECK(decode(setup, &state.fields.font) &&
              decode(setup + 2, &state.font_getter));
        for (size_t step = 0; step < 64; ++step) {
            uint32_t command[10];
            CHECK(words(file, command, 10) && command[0] < 3 && command[1] < 3);
            AbUiPointerValue font;
            CHECK(decode(command + 2, &font));
            Boundaries calls = {.measured = {.kind = AB_UI_POINTER_NUMBER}};
            for (size_t token = 0; token < 3; ++token) {
                CHECK(decode(command + 4 + token * 2, calls.font_results + token));
            }
            AbUiInitBoundaries boundaries = provider(&calls);
            boundaries.font_setting_available = command[1] == 2;
            bool okay;
            if (command[0] == 0) {
                AbUiFrameFields frame = {.name = text(1)};
                AbUiTextArguments arguments = {
                    .font = font, .text = text(0), .group = text(0)};
                okay = ab_ui_text_init(&state, &frame, &arguments, &boundaries, NULL);
                function_fallbacks +=
                    font.kind == AB_UI_POINTER_FUNCTION &&
                    !ab_ui_pointer_value_truthy(calls.font_results + font.function);
            } else if (command[0] == 1) {
                okay = ab_ui_text_set_font(&state, font, &boundaries, NULL);
            } else {
                okay = ab_ui_text_layout(&state, &boundaries, NULL);
                failed_layouts += !okay;
            }
            uint32_t metadata[2];
            CHECK(words(file, metadata, 2) && metadata[0] == (uint32_t)!okay &&
                  metadata[1] == calls.count &&
                  compare_state(&state, file, instance * 64 + step));
            for (size_t index = 0; index < calls.count; ++index) {
                uint32_t expected[7];
                CHECK(words(file, expected, 7) &&
                      memcmp(expected, calls.events[index], sizeof(expected)) == 0);
                getter_calls += calls.events[index][0] == 5;
            }
            total_calls += calls.count;
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB UI font reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("4096 original Text init/setFont/layout commands, %zu getter calls, "
           "%zu constructor function fallbacks, %zu non-callable layout failures "
           "and %zu ordered boundaries match exactly.\n",
           getter_calls, function_fallbacks, failed_layouts, total_calls);
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
    char header[80];
    bool okay =
        fgets(header, sizeof(header), file) != NULL && fseek(file, 0, SEEK_SET) == 0;
    if (okay) {
        okay = strcmp(header, "AB_ANDROID_UI_FONT_V1 64 64\n") == 0
                   ? compare_font_native(file)
                   : compare_native(file);
    }
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
