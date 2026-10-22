#include "ab/theme_background.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Theme background check failed at %d\n", __LINE__);        \
            return false;                                                              \
        }                                                                              \
    } while (0)

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

typedef struct {
    size_t calls, stop;
    uint32_t *color;
    AbThemeLayerRenderState *render;
} Calls;

static bool record(void *context, uintptr_t layer, uint32_t filter, AbError *error) {
    Calls *calls = context;
    CHECK(layer == 7 && filter == UINT32_MAX && *calls->color == UINT32_C(0xff00feff));
    ++calls->calls;
    calls->render->alpha = 7;
    return calls->calls != calls->stop || ab_fail(error, "Prepared layer failure");
}

static bool edges(void) {
    AbThemeBackground background = {.color = {-0.5f, 254.75f, 400}};
    uint32_t color = 17;
    AbThemeLayerRenderState render = {.alpha = NAN};
    AbThemeBackgroundState state = {&color, &render};
    AbError error = {0};
    CHECK(ab_theme_background_draw(&background, state, 0, NULL, NULL, &error) &&
          color == UINT32_C(0xff00feff) && render.alpha == 1);
    const uintptr_t layers[] = {7, 7};
    background.layers = layers;
    background.count = 2;
    Calls calls = {.color = &color, .render = &render};
    CHECK(ab_theme_background_draw(&background, state, UINT32_MAX, record, &calls,
                                   &error) &&
          calls.calls == 2 && render.alpha == 1);
    calls.calls = 0;
    calls.stop = 2;
    CHECK(!ab_theme_background_draw(&background, state, UINT32_MAX, record, &calls,
                                    &error) &&
          calls.calls == 2 && render.alpha == 7 && color == UINT32_C(0xff00feff));
    background.color[0] = NAN;
    calls.calls = 0;
    CHECK(!ab_theme_background_draw(&background, state, UINT32_MAX, record, &calls,
                                    &error) &&
          !calls.calls && render.alpha == 7 && color == UINT32_C(0xff00feff));
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
    FILE *file;
    const uint32_t *input;
    AbThemeBackgroundState state;
    size_t calls;
} Comparison;

static bool compare_draw(void *context, uintptr_t layer, uint32_t filter,
                         AbError *error) {
    (void)error;
    Comparison *comparison = context;
    uint32_t expected[4];
    CHECK(layer >= 1 && layer <= 8 && words(comparison->file, expected, 4) &&
          expected[0] == layer && expected[1] == filter &&
          expected[2] == *comparison->state.background_argb &&
          expected[3] == bits(comparison->state.render->alpha));
    comparison->state.render->alpha = number(comparison->input[5 + layer]);
    ++comparison->calls;
    return true;
}

static bool compare(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_THEME_BACKGROUND_V1 8192\n") == 0);
    size_t calls = 0;
    for (size_t item = 0; item < 8192; ++item) {
        uint32_t input[22], expected[3], color = UINT32_C(0x12345678);
        CHECK(words(file, input, 22) && input[0] <= 8);
        uintptr_t layers[8];
        for (size_t index = 0; index < 8; ++index) {
            CHECK(input[14 + index] >= 1 && input[14 + index] <= 8);
            layers[index] = input[14 + index];
        }
        AbThemeBackground background = {
            .color = {number(input[2]), number(input[3]), number(input[4])},
            .layers = layers,
            .count = input[0]};
        AbThemeLayerRenderState render = {.alpha = number(input[5])};
        AbThemeBackgroundState state = {&color, &render};
        Comparison comparison = {file, input, state, 0};
        AbError error = {0};
        CHECK(ab_theme_background_draw(&background, state, input[1], compare_draw,
                                       &comparison, &error) &&
              words(file, expected, 3) && expected[0] == comparison.calls &&
              expected[1] == color && expected[2] == bits(render.alpha));
        calls += comparison.calls;
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB theme background reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 8192 native background compositions match: %zu layer callbacks, "
           "exact clear RGB packing and alpha resets.\n",
           calls);
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
