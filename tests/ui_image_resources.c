#include "ab/ui_image_resources.h"
#include "ab/ui_image_state.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "UI image resources failed at %d: %s\n", __LINE__,         \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static const char *const texts[] = {"", "raw", "localized", "disabled", "alias"};

typedef struct {
    AbUiImageButtonResources *state;
    AbUiImageButtonState *typed;
    uint32_t localization[5], geometry[5][4], calls[6][14], fail_kind;
    size_t count;
} Boundaries;

static uint32_t bits(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

static float number(uint32_t word) {
    float value;
    memcpy(&value, &word, sizeof(value));
    return value;
}

static AbSpan text(uint32_t id) {
    return (AbSpan){(const uint8_t *)texts[id], strlen(texts[id])};
}

static uint32_t text_id(AbSpan value) {
    for (uint32_t id = 0; id < 5; ++id) {
        AbSpan candidate = text(id);
        if (candidate.size == value.size &&
            (!candidate.size ||
             memcmp(candidate.data, value.data, candidate.size) == 0)) {
            return id;
        }
    }
    return UINT32_MAX;
}

static AbUiPointerValue value(uint32_t kind, uint32_t payload) {
    AbUiPointerValue result = {.kind = (AbUiPointerKind)kind};
    if (kind == AB_UI_POINTER_BOOLEAN) {
        result.boolean = payload != 0;
    } else if (kind == AB_UI_POINTER_NUMBER) {
        result.number_bits = payload;
    } else if (kind == AB_UI_POINTER_TEXT) {
        result.text = text(payload);
    } else if (kind == AB_UI_POINTER_OBJECT) {
        result.object = payload;
    }
    return result;
}

static uint32_t payload(AbUiPointerValue value) {
    switch (value.kind) {
        case AB_UI_POINTER_NONE:
            return 0;
        case AB_UI_POINTER_BOOLEAN:
            return value.boolean;
        case AB_UI_POINTER_NUMBER:
            return value.number_bits;
        case AB_UI_POINTER_TEXT:
            return text_id(value.text);
        case AB_UI_POINTER_OBJECT:
            return (uint32_t)value.object;
        case AB_UI_POINTER_FUNCTION:
            return (uint32_t)value.function;
    }
    return UINT32_MAX;
}

static void typed_numeric(AbUiImageButtonState *state, AbUiPointerValue *output[9]) {
    AbUiImageState *image = &state->image;
    AbUiPointerValue *values[] = {
        &image->frame.fields.w,   &image->frame.fields.h,   &image->resource_pivot_x,
        &image->resource_pivot_y, &image->rotation_pivot_x, &image->rotation_pivot_y,
        &image->pivot_x,          &image->pivot_y,          &image->frame.fields.angle};
    memcpy(output, values, sizeof(values));
}

static bool boundary(Boundaries *context, uint32_t kind, uint32_t id, uint32_t first,
                     uint32_t second) {
    CHECK(context->count < 6 && id < 5);
    uint32_t *call = context->calls[context->count++];
    call[0] = kind;
    call[1] = id;
    call[2] = first;
    call[3] = second;
    if (context->typed) {
        AbUiImageButtonState *state = context->typed;
        call[4] = (uint32_t)state->image.image.kind;
        call[5] = payload(state->image.image);
        AbUiPointerValue *numeric[9];
        typed_numeric(state, numeric);
        for (size_t field = 0; field < 6; ++field) {
            CHECK(numeric[field]->kind == AB_UI_POINTER_NUMBER);
            call[6 + field] = numeric[field]->number_bits;
        }
        call[12] = (uint32_t)state->enabled.kind;
        call[13] = payload(state->enabled);
    } else {
        const AbUiImageButtonResources *state = context->state;
        const AbUiImageResources *image = &state->image;
        call[4] = image->draw.image_present ? AB_UI_POINTER_TEXT : AB_UI_POINTER_NONE;
        call[5] = image->draw.image_present ? text_id(image->draw.image) : 0;
        float numeric[] = {image->draw.size.x,           image->draw.size.y,
                           image->sprite_pivot.x,        image->sprite_pivot.y,
                           image->draw.rotation_pivot.x, image->draw.rotation_pivot.y};
        for (size_t field = 0; field < 6; ++field) {
            call[6 + field] = bits(numeric[field]);
        }
        call[12] = (uint32_t)state->enabled.kind;
        call[13] = payload(state->enabled);
    }
    return kind != context->fail_kind;
}

static bool localize(void *context, AbSpan requested, AbSpan *output, AbError *error) {
    (void)error;
    Boundaries *input = context;
    uint32_t id = text_id(requested);
    CHECK(id < 5 && input->localization[id] < 5);
    *output = text(input->localization[id]);
    return boundary(input, 0, id, input->localization[id], 0);
}

static bool geometry(void *context, AbSpan requested, AbVec2 *output, uint32_t kind) {
    Boundaries *input = context;
    uint32_t id = text_id(requested);
    CHECK(id < 5);
    const uint32_t *words = input->geometry[id] + (kind - 1) * 2;
    *output = (AbVec2){number(words[0]), number(words[1])};
    return boundary(input, kind, id, words[0], words[1]);
}

static bool bounds(void *context, AbSpan requested, AbVec2 *output, AbError *error) {
    (void)error;
    return geometry(context, requested, output, 1);
}

static bool pivot(void *context, AbSpan requested, AbVec2 *output, AbError *error) {
    (void)error;
    return geometry(context, requested, output, 2);
}

static bool edge_checks(void) {
    AbUiImageButtonResources state = {
        .enabled = {.kind = AB_UI_POINTER_BOOLEAN, .boolean = true}};
    state.image.draw.pivot = (AbVec2){7, 8};
    state.image.draw.pivot_present = 3;
    Boundaries calls = {
        .state = &state, .localization = {0, 2, 0, 0, 0}, .fail_kind = UINT32_MAX};
    calls.geometry[2][0] = bits(40);
    calls.geometry[2][1] = bits(50);
    calls.geometry[2][2] = bits(12);
    calls.geometry[2][3] = bits(13);
    AbUiImageResourceBoundaries resources = {&calls, localize, bounds, pivot};
    CHECK(ab_ui_image_button_resource_set(&state, value(3, 1), value(0, 0), &resources,
                                          NULL) &&
          calls.count == 6 && state.image.draw.image_present &&
          text_id(state.image.draw.image) == 2 && state.image.draw.size.x == 40 &&
          state.image.draw.rotation_pivot.x == 12 && state.image.draw.pivot.x == 7 &&
          state.image.draw.pivot_present == 15);
    calls.count = 0;
    CHECK(ab_ui_image_button_resource_enable(&state, value(1, 0), &resources, NULL) &&
          !calls.count && state.image.draw.image_present);
    CHECK(ab_ui_image_resource_set(&state.image, value(0, 0), &resources, NULL) &&
          !calls.count && !state.image.draw.image_present &&
          state.image.draw.size.x == 40 && state.image.sprite_pivot.x == 12);
    CHECK(ab_ui_image_resource_size(&state.image, (AbVec2){0, NAN}, 1) &&
          state.image.draw.size.x == 0 && state.image.draw.size.y == 50 &&
          ab_ui_image_resource_angle(&state.image, 1000) &&
          state.image.draw.frame.angle == 1000);
    calls.count = 0;
    calls.fail_kind = 2;
    state.image.sprite_pivot = state.image.draw.rotation_pivot = (AbVec2){9, 9};
    CHECK(!ab_ui_image_resource_set(&state.image, value(3, 1), &resources, NULL) &&
          calls.count == 3 && state.image.draw.image_present &&
          state.image.draw.size.x == 40 && state.image.sprite_pivot.x == 9 &&
          state.image.draw.rotation_pivot.y == 9);
    CHECK(!ab_ui_image_resource_set(&state.image, value(1, 0), &resources, NULL) &&
          !ab_ui_image_resource_size(&state.image, (AbVec2){0}, 4) &&
          !ab_ui_image_resource_angle(&state.image, INFINITY));
    puts("UI image localization fallback, repeated button refresh, absent-disabled "
         "retention, image clear, partial size, raw angle and failed-prefix checks "
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

static bool valid_value(uint32_t kind, uint32_t payload) {
    return kind <= 4 && (kind != 1 || payload <= 1) && (kind != 3 || payload < 5);
}

static void state_words(const AbUiImageButtonResources *state, uint32_t output[17]) {
    const AbUiImageResources *image = &state->image;
    AbUiPointerValue current =
        image->draw.image_present
            ? (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = image->draw.image}
            : (AbUiPointerValue){0};
    AbUiPointerValue values[] = {current, state->enabled, state->enabled_image,
                                 state->disabled_image};
    for (size_t field = 0; field < 4; ++field) {
        output[field * 2] = (uint32_t)values[field].kind;
        output[1 + field * 2] = payload(values[field]);
    }
    float numeric[] = {image->draw.size.x,           image->draw.size.y,
                       image->sprite_pivot.x,        image->sprite_pivot.y,
                       image->draw.rotation_pivot.x, image->draw.rotation_pivot.y,
                       image->draw.pivot.x,          image->draw.pivot.y,
                       image->draw.frame.angle};
    for (size_t field = 0; field < 9; ++field) {
        output[8 + field] = bits(numeric[field]);
    }
}

static bool typed_state_words(AbUiImageButtonState *state, uint32_t output[17]) {
    AbUiPointerValue values[] = {state->image.image, state->enabled,
                                 state->enabled_image, state->disabled_image};
    for (size_t field = 0; field < 4; ++field) {
        output[field * 2] = (uint32_t)values[field].kind;
        output[field * 2 + 1] = payload(values[field]);
    }
    AbUiPointerValue *numeric[9];
    typed_numeric(state, numeric);
    for (size_t field = 0; field < 9; ++field) {
        CHECK(numeric[field]->kind == AB_UI_POINTER_NUMBER);
        output[8 + field] = numeric[field]->number_bits;
    }
    return true;
}

static bool typed_command(AbUiImageButtonState *state, const uint32_t command[11],
                          const AbUiImageResourceBoundaries *resources) {
    switch (command[0]) {
        case 0:
            return ab_ui_image_state_set_image(
                &state->image, value(command[1], command[2]), resources, NULL);
        case 1:
            return ab_ui_image_state_refresh(&state->image, resources, NULL);
        case 2:
            if (command[7] & 1) {
                state->image.frame.fields.w = value(AB_UI_POINTER_NUMBER, command[8]);
            }
            if (command[7] & 2) {
                state->image.frame.fields.h = value(AB_UI_POINTER_NUMBER, command[9]);
            }
            return true;
        case 3:
            return ab_ui_image_button_state_enable(state, value(command[5], command[6]),
                                                   resources, NULL);
        case 4:
            return ab_ui_image_button_state_set_image(
                state, value(command[1], command[2]), value(command[3], command[4]),
                resources, NULL);
        default:
            state->image.frame.fields.angle = value(AB_UI_POINTER_NUMBER, command[10]);
            return true;
    }
}

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_UI_IMAGE_RESOURCES_V1 32 256\n") == 0);
    size_t operations[6] = {0}, call_counts[3] = {0};
    for (size_t configuration = 0; configuration < 32; ++configuration) {
        uint32_t setup[11];
        CHECK(words(file, setup, 11) && valid_value(setup[9], setup[10]));
        AbUiImageButtonResources state = {.enabled = value(setup[9], setup[10])};
        state.image.draw.size = (AbVec2){number(setup[0]), number(setup[1])};
        state.image.sprite_pivot = (AbVec2){number(setup[2]), number(setup[3])};
        state.image.draw.rotation_pivot = (AbVec2){number(setup[4]), number(setup[5])};
        state.image.draw.pivot = (AbVec2){number(setup[6]), number(setup[7])};
        state.image.draw.frame.angle = number(setup[8]);
        state.image.draw.pivot_present = 15;
        Boundaries calls = {.state = &state, .fail_kind = UINT32_MAX};
        CHECK(words(file, calls.localization, 5));
        for (size_t id = 0; id < 5; ++id) {
            CHECK(calls.localization[id] < 5 && words(file, calls.geometry[id], 4));
        }
        AbUiImageResourceBoundaries resources = {&calls, localize, bounds, pivot};
        AbUiImageButtonState typed = {.enabled = value(setup[9], setup[10])};
        AbUiPointerValue *numeric[9];
        typed_numeric(&typed, numeric);
        for (size_t field = 0; field < 9; ++field) {
            *numeric[field] = value(AB_UI_POINTER_NUMBER, setup[field]);
        }
        Boundaries typed_calls = calls;
        typed_calls.state = NULL;
        typed_calls.typed = &typed;
        AbUiImageResourceBoundaries typed_resources = {&typed_calls, localize, bounds,
                                                       pivot};
        for (size_t step = 0; step < 256; ++step) {
            uint32_t command[11];
            CHECK(words(file, command, 11) && command[0] < 6 &&
                  (command[1] == 0 || command[1] == 3) &&
                  valid_value(command[1], command[2]) &&
                  (command[3] == 0 || command[3] == 3 ||
                   (command[3] == 1 && !command[4])) &&
                  valid_value(command[3], command[4]) &&
                  valid_value(command[5], command[6]) && command[7] < 4);
            calls.count = 0;
            bool okay;
            switch (command[0]) {
                case 0:
                    okay = ab_ui_image_resource_set(
                        &state.image, value(command[1], command[2]), &resources, NULL);
                    break;
                case 1:
                    okay = ab_ui_image_resource_refresh(&state.image, &resources, NULL);
                    break;
                case 2:
                    okay = ab_ui_image_resource_size(
                        &state.image, (AbVec2){number(command[8]), number(command[9])},
                        command[7]);
                    break;
                case 3:
                    okay = ab_ui_image_button_resource_enable(
                        &state, value(command[5], command[6]), &resources, NULL);
                    break;
                case 4:
                    okay = ab_ui_image_button_resource_set(
                        &state, value(command[1], command[2]),
                        value(command[3], command[4]), &resources, NULL);
                    break;
                default:
                    okay =
                        ab_ui_image_resource_angle(&state.image, number(command[10]));
                    break;
            }
            CHECK(okay);
            typed_calls.count = 0;
            CHECK(typed_command(&typed, command, &typed_resources));
            ++operations[command[0]];
            uint32_t expected[18], actual[18];
            CHECK(words(file, expected, 18) && expected[17] <= 6);
            state_words(&state, actual);
            actual[17] = (uint32_t)calls.count;
            for (size_t field = 0; field < 18; ++field) {
                if (actual[field] != expected[field]) {
                    fprintf(stderr,
                            "UI resource %zu/%zu field %zu: %08" PRIx32 " != %08" PRIx32
                            "\n",
                            configuration, step, field, actual[field], expected[field]);
                    return false;
                }
            }
            uint32_t typed_actual[18];
            CHECK(typed_state_words(&typed, typed_actual));
            typed_actual[17] = (uint32_t)typed_calls.count;
            CHECK(memcmp(typed_actual, expected, sizeof(expected)) == 0);
            for (size_t index = 0; index < calls.count; ++index) {
                uint32_t call[14];
                CHECK(words(file, call, 14) && call[0] < 3);
                if (memcmp(call, calls.calls[index], sizeof(call)) != 0 ||
                    memcmp(call, typed_calls.calls[index], sizeof(call)) != 0) {
                    fprintf(stderr, "UI resource %zu/%zu callback %zu differs\n",
                            configuration, step, index);
                    return false;
                }
                ++call_counts[call[0]];
            }
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB UI image resources reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf(
        "All 8192 original UI resource commands (%zu/%zu/%zu/%zu/%zu/%zu operations), "
        "%zu localizations, %zu bounds and %zu pivot callbacks with prefix state "
        "match both resolved and typed value models.\n",
        operations[0], operations[1], operations[2], operations[3], operations[4],
        operations[5], call_counts[0], call_counts[1], call_counts[2]);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
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
