#include "ab/theme_foreground.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Theme foreground check failed at %d\n", __LINE__);        \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float number(uint32_t word) {
    float value;
    memcpy(&value, &word, sizeof(value));
    return value;
}

static uint32_t bits(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

typedef struct {
    AbThemeLayerRenderState *render;
    size_t fills, layers, stop_layer;
    int32_t top;
} Calls;

static bool record_fill(void *context, const AbGameRectanglePlan *plan,
                        AbError *error) {
    (void)error;
    Calls *calls = context;
    CHECK(plan->reset_state && plan->argb == UINT32_C(0xff7f3319) && !plan->left &&
          plan->top == calls->top && plan->width == 480 &&
          plan->height == 321 - calls->top);
    ++calls->fills;
    calls->render->alpha = 1;
    return true;
}

static bool record_layer(void *context, uintptr_t layer, uint32_t filter,
                         AbError *error) {
    Calls *calls = context;
    CHECK(layer == 3 && filter == 9 && calls->fills == 1);
    ++calls->layers;
    calls->render->alpha = 3;
    return calls->layers != calls->stop_layer ||
           ab_fail(error, "Prepared layer failure");
}

static bool edges(void) {
    AbThemeForeground foreground = {.ground_color = {0.5f, 0.2f, 0.1f},
                                    .first = {true, NAN, NAN, NAN}};
    AbThemeForegroundInput input = {.camera = {{NAN, -100}, {NAN, 2}},
                                    .viewport_width = 480,
                                    .viewport_height = 320,
                                    .filter = 9};
    AbThemeLayerRenderState render = {.alpha = 17};
    Calls calls = {.render = &render, .top = 200};
    AbThemeForegroundBoundaries boundaries = {&calls, record_fill, record_layer};
    AbError error = {0};
    CHECK(ab_theme_foreground_draw(&foreground, &input, &render, &boundaries, &error) &&
          calls.fills == 1 && !calls.layers && render.alpha == 1);
    const uintptr_t layers[] = {3, 3};
    foreground.layers = layers;
    foreground.count = 2;
    foreground.first = (AbThemeForegroundFirstLayer){true, 100.75f, 25.5f, 0.5f};
    calls = (Calls){.render = &render, .top = 275};
    CHECK(ab_theme_foreground_draw(&foreground, &input, &render, &boundaries, &error) &&
          calls.fills == 1 && calls.layers == 2 && render.alpha == 1);
    calls = (Calls){.render = &render, .top = 275, .stop_layer = 2};
    CHECK(
        !ab_theme_foreground_draw(&foreground, &input, &render, &boundaries, &error) &&
        calls.fills == 1 && calls.layers == 2 && render.alpha == 3);
    input.camera.top_left.y = NAN;
    calls.fills = calls.layers = 0;
    CHECK(
        !ab_theme_foreground_draw(&foreground, &input, &render, &boundaries, &error) &&
        !calls.fills && !calls.layers && render.alpha == 3);
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
    AbThemeLayerRenderState *render;
    size_t fills, layers;
} Comparison;

static bool compare_fill(void *context, const AbGameRectanglePlan *plan,
                         AbError *error) {
    (void)error;
    Comparison *comparison = context;
    uint32_t expected[8];
    CHECK(plan->reset_state && words(comparison->file, expected, 8));
    /* The resolved fill provider applies only the alpha reset observed by this
     * composition corpus; full RenderState2D reset contents remain separate. */
    comparison->render->alpha = 1;
    uint32_t actual[] = {plan->argb,
                         (uint32_t)plan->left,
                         (uint32_t)plan->top,
                         (uint32_t)plan->width,
                         (uint32_t)plan->height,
                         1,
                         bits(comparison->render->alpha),
                         2};
    for (size_t index = 0; index < 8; ++index) {
        CHECK(actual[index] == expected[index]);
    }
    ++comparison->fills;
    return true;
}

static bool compare_layer(void *context, uintptr_t layer, uint32_t filter,
                          AbError *error) {
    (void)error;
    Comparison *comparison = context;
    uint32_t expected[4];
    CHECK(layer >= 1 && layer <= 8 && words(comparison->file, expected, 4) &&
          expected[0] == layer && expected[1] == filter &&
          expected[2] == bits(comparison->render->alpha) &&
          expected[3] == comparison->fills);
    comparison->render->alpha = number(comparison->input[22 + layer]);
    ++comparison->layers;
    return true;
}

static bool compare(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_THEME_FOREGROUND_V1 4096\n") == 0);
    size_t layers_total = 0, base = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[33], expected[3];
        CHECK(words(file, input, 33) && input[0] <= 8 && input[7] <= 1 &&
              input[2] <= INT32_MAX && input[3] <= INT32_MAX && !input[32]);
        uintptr_t layers[8];
        for (size_t index = 0; index < 8; ++index) {
            layers[index] = input[15 + index];
        }
        AbThemeForeground foreground = {
            .ground_color = {number(input[11]), number(input[12]), number(input[13])},
            .layers = layers,
            .count = input[0],
            .first = {input[7] != 0, number(input[8]), number(input[9]),
                      number(input[10])}};
        AbThemeForegroundInput view = {.camera = {{number(input[31]), number(input[4])},
                                                  {number(input[6]), number(input[5])}},
                                       .viewport_width = (int32_t)input[2],
                                       .viewport_height = (int32_t)input[3],
                                       .filter = input[1]};
        AbThemeLayerRenderState render = {.alpha = number(input[14])};
        Comparison comparison = {file, input, &render, 0, 0};
        AbThemeForegroundBoundaries boundaries = {&comparison, compare_fill,
                                                  compare_layer};
        AbError error = {0};
        CHECK(ab_theme_foreground_draw(&foreground, &view, &render, &boundaries,
                                       &error) &&
              words(file, expected, 3) && expected[0] == comparison.layers &&
              expected[1] == comparison.fills && expected[2] == bits(render.alpha));
        layers_total += comparison.layers;
        base += foreground.count && foreground.first.is_base_layer;
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB theme foreground reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 native foreground compositions match: 4096 ground fills, %zu "
           "layer callbacks and %zu first-base-layer metric cases.\n",
           layers_total, base);
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
