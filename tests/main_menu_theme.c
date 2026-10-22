#include "ab/main_menu_theme.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Main-menu theme failed at %d\n", __LINE__);               \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    uint32_t input[14], calls[8][8], count, fail;
    AbMainMenuThemeState *state;
} Context;

static const char *const themes[] = {"theme1", "themeUGC_menu", "", "blue grass"};
static const char *const texts[] = {"", "water", "tag", "themeUGC_menu"};

static AbSpan text(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static float number(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t bits(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

static bool named(AbSpan value, const char *name) {
    AbSpan candidate = text(name);
    return value.size == candidate.size &&
           (!value.size || memcmp(value.data, candidate.data, value.size) == 0);
}

static bool decode(const uint32_t input[2], AbUiPointerValue *output) {
    CHECK(input[0] <= AB_UI_POINTER_FUNCTION);
    *output = (AbUiPointerValue){.kind = (AbUiPointerKind)input[0]};
    switch (output->kind) {
        case AB_UI_POINTER_NONE:
        case AB_UI_POINTER_FUNCTION:
            CHECK(!input[1]);
            break;
        case AB_UI_POINTER_BOOLEAN:
            CHECK(input[1] <= 1);
            output->boolean = input[1] != 0;
            break;
        case AB_UI_POINTER_NUMBER:
            output->number_bits = input[1];
            break;
        case AB_UI_POINTER_TEXT:
            CHECK(input[1] < 4);
            output->text = text(texts[input[1]]);
            break;
        case AB_UI_POINTER_OBJECT:
            output->object = input[1];
            break;
    }
    return true;
}

static bool encode(AbUiPointerValue value, uint32_t output[2]) {
    CHECK(ab_ui_pointer_value_valid(&value));
    output[0] = (uint32_t)value.kind;
    output[1] = 0;
    switch (value.kind) {
        case AB_UI_POINTER_BOOLEAN:
            output[1] = value.boolean;
            break;
        case AB_UI_POINTER_NUMBER:
            output[1] = value.number_bits;
            break;
        case AB_UI_POINTER_OBJECT:
            output[1] = (uint32_t)value.object;
            break;
        case AB_UI_POINTER_TEXT:
            output[1] = 4;
            for (uint32_t index = 0; index < 4; ++index) {
                if (named(value.text, texts[index])) {
                    output[1] = index;
                }
            }
            CHECK(output[1] < 4);
            break;
        case AB_UI_POINTER_NONE:
        case AB_UI_POINTER_FUNCTION:
            break;
    }
    return true;
}

static bool record(Context *context, uint32_t operation) {
    CHECK(context->count < 8);
    uint32_t *row = context->calls[context->count++];
    memset(row, 0, 8 * sizeof(*row));
    row[0] = operation;
    CHECK(encode(context->state->water_level, row + 5));
    return context->fail != operation;
}

static bool settings(void *context, AbMainMenuSetting operation,
                     AbUiPointerValue *output, AbError *error) {
    (void)error;
    Context *provider = context;
    CHECK(record(provider, (uint32_t)operation));
    if (operation == AB_MAIN_MENU_GET_ZOOM) {
        CHECK(output);
        *output = (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER,
                                     .number_bits = provider->input[1]};
    } else if (operation == AB_MAIN_MENU_GET_THEME) {
        CHECK(output && provider->input[5] < 4);
        *output = (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                                     .text = text(themes[provider->input[5]])};
    } else {
        CHECK(!output && operation == AB_MAIN_MENU_SET_ZOOM);
    }
    return true;
}

static bool sprite(void *context, AbSpan image, AbVec2 *output, AbError *error) {
    (void)error;
    Context *provider = context;
    CHECK(named(image, "BUTTON_EMPTY") && record(provider, 3));
    *output = (AbVec2){number(provider->input[2]), number(provider->input[3])};
    return true;
}

static bool field(void *context, AbSpan theme, AbMainMenuThemeField operation,
                  AbUiPointerValue *output, AbError *error) {
    (void)error;
    Context *provider = context;
    CHECK(named(theme, themes[provider->input[5]]) &&
          operation <= AB_MAIN_MENU_WATER_LEVEL);
    return decode(provider->input + 6 + (size_t)operation * 2, output);
}

static bool call(void *context, AbMainMenuThemeCall operation, AbSpan theme,
                 const float *numbers, size_t count, AbError *error) {
    (void)error;
    Context *provider = context;
    bool okay = record(provider, (uint32_t)operation + 4);
    uint32_t *row = provider->calls[provider->count - 1];
    row[2] = (uint32_t)count;
    if (operation == AB_MAIN_MENU_SELECT_THEME ||
        operation == AB_MAIN_MENU_SELECT_EFFECTS) {
        CHECK(named(theme, themes[provider->input[5]]) && !numbers && !count);
        row[1] = provider->input[5] + 1;
    } else {
        CHECK(!theme.size &&
              count == (operation == AB_MAIN_MENU_CAMERA_ZOOM ? 1u : 2u));
        for (size_t index = 0; index < count; ++index) {
            row[3 + index] = bits(numbers[index]);
        }
    }
    return okay;
}

static bool run(Context *context, float *top) {
    AbMainMenuThemeBoundaries boundaries = {context,
                                            number(context->input[0]),
                                            number(context->input[4]),
                                            settings,
                                            sprite,
                                            field,
                                            call};
    return ab_main_menu_theme_layout(context->state, &boundaries, top, NULL);
}

static bool edges(void) {
    AbMainMenuThemeState state = {.water_level = {.kind = AB_UI_POINTER_TEXT,
                                                  .text = {(const uint8_t *)"tag", 3}}};
    Context context = {.fail = 5, .state = &state};
    context.input[0] = bits(320);
    context.input[1] = bits(1);
    context.input[3] = bits(100);
    context.input[8] = 1;
    context.input[9] = 1;
    context.input[10] = 1;
    float top = 123;
    CHECK(!run(&context, &top) && top == 123 && context.count == 6 &&
          state.water_level.kind == AB_UI_POINTER_TEXT);
    context.count = 0;
    context.fail = 7;
    CHECK(!run(&context, &top) && top == 123 && context.count == 8 &&
          state.water_level.kind == AB_UI_POINTER_NUMBER &&
          state.water_level.number_bits == bits(20));
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool compare(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_MAIN_MENU_THEME_V1 4096\n") == 0);
    for (size_t item = 0; item < 4096; ++item) {
        AbMainMenuThemeState state;
        Context context = {.state = &state, .fail = UINT32_MAX};
        uint32_t expected[3], actual[3], count;
        CHECK(words(file, context.input, 14) &&
              decode(context.input + 12, &state.water_level));
        float top;
        CHECK(run(&context, &top));
        actual[0] = bits(top);
        CHECK(encode(state.water_level, actual + 1) && words(file, expected, 3));
        if (memcmp(actual, expected, sizeof(actual)) != 0) {
            fprintf(stderr, "Main-menu theme result differs at %zu\n", item);
            return false;
        }
        CHECK(words(file, &count, 1) && count == context.count && count == 8);
        for (size_t index = 0; index < count; ++index) {
            uint32_t row[8];
            CHECK(words(file, row, 8));
            if (memcmp(row, context.calls[index], sizeof(row)) != 0) {
                fprintf(stderr, "Main-menu theme callback %zu/%zu differs\n", item,
                        index);
                return false;
            }
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB main-menu theme reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 4096 original main-menu theme layouts, 32768 callbacks, camera values "
         "and typed water-state prefixes match.");
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edges() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = compare(file);
    return fclose(file) == 0 && okay ? 0 : 1;
}
