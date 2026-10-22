#include "ab/main_menu_settings.h"
#include "ui_value_fixture.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Main-menu settings failed at %d\n", __LINE__);            \
            return false;                                                              \
        }                                                                              \
    } while (0)

static const char *const texts[] = {"", "water", "tag", "themeUGC_menu", "theme1"};

typedef struct {
    bool present, fail;
    unsigned calls;
} Lookup;

static bool lookup(void *context, AbUiPointerValue key, bool *output, AbError *error) {
    (void)error;
    Lookup *provider = context;
    CHECK(ab_ui_pointer_value_truthy(&key));
    ++provider->calls;
    *output = provider->present;
    return !provider->fail;
}

static bool encode(AbUiPointerValue value, uint32_t output[2]) {
    return ab_test_ui_value_encode(value, texts, 5, output);
}

static bool decode(const uint32_t input[2], AbUiPointerValue *output) {
    return ab_test_ui_value_decode(input, texts, 5, output);
}

static bool edges(void) {
    AbMainMenuSettings state = {.theme = {.kind = AB_UI_POINTER_NUMBER}};
    Lookup provider = {.present = true, .fail = true};
    AbUiPointerValue output = {.kind = AB_UI_POINTER_OBJECT, .object = 93};
    CHECK(!ab_main_menu_settings_theme(&state, (AbUiPointerValue){0}, lookup, &provider,
                                       &output, NULL) &&
          provider.calls == 1 && output.object == 93);
    provider.fail = false;
    CHECK(ab_main_menu_settings_theme(&state, (AbUiPointerValue){0}, lookup, &provider,
                                      &output, NULL) &&
          output.kind == AB_UI_POINTER_NUMBER && !output.number_bits);
    CHECK(ab_main_menu_settings_set_zoom(&state, (AbUiPointerValue){0}) &&
          ab_main_menu_settings_zoom(&state, &output) &&
          output.number_bits == UINT32_C(0x3fea3d71));
    return true;
}

static bool result(const uint32_t input[13], uint32_t output[22]) {
    AbMainMenuSettings state;
    AbUiPointerValue fallback, new_zoom, new_theme, value;
    CHECK(decode(input, &state.zoom) && decode(input + 2, &state.theme) &&
          decode(input + 4, &state.low_quality) && decode(input + 6, &fallback) &&
          decode(input + 8, &new_zoom) && decode(input + 10, &new_theme) &&
          input[12] < 4);
    Lookup provider = {.present = input[12] >= 2};
    CHECK(ab_main_menu_settings_zoom(&state, &value) && encode(value, output) &&
          ab_main_menu_settings_theme(&state, fallback, lookup, &provider, &value,
                                      NULL) &&
          encode(value, output + 2) && ab_main_menu_settings_quality(&state, &value) &&
          encode(value, output + 4));
    CHECK(ab_main_menu_settings_set_zoom(&state, new_zoom) &&
          ab_main_menu_settings_zoom(&state, &value) && encode(value, output + 6) &&
          encode(state.zoom, output + 8));
    CHECK(ab_main_menu_settings_set_theme(&state, new_theme) &&
          ab_main_menu_settings_theme(&state, fallback, lookup, &provider, &value,
                                      NULL) &&
          encode(value, output + 10) && encode(state.theme, output + 12));
    CHECK(ab_main_menu_settings_set_zoom(&state, (AbUiPointerValue){0}) &&
          ab_main_menu_settings_zoom(&state, &value) && encode(value, output + 14) &&
          encode(state.zoom, output + 16) && encode(state.theme, output + 18) &&
          encode(state.low_quality, output + 20));
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
          strcmp(header, "AB_ANDROID_MAIN_MENU_SETTINGS_V1 4096\n") == 0);
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[13], expected[22], actual[22];
        CHECK(words(file, input, 13) && words(file, expected, 22) &&
              result(input, actual));
        for (size_t field = 0; field < 22; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Settings case %zu field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB main-menu settings reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 4096 original main-menu settings cases and 45056 typed getter/stored "
         "values match.");
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
