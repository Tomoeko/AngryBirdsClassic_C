#include "ab/ui_image_state.h"

#include "../tools/oracle/ui_image_state_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Image state failed at %d: %s\n", __LINE__, #condition);   \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbUiImageButtonState state;
    AbUiFrameNode node;
    AbUiFrameLifecycle lifecycle;
    uint32_t mapping[5], geometry[5][4], events[8][80];
    size_t count, fail_at;
} Context;

static AbSpan span(const char *bytes) {
    return (AbSpan){(const uint8_t *)bytes, strlen(bytes)};
}

static AbUiPointerValue text(size_t index) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                              .text = span(ui_image_state_texts[index])};
}

static float number(uint32_t bits) {
    float output;
    memcpy(&output, &bits, sizeof(output));
    return output;
}

static AbUiPointerValue numeric(uint32_t bits) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER, .number_bits = bits};
}

static void fields(AbUiImageButtonState *state, AbUiPointerValue *output[35]) {
    AbUiImageState *image = &state->image;
    AbUiFrameFields *frame = &image->frame.fields;
    AbUiPointerValue *values[] = {&frame->name,
                                  &frame->x,
                                  &frame->y,
                                  &frame->scale_x,
                                  &frame->scale_y,
                                  &frame->angle,
                                  &frame->w,
                                  &frame->h,
                                  &frame->visible,
                                  &frame->active,
                                  &image->image,
                                  &image->alpha,
                                  &image->floor_coordinates,
                                  &image->resource_pivot_x,
                                  &image->resource_pivot_y,
                                  &image->rotation_pivot_x,
                                  &image->rotation_pivot_y,
                                  &image->pivot_x,
                                  &image->pivot_y,
                                  &state->enabled,
                                  &state->hover_enabled,
                                  &state->hover_scale,
                                  &state->hover_state,
                                  &state->hover_count,
                                  &state->scale_on_touch,
                                  &state->padding,
                                  &state->click_sound,
                                  &state->activate_on_release,
                                  &state->return_value,
                                  &state->disabled_return_value,
                                  &state->enabled_image,
                                  &state->disabled_image,
                                  &state->meta,
                                  &state->click_started,
                                  &state->clear_pointer_flags};
    memcpy(output, values, sizeof(values));
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
            CHECK(input[1] < 9);
            *output = text(input[1]);
            break;
        case AB_UI_POINTER_OBJECT:
            output->object = input[1];
            break;
        case AB_UI_POINTER_FUNCTION:
            CHECK(input[1] == 0);
            output->function = 0;
            break;
    }
    return true;
}

static uint32_t text_id(AbSpan value) {
    for (uint32_t index = 0; index < 9; ++index) {
        AbSpan candidate = span(ui_image_state_texts[index]);
        if (candidate.size == value.size &&
            (!candidate.size || memcmp(candidate.data, value.data, value.size) == 0)) {
            return index;
        }
    }
    return UINT32_MAX;
}

static bool encode(AbUiPointerValue value, uint32_t output[2]) {
    CHECK(ab_ui_pointer_value_valid(&value));
    output[0] = (uint32_t)value.kind;
    output[1] = 0;
    if (value.kind == AB_UI_POINTER_BOOLEAN) {
        output[1] = value.boolean;
    } else if (value.kind == AB_UI_POINTER_NUMBER) {
        output[1] = value.number_bits;
    } else if (value.kind == AB_UI_POINTER_TEXT) {
        output[1] = text_id(value.text);
        CHECK(output[1] < 9);
    } else if (value.kind == AB_UI_POINTER_OBJECT) {
        CHECK(value.object <= UINT32_MAX);
        output[1] = (uint32_t)value.object;
    } else if (value.kind == AB_UI_POINTER_FUNCTION) {
        CHECK(value.function == 0);
    }
    return true;
}

