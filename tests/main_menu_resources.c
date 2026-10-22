#include "ab/main_menu_resources.h"
#include "ab/target_data.h"
#include "ui_value_fixture.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Main-menu resources failed at %d\n", __LINE__);           \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    unsigned calls;
    const char *theme;
    float scale, top, scroll;
    bool native;
    uint32_t rows[4][8];
    AbMainMenuThemeState *state;
} Context;

static const char *const raw_texts[] = {"", "water", "tag", "themeUGC_menu"};

static AbSpan text(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

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

static bool call(void *context, AbMainMenuThemeCall operation, AbSpan theme,
                 const float *numbers, size_t count, AbError *error) {
    (void)error;
    Context *provider = context;
    CHECK((unsigned)operation < 4 && (unsigned)operation == provider->calls++ &&
          count <= 2);
    if (provider->native) {
        uint32_t *row = provider->rows[operation];
        memset(row, 0, sizeof(provider->rows[operation]));
        row[0] = (uint32_t)operation + 4;
        row[1] = theme.size ? (strcmp(provider->theme, "settings") == 0 ? 2u : 1u) : 0;
        row[2] = (uint32_t)count;
        for (size_t index = 0; index < count; ++index) {
            row[3 + index] = bits(numbers[index]);
        }
        CHECK(ab_test_ui_value_encode(provider->state->water_level, raw_texts, 4,
                                      row + 5));
    }
    if (operation == AB_MAIN_MENU_CAMERA_ZOOM) {
        CHECK(count == 1 && !theme.size && isfinite(numbers[0]) && numbers[0] > 0);
        provider->scale = numbers[0];
    } else if (operation == AB_MAIN_MENU_CAMERA_TOP_LEFT) {
        CHECK(count == 2 && !theme.size && bits(numbers[0]) == bits(provider->scroll) &&
              isfinite(numbers[1]));
        provider->top = numbers[1];
    } else {
        CHECK(!count && !numbers && theme.size == strlen(provider->theme) &&
              memcmp(theme.data, provider->theme, theme.size) == 0);
    }
    return true;
}

static bool cases(const AbDataTable *themes, AbSpriteSet *sprites) {
    const char *names[] = {"themeBeach", "settings", "missing", ""};
    for (size_t index = 0; index < 9; ++index) {
        AbMainMenuSettings settings = {
            .zoom = {.kind = AB_UI_POINTER_NUMBER, .number_bits = UINT32_C(0x42c80000)},
            .low_quality = {.kind = AB_UI_POINTER_NUMBER}};
        if (index < 4) {
            settings.theme = (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                                                .text = text(names[index])};
        } else if (index == 5) {
            settings.theme.kind = AB_UI_POINTER_BOOLEAN;
        } else if (index == 6) {
            settings.theme.kind = AB_UI_POINTER_NUMBER;
        } else if (index == 7) {
            settings.theme.kind = AB_UI_POINTER_OBJECT;
        } else if (index == 8) {
            settings.theme.kind = AB_UI_POINTER_FUNCTION;
        }
        Context context = {.theme = index == 1 ? "settings" : "themeBeach",
                           .scroll = -17};
        AbMainMenuResources resources = {themes, sprites,  &settings, 320,
                                         -17,    &context, call};
        AbMainMenuThemeState state = {
            .water_level = {.kind = AB_UI_POINTER_NUMBER, .number_bits = 1}};
        float top;
        CHECK(ab_main_menu_resources_layout(&state, &resources, &top, NULL) &&
              context.calls == 4 && top == context.top &&
              settings.zoom.kind == AB_UI_POINTER_NONE &&
              settings.low_quality.kind == AB_UI_POINTER_NUMBER &&
              !settings.low_quality.number_bits &&
              state.water_level.kind == AB_UI_POINTER_NONE);
    }
    return true;
}

static bool missing(const AbDataTable *themes, AbSpriteSet *empty) {
    AbMainMenuSettings settings = {
        .zoom = {.kind = AB_UI_POINTER_BOOLEAN, .boolean = true}};
    Context context = {.theme = "themeBeach", .scroll = -17};
    AbMainMenuResources resources = {themes, empty,    &settings, 320,
                                     -17,    &context, call};
    AbMainMenuThemeState state = {
        .water_level = {.kind = AB_UI_POINTER_OBJECT, .object = 17}};
    float top = 91;
    CHECK(!ab_main_menu_resources_layout(&state, &resources, &top, NULL) &&
          !context.calls && top == 91 && settings.zoom.kind == AB_UI_POINTER_NONE &&
          state.water_level.kind == AB_UI_POINTER_OBJECT &&
          state.water_level.object == 17);
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool native_case(FILE *file, const AbDataTable *themes, AbSpriteSet *sprites) {
    uint32_t input[14], expected[3], actual[3], count;
    CHECK(words(file, input, 14) && words(file, expected, 3) && input[5] < 9);
    AbMainMenuSettings settings = {
        .zoom = {.kind = AB_UI_POINTER_NUMBER, .number_bits = input[1]}};
    const char *names[] = {"themeBeach", "settings", "", "missing"};
    if (input[5] < 4) {
        settings.theme = (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                                            .text = text(names[input[5]])};
    } else {
        const AbUiPointerKind kinds[] = {AB_UI_POINTER_NONE, AB_UI_POINTER_BOOLEAN,
                                         AB_UI_POINTER_NUMBER, AB_UI_POINTER_OBJECT,
                                         AB_UI_POINTER_FUNCTION};
        settings.theme.kind = kinds[input[5] - 4];
    }
    CHECK(ab_test_ui_value_decode(input + 8, raw_texts, 4, &settings.low_quality));
    AbMainMenuThemeState state;
    CHECK(ab_test_ui_value_decode(input + 12, raw_texts, 4, &state.water_level));
    Context context = {.theme = input[5] == 1 ? "settings" : "themeBeach",
                       .scroll = number(input[4]),
                       .native = true,
                       .state = &state};
    AbMainMenuResources resources = {
        themes, sprites, &settings, number(input[0]), context.scroll, &context, call};
    AbSpriteBinding binding;
    CHECK(ab_sprite_set_find(sprites, text("BUTTON_EMPTY"), &binding) &&
          bits((float)binding.region->width) == input[2] &&
          bits((float)binding.region->height) == input[3]);
    float top;
    CHECK(ab_main_menu_resources_layout(&state, &resources, &top, NULL) &&
          context.calls == 4 && settings.zoom.kind == AB_UI_POINTER_NONE);
    actual[0] = bits(top);
    CHECK(ab_test_ui_value_encode(state.water_level, raw_texts, 4, actual + 1) &&
          memcmp(actual, expected, sizeof(actual)) == 0 && words(file, &count, 1) &&
          count == 8);
    for (size_t index = 0; index < count; ++index) {
        uint32_t row[8];
        CHECK(words(file, row, 8) && row[0] == index);
        if (index >= 4) {
            CHECK(memcmp(row, context.rows[index - 4], sizeof(row)) == 0);
        }
    }
    return true;
}

static bool compare(FILE *file, const AbDataTable *themes, AbSpriteSet *small,
                    AbSpriteSet *large) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_MAIN_MENU_RESOURCES_V1 4096\n") == 0);
    for (size_t item = 0; item < 4096; ++item) {
        if (!native_case(file, themes, item % 2 ? large : small)) {
            fprintf(stderr, "Main-menu APK/native resource case %zu differs\n", item);
            return false;
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB main-menu resources reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 4096 APK menu settings/theme/metadata compositions, camera values and "
         "16384 application callbacks match the original scripts.");
    return true;
}

int main(int argc, char **argv) {
    if (argc != 2 && argc != 3) {
        return 2;
    }
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbTargetData themes = {0};
    AbSpriteSet *sprites = NULL, *large = NULL, *empty = NULL;
    AbError error = {0};
    uint8_t key[32];
    const char *small_path = "assets/data/images/480x320/BUTTONS_SHEET_1.dat";
    const char *large_path = "assets/data/images/1024x768/BUTTONS_SHEET_1.dat";
    bool okay =
        ab_file_read(argv[1], 64 * 1024 * 1024, &bytes, &error) &&
        ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
        ab_target_resource_key(&archive, 16 * 1024 * 1024, key, &error) &&
        ab_target_data_open(&archive, "assets/data/blocks/themes_new.lua",
                            (AbSpan){key, sizeof(key)}, 2 * 1024 * 1024, NULL, &themes,
                            &error) &&
        ab_sprite_set_open(&archive, &small_path, 1, 2 * 1024 * 1024, &sprites,
                           &error) &&
        ab_sprite_set_open(&archive, &large_path, 1, 2 * 1024 * 1024, &large, &error) &&
        ab_sprite_set_open(&archive, NULL, 0, 2 * 1024 * 1024, &empty, &error);
    if (okay) {
        okay = cases(themes.data.globals, sprites) &&
               cases(themes.data.globals, large) && missing(themes.data.globals, empty);
    }
    if (okay && argc == 3) {
        FILE *file = fopen(argv[2], "rb");
        okay = file && compare(file, themes.data.globals, sprites, large);
        if (file && fclose(file) != 0) {
            okay = false;
        }
    }
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    memset(key, 0, sizeof(key));
    ab_sprite_set_close(empty);
    ab_sprite_set_close(large);
    ab_sprite_set_close(sprites);
    ab_target_data_close(&themes);
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay ? 0 : 1;
}