static bool capture(Context *context, uint32_t output[76]) {
    AbUiPointerValue *values[35];
    fields(&context->state, values);
    for (size_t index = 0; index < 35; ++index) {
        CHECK(encode(*values[index], output + index * 2));
    }
    CHECK(context->state.image.frame.child_count <= UINT32_MAX &&
          context->state.image.frame.lookup_count <= UINT32_MAX);
    output[70] = (uint32_t)context->state.image.frame.child_count;
    output[71] = (uint32_t)context->state.image.frame.lookup_count;
    CHECK(encode(context->node.entered, output + 72) &&
          encode(context->lifecycle.entry_counts, output + 74));
    return true;
}

static bool record(Context *context, uint32_t kind, uint32_t id, uint32_t first,
                   uint32_t second) {
    CHECK(context->count < 8);
    uint32_t *event = context->events[context->count++];
    event[0] = kind;
    event[1] = id;
    event[2] = first;
    event[3] = second;
    CHECK(capture(context, event + 4));
    return !context->fail_at || context->count != context->fail_at;
}

static bool default_name(void *context, AbUiPointerValue *output, AbError *error) {
    (void)error;
    *output = text(6);
    return record(context, 0, 0, 6, 0);
}

static bool localize(void *context, AbSpan requested, AbSpan *output, AbError *error) {
    (void)error;
    Context *data = context;
    uint32_t id = text_id(requested);
    CHECK(id < 5 && data->mapping[id] < 5);
    *output = span(ui_image_state_texts[data->mapping[id]]);
    return record(data, 1, id, data->mapping[id], 0);
}

static bool geometry(Context *data, AbSpan requested, AbVec2 *output, uint32_t kind) {
    uint32_t id = text_id(requested);
    CHECK(id < 5 && kind >= 2 && kind <= 3);
    const uint32_t *values = data->geometry[id] + (kind - 2) * 2;
    *output = (AbVec2){number(values[0]), number(values[1])};
    return record(data, kind, id, values[0], values[1]);
}

static bool bounds(void *context, AbSpan requested, AbVec2 *output, AbError *error) {
    (void)error;
    return geometry(context, requested, output, 2);
}

static bool pivot(void *context, AbSpan requested, AbVec2 *output, AbError *error) {
    (void)error;
    return geometry(context, requested, output, 3);
}

static AbUiImageInitBoundaries provider(Context *context, uint32_t padding_id) {
    return (AbUiImageInitBoundaries){
        .frame = {.context = context, .default_name = default_name},
        .resources = {.context = context,
                      .localize = localize,
                      .bounds = bounds,
                      .pivot = pivot},
        .empty_padding = {.kind = AB_UI_POINTER_OBJECT, .object = padding_id}};
}

static void prepare(Context *context) {
    memset(context, 0, sizeof(*context));
    context->node.state = &context->state.image.frame;
    for (uint32_t index = 0; index < 5; ++index) {
        context->mapping[index] = index;
        context->geometry[index][0] = 0x42000000;
        context->geometry[index][1] = 0x41800000;
        context->geometry[index][2] = 0x40800000;
        context->geometry[index][3] = 0x40a00000;
    }
}

static bool edges(void) {
    Context context;
    prepare(&context);
    context.state.enabled =
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    context.state.click_started =
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    AbUiImageButtonArguments arguments = {
        .image = {.frame = {.name = text(5), .angle = numeric(0x3f800000)},
                  .image = text(1)},
        .scale_on_touch = {.kind = AB_UI_POINTER_BOOLEAN},
        .activate_on_release = {.kind = AB_UI_POINTER_BOOLEAN}};
    AbUiImageInitBoundaries boundaries = provider(&context, 100);
    CHECK(
        ab_ui_image_button_state_init(&context.state, &arguments, &boundaries, NULL) &&
        context.count == 6 && context.state.enabled.boolean &&
        context.state.image.frame.fields.angle.number_bits == 0 &&
        context.state.image.frame.fields.w.number_bits == 0x42000000 &&
        context.state.image.rotation_pivot_y.number_bits == 0x40a00000 &&
        !context.state.scale_on_touch.boolean &&
        !context.state.activate_on_release.boolean &&
        context.state.enabled_image.kind == AB_UI_POINTER_NONE &&
        context.state.image.image.kind == AB_UI_POINTER_TEXT &&
        context.state.padding.object == 100 && context.state.click_started.boolean);
    context.state.hover_count = numeric(0x40000000);
    context.count = 0;
    CHECK(ab_ui_image_button_state_enter(&context.state, &context.node,
                                         &context.lifecycle, NULL) &&
          !context.state.click_started.boolean &&
          context.state.hover_count.number_bits == 0x40000000 &&
          context.node.entered.boolean &&
          context.lifecycle.entry_counts.number_bits == 0x3f800000);
    context.state.click_started.boolean = true;
    CHECK(!ab_ui_image_button_state_enter(&context.state, &context.node,
                                          &context.lifecycle, NULL) &&
          !context.state.click_started.boolean && context.node.entered.boolean);
    context.count = 0;
    context.fail_at = 3;
    context.state.image.resource_pivot_x = numeric(0x41200000);
    CHECK(!ab_ui_image_state_set_image(&context.state.image, text(2),
                                       &boundaries.resources, NULL) &&
          context.state.image.frame.fields.w.number_bits == 0x42000000 &&
          context.state.image.resource_pivot_x.number_bits == 0x41200000);
    context.count = context.fail_at = 0;
    CHECK(ab_ui_image_state_set_image(&context.state.image, (AbUiPointerValue){0},
                                      &boundaries.resources, NULL) &&
          !context.count &&
          context.state.image.frame.fields.w.number_bits == 0x42000000);
    puts("Image/button constructor dispatch, repeated refresh, overrides, false "
         "defaults, "
         "prepared padding, retained flags, entry reset and resource failure-prefix "
         "checks pass.");
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static AbUiImageButtonArguments arguments(const AbUiPointerValue input[20]) {
    return (AbUiImageButtonArguments){.image = {.frame = {.name = input[0],
                                                          .x = input[1],
                                                          .y = input[2],
                                                          .scale_x = input[3],
                                                          .scale_y = input[4],
                                                          .angle = input[5],
                                                          .w = input[6],
                                                          .h = input[7],
                                                          .visible = input[8],
                                                          .active = input[9]},
                                                .image = input[10]},
                                      .scale_on_touch = input[11],
                                      .padding = input[12],
                                      .click_sound = input[13],
                                      .activate_on_release = input[14],
                                      .return_value = input[15],
                                      .disabled_return_value = input[16],
                                      .enabled_image = input[17],
                                      .disabled_image = input[18],
                                      .meta = input[19]};
}

static bool compare_words(const uint32_t *actual, const uint32_t *expected,
                          size_t count, size_t test) {
    for (size_t word = 0; word < count; ++word) {
        if (actual[word] != expected[word]) {
            fprintf(stderr,
                    "Image constructor case %zu word %zu differs: %08" PRIx32
                    " != %08" PRIx32 "\n",
                    test, word, actual[word], expected[word]);
            if (word < 70) {
                fprintf(stderr, "Field %s\n", ui_image_state_fields[word / 2]);
            }
            return false;
        }
    }
    return true;
}

typedef struct {
    size_t callbacks, entry_attempts, entry_failures, default_padding;
    size_t callback_kinds[4];
} Counts;

static bool compare_case(FILE *file, size_t test, Counts *counts) {
    uint32_t setup[8], old[70], input[40];
    Context context;
    prepare(&context);
    CHECK(words(file, setup, 8) && words(file, old, 70) && words(file, input, 40) &&
          setup[0] < 4 && setup[1] < 3 && setup[2] < 2 &&
          words(file, context.mapping, 5));
    for (size_t image = 0; image < 5; ++image) {
        CHECK(context.mapping[image] < 5 && words(file, context.geometry[image], 4));
    }
    AbUiPointerValue old_values[35], input_values[20], *state_fields[35];
    fields(&context.state, state_fields);
    bool button = (setup[0] & 1) != 0, fresh = (setup[0] & 2) != 0;
    for (size_t field = 0; field < 35; ++field) {
        CHECK(decode(old + field * 2, old_values + field));
        if (!fresh) {
            *state_fields[field] = old_values[field];
        }
    }
    for (size_t field = 0; field < 20; ++field) {
        if (!decode(input + field * 2, input_values + field)) {
            fprintf(stderr, "Invalid image argument %s\n",
                    ui_image_state_arguments[field]);
            return false;
        }
    }
    if (!fresh) {
        context.state.image.frame.child_count = context.state.image.frame.lookup_count =
            1;
        CHECK(decode(setup + 3, &context.node.entered));
    }
    CHECK(decode(setup + 5, &context.lifecycle.entry_counts));
    AbUiImageButtonArguments supplied = arguments(input_values);
    AbUiImageInitBoundaries boundaries = provider(&context, setup[7]);
    CHECK(button ? ab_ui_image_button_state_init(&context.state,
                                                 setup[1] == 2 ? &supplied : NULL,
                                                 &boundaries, NULL)
                 : ab_ui_image_state_init(&context.state.image,
                                          setup[1] == 2 ? &supplied.image : NULL,
                                          &boundaries, NULL));
    uint32_t metadata[2], expected[76], actual[76];
    bool defaulted =
        button && (setup[1] != 2 || !ab_ui_pointer_value_truthy(&supplied.padding));
    CHECK(words(file, metadata, 2) && metadata[0] == (uint32_t)defaulted &&
          metadata[1] == context.count && words(file, expected, 76) &&
          capture(&context, actual) && compare_words(actual, expected, 76, test));
    for (size_t event = 0; event < context.count; ++event) {
        uint32_t native_event[80];
        CHECK(words(file, native_event, 80) &&
              compare_words(context.events[event], native_event, 80, test));
        CHECK(native_event[0] < 4);
        ++counts->callback_kinds[native_event[0]];
    }
    counts->callbacks += context.count;
    counts->default_padding += defaulted;
    if (setup[2]) {
        CHECK(button);
        static const size_t restored[] = {21, 22, 23, 33, 34};
        for (size_t field = 0; field < 5; ++field) {
            *state_fields[restored[field]] = old_values[restored[field]];
        }
        CHECK(decode(setup + 3, &context.node.entered) &&
              decode(setup + 5, &context.lifecycle.entry_counts));
        context.count = 0;
        bool okay = ab_ui_image_button_state_enter(&context.state, &context.node,
                                                   &context.lifecycle, NULL);
        uint32_t status;
        CHECK(words(file, &status, 1) && status == (uint32_t)!okay &&
              words(file, expected, 76) && capture(&context, actual) &&
              compare_words(actual, expected, 76, test) && !context.count);
        ++counts->entry_attempts;
        counts->entry_failures += !okay;
    }
    return true;
}

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_UI_IMAGE_STATE_V1 4096\n") == 0);
    Counts counts = {0};
    for (size_t test = 0; test < 4096; ++test) {
        CHECK(compare_case(file, test, &counts));
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB UI image state reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("4096 original full-class Image/ImageButton constructors, all 35 fields and "
           "%zu callback snapshots match; %zu default paddings and %zu entry attempts/"
           "%zu inherited-entry failures match.\n",
           counts.callbacks, counts.default_padding, counts.entry_attempts,
           counts.entry_failures);
    printf("Default-name/localization/bounds/pivot snapshots: %zu/%zu/%zu/%zu.\n",
           counts.callback_kinds[0], counts.callback_kinds[1], counts.callback_kinds[2],
           counts.callback_kinds[3]);
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
