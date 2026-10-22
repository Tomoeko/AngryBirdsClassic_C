#include "ab/theme_layer.h"
#include "ab/image_points.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Theme base check failed at %d\n", __LINE__);              \
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
    size_t count, fail_at;
    AbVec2 positions[8];
} Calls;

static bool record(void *context, AbVec2 position, AbVec2 scale, const float tint[4],
                   AbError *error) {
    Calls *calls = context;
    CHECK(scale.x == 1 && scale.y == 1 && tint[0] == 255 && calls->count < 8);
    calls->positions[calls->count++] = position;
    return calls->count != calls->fail_at || ab_fail(error, "Prepared draw failure");
}

typedef struct {
    AbSpriteRegion region;
    size_t lookups, images, stop_lookup, stop_image;
    AbVec2 first;
} CompositeCalls;

static bool record_lookup(void *context, AbSpan name, const AbSpriteRegion **output,
                          AbError *error) {
    (void)name;
    CompositeCalls *calls = context;
    if (++calls->lookups == calls->stop_lookup) {
        return ab_fail(error, "Prepared composite lookup failure");
    }
    *output = &calls->region;
    return true;
}

static bool record_image(void *context, const AbSpriteRegion *region,
                         const AbVec2 vertices[4], const float tint[4],
                         AbError *error) {
    CompositeCalls *calls = context;
    CHECK(region == &calls->region && tint[0] == 1);
    if (!calls->images) {
        calls->first = vertices[0];
    }
    return ++calls->images != calls->stop_image ||
           ab_fail(error, "Prepared composite image failure");
}

static bool composite_edges(void) {
    AbCompositePart parts[] = {{.x = 3, .y = 7, .scale_x_bits = UINT32_C(0x7fc00000)},
                               {.x = 11, .y = 13}};
    AbComposite composite = {.parts = parts, .part_count = 2};
    CompositeCalls calls = {.region = {.width = 10, .height = 20, .pivot_x = -2}};
    AbThemeCompositeBoundaries boundaries = {&calls, record_lookup, record_image};
    AbError error = {0};
    const float tint[] = {1, 2, 3, 4};
    CHECK(ab_theme_composite_draw(&composite, (AbVec2){0, 0}, (AbVec2){1, 1}, tint,
                                  &boundaries, &error) &&
          calls.lookups == 2 && calls.images == 2 && calls.first.x == 4 &&
          calls.first.y == 7);
    calls.lookups = calls.images = 0;
    calls.stop_lookup = 2;
    CHECK(!ab_theme_composite_draw(&composite, (AbVec2){0, 0}, (AbVec2){1, 1}, tint,
                                   &boundaries, &error) &&
          calls.lookups == 2 && calls.images == 1);
    calls.lookups = calls.images = calls.stop_lookup = 0;
    calls.stop_image = 2;
    CHECK(!ab_theme_composite_draw(&composite, (AbVec2){0, 0}, (AbVec2){1, 1}, tint,
                                   &boundaries, &error) &&
          calls.lookups == 2 && calls.images == 2);
    composite.part_count = 0;
    calls.lookups = calls.images = 0;
    CHECK(ab_theme_composite_draw(&composite, (AbVec2){0, 0}, (AbVec2){1, 1}, tint,
                                  &boundaries, &error) &&
          !calls.lookups && !calls.images);
    return true;
}

static bool image_point_edges(void) {
    AbVec2 points[] = {{0, 0}, {480, 0}, {0, 320}, {480, 320}};
    const float uv[] = {0, 0, 1, 0, 0, 1, 1, 1};
    AbDrawQuad quad;
    bool visible;
    AbError error = {0};
    CHECK(ab_image_points_quad(points, 480, 320, uv, &quad, &visible, &error) &&
          visible && quad.vertices[0].x == -1 && quad.vertices[0].y == 1 &&
          quad.vertices[3].x == 1 && quad.vertices[3].y == -1);
    for (size_t index = 0; index < 4; ++index) {
        points[index] = (AbVec2){480, 160};
    }
    CHECK(ab_image_points_quad(points, 480, 320, uv, &quad, &visible, &error) &&
          !visible);
    for (size_t index = 0; index < 4; ++index) {
        points[index] = (AbVec2){0, 320};
    }
    CHECK(ab_image_points_quad(points, 480, 320, uv, &quad, &visible, &error) &&
          visible);
    uint8_t retained[sizeof(quad)];
    memcpy(retained, &quad, sizeof(quad));
    CHECK(!ab_image_points_quad(points, 0, 320, uv, &quad, &visible, &error) &&
          visible && memcmp(retained, &quad, sizeof(quad)) == 0);
    points[0].x = NAN;
    CHECK(!ab_image_points_quad(points, 480, 320, uv, &quad, &visible, &error) &&
          visible && memcmp(retained, &quad, sizeof(quad)) == 0);
    return true;
}

static bool edges(void) {
    AbThemeLayerBase layer = {
        .width = 100, .scale = 1, .color = {1, 1, 1, 1}, .repeats = true};
    AbThemeLayerTransform transform = {{{0, 0}, {1, 1}}, 480, 1};
    AbThemeLayerRenderState state = {.geometry = {.rotation_origin_x = 13,
                                                  .rotation_origin_y = 14,
                                                  .direct_vertices = true}};
    AbError error = {0};
    Calls calls = {0};
    CHECK(ab_theme_layer_draw_base(&layer, &transform, NAN, &state, 8, record, &calls,
                                   &error) &&
          calls.count == 6 && calls.positions[0].x == -100 &&
          calls.positions[5].x == 400 && state.alpha == 1 &&
          state.geometry.rotation_origin_x == 13 &&
          state.geometry.rotation_origin_y == 14 && state.geometry.direct_vertices);
    calls = (Calls){0};
    CHECK(!ab_theme_layer_draw_base(&layer, &transform, 0, &state, 2, record, &calls,
                                    &error) &&
          calls.count == 2 && state.geometry.scale_x == 1);
    calls = (Calls){.fail_at = 3};
    CHECK(!ab_theme_layer_draw_base(&layer, &transform, 0, &state, 8, record, &calls,
                                    &error) &&
          calls.count == 3);
    calls = (Calls){0};
    layer.repeats = false;
    layer.x_offset = -25;
    layer.vertical_offset = 7;
    CHECK(ab_theme_layer_draw_base(&layer, &transform, 0, &state, 1, record, &calls,
                                   &error) &&
          calls.count == 1 && calls.positions[0].x == -25 && calls.positions[0].y == 7);
    layer.speed = 1;
    calls.count = 0;
    CHECK(!ab_theme_layer_draw_base(&layer, &transform, NAN, &state, 1, record, &calls,
                                    &error) &&
          calls.count == 0);
    AbSpriteRegion region = {.width = 10, .height = 20, .pivot_x = -2, .pivot_y = 3};
    AbVec2 vertices[4];
    CHECK(ab_theme_sprite_vertices(&region, (AbVec2){0, 0}, (AbVec2){1, 1}, vertices,
                                   &error) &&
          vertices[0].x == 1 && vertices[0].y == -3 && vertices[3].x == 11 &&
          vertices[3].y == 17);
    AbVec2 retained[4];
    memcpy(retained, vertices, sizeof(retained));
    CHECK(!ab_theme_sprite_vertices(&region, (AbVec2){NAN, 0}, (AbVec2){1, 1}, vertices,
                                    &error) &&
          memcmp(retained, vertices, sizeof(retained)) == 0);
    return composite_edges() && image_point_edges();
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
    const AbThemeLayerRenderState *state;
    size_t item, calls;
    bool composite, image_batch;
    size_t quads, culled;
    AbComposite definition;
    AbCompositePart parts[4];
    AbSpriteRegion regions[4];
} Comparison;

static void state_words(const Comparison *comparison, uint32_t output[24]) {
    memcpy(output, comparison->input + 24, 24 * sizeof(*output));
    const AbThemeLayerRenderState *state = comparison->state;
    output[4] = output[7] = UINT32_C(0x3f800000);
    output[5] = output[18] = 0;
    output[6] = UINT32_C(0x80000000);
    output[8] = bits(state->geometry.translation_x);
    output[9] = bits(state->geometry.translation_y);
    output[10] = bits(state->geometry.scale_x);
    output[11] = bits(state->geometry.scale_y);
    output[16] = bits(state->alpha);
}

static bool equal_words(const uint32_t *actual, const uint32_t *expected, size_t count,
                        const Comparison *comparison, const char *kind) {
    for (size_t index = 0; index < count; ++index) {
        if (actual[index] != expected[index]) {
            fprintf(stderr,
                    "Theme base %zu call %zu %s[%zu]: %08" PRIx32 " != %08" PRIx32 "\n",
                    comparison->item, comparison->calls, kind, index, actual[index],
                    expected[index]);
            return false;
        }
    }
    return true;
}

static bool compare_batch(Comparison *comparison, const AbVec2 points[4],
                          const float uv[8], const float tint[4], AbError *error) {
    AbDrawQuad quad;
    bool visible;
    uint32_t drawn;
    CHECK(ab_image_points_quad(points, comparison->input[0], comparison->input[23], uv,
                               &quad, &visible, error) &&
          words(comparison->file, &drawn, 1) && drawn <= 1 && visible == (drawn != 0));
    comparison->quads += visible;
    comparison->culled += !visible;
    if (!visible) {
        return true;
    }
    uint32_t expected[54], actual[54];
    CHECK(words(comparison->file, expected, 18) &&
          words(comparison->file, expected + 18, 12) &&
          words(comparison->file, expected + 30, 24));
    const size_t order[] = {0, 1, 2, 2, 1, 3};
    for (size_t index = 0; index < 6; ++index) {
        const AbDrawVertex *vertex = quad.vertices + order[index];
        actual[index * 3] = bits(vertex->x);
        actual[index * 3 + 1] = bits(vertex->y);
        actual[index * 3 + 2] = 0;
        actual[18 + index * 2] = bits(vertex->u);
        actual[19 + index * 2] = bits(vertex->v);
        for (size_t channel = 0; channel < 4; ++channel) {
            actual[30 + index * 4 + channel] = bits(tint[channel]);
        }
    }
    return equal_words(actual, expected, 54, comparison, "image batch");
}

/* Compose the native DrawUtils/Sprite image boundary from prepared metadata.
 * Preserve off-diagonal zero products, multiply/add order and signed zeros. */
static bool compare_image(void *context, const AbSpriteRegion *region,
                          const AbVec2 points[4], const float tint[4], AbError *error) {
    (void)error;
    Comparison *comparison = context;
    const uint32_t *input = comparison->input;
    uint32_t expected[60], actual[60];
    CHECK(words(comparison->file, expected, 12) &&
          words(comparison->file, expected + 12, 8) &&
          words(comparison->file, expected + 20, 16) &&
          words(comparison->file, expected + 36, 24));
    float right = (float)region->width / (float)input[21];
    float bottom = (float)region->height / (float)input[22];
    const float uv[] = {0, 0, right, 0, 0, bottom, right, bottom};
    for (size_t index = 0; index < 4; ++index) {
        actual[index * 3] = bits(points[index].x);
        actual[index * 3 + 1] = bits(points[index].y);
        actual[index * 3 + 2] = 0;
        for (size_t channel = 0; channel < 4; ++channel) {
            actual[20 + index * 4 + channel] = bits(tint[channel]);
        }
    }
    for (size_t index = 0; index < 8; ++index) {
        actual[12 + index] = bits(uv[index]);
    }
    state_words(comparison, actual + 36);
    CHECK(equal_words(actual, expected, 60, comparison, "image"));
    CHECK(!comparison->image_batch ||
          compare_batch(comparison, points, uv, tint, error));
    ++comparison->calls;
    return true;
}

static const char *const part_names[] = {"part0", "part1", "part2", "part3"};

static bool compare_lookup(void *context, AbSpan name, const AbSpriteRegion **output,
                           AbError *error) {
    (void)error;
    Comparison *comparison = context;
    for (size_t index = 0; index < comparison->definition.part_count; ++index) {
        if (name.size == 5 && memcmp(name.data, part_names[index], 5) == 0) {
            *output = comparison->regions + index;
            return true;
        }
    }
    return false;
}

static bool compare_draw(void *context, AbVec2 position, AbVec2 scale,
                         const float tint[4], AbError *error) {
    Comparison *comparison = context;
    if (comparison->composite) {
        AbThemeCompositeBoundaries boundaries = {context, compare_lookup,
                                                 compare_image};
        return ab_theme_composite_draw(&comparison->definition, position, scale, tint,
                                       &boundaries, error);
    }
    const uint32_t *input = comparison->input;
    AbSpriteRegion region = {.width = (int16_t)input[5],
                             .height = (int16_t)input[6],
                             .pivot_x = (int16_t)(int32_t)input[7],
                             .pivot_y = (int16_t)(int32_t)input[8]};
    AbVec2 points[4];
    return ab_theme_sprite_vertices(&region, position, scale, points, error) &&
           compare_image(context, &region, points, tint, error);
}

static bool read_composite(FILE *file, Comparison *comparison) {
    uint32_t count;
    CHECK(words(file, &count, 1) && count <= 4);
    comparison->definition =
        (AbComposite){.parts = comparison->parts, .part_count = count};
    for (size_t index = 0; index < count; ++index) {
        uint32_t part[12];
        CHECK(words(file, part, 12) && part[9] <= 1 && part[10] <= 1 && !part[11]);
        comparison->regions[index] =
            (AbSpriteRegion){.width = (int16_t)part[0],
                             .height = (int16_t)part[1],
                             .pivot_x = (int16_t)(int32_t)part[2],
                             .pivot_y = (int16_t)(int32_t)part[3]};
        comparison->parts[index] =
            (AbCompositePart){.sprite = {(const uint8_t *)part_names[index], 5},
                              .x = (int16_t)(int32_t)part[4],
                              .y = (int16_t)(int32_t)part[5],
                              .scale_x_bits = part[6],
                              .scale_y_bits = part[7],
                              .rotation_bits = part[8],
                              .flip_x = part[9] != 0,
                              .flip_y = part[10] != 0};
    }
    return true;
}

static bool compare(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file));
    bool image_batch = strcmp(header, "AB_ANDROID_THEME_LAYER_BASE_V3 4096\n") == 0;
    bool composite =
        image_batch || strcmp(header, "AB_ANDROID_THEME_LAYER_BASE_V2 4096\n") == 0;
    CHECK(composite || strcmp(header, "AB_ANDROID_THEME_LAYER_BASE_V1 4096\n") == 0);
    size_t images = 0, empty = 0, largest = 0, quads = 0, culled = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[48], expected[24], result[3], actual[24];
        CHECK(words(file, input, 48) && input[0] <= INT32_MAX && input[11] <= 1 &&
              (image_batch ? input[23] > 0 : input[23] == 0));
        AbThemeLayerTransform transform = {{{number(input[1]), number(input[2])},
                                            {number(input[3]), number(input[4])}},
                                           (int32_t)input[0],
                                           number(input[9])};
        AbThemeLayerBase layer = {.width = (float)input[5],
                                  .pivot_x = (float)(int32_t)input[7],
                                  .scale = number(input[10]),
                                  .x_offset = number(input[12]),
                                  .speed = number(input[13]),
                                  .phase = number(input[14]),
                                  .vertical_offset = number(input[15]),
                                  .color = {number(input[17]), number(input[18]),
                                            number(input[19]), number(input[20])},
                                  .repeats = input[11] != 0};
        AbThemeLayerRenderState state = {0};
        Comparison comparison = {.file = file,
                                 .input = input,
                                 .state = &state,
                                 .item = item,
                                 .composite = composite,
                                 .image_batch = image_batch};
        CHECK(!composite || read_composite(file, &comparison));
        AbError error = {0};
        CHECK(ab_theme_layer_draw_base(&layer, &transform, number(input[16]), &state,
                                       512, compare_draw, &comparison, &error));
        CHECK(words(file, result, 3) && result[0] == comparison.calls &&
              result[1] == 1000 && result[2] == 1 && words(file, expected, 24));
        state_words(&comparison, actual);
        CHECK(equal_words(actual, expected, 24, &comparison, "state"));
        images += comparison.calls;
        quads += comparison.quads;
        culled += comparison.culled;
        empty += comparison.calls == 0;
        if (comparison.calls > largest) {
            largest = comparison.calls;
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB theme layer base reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 native theme base draws match: %zu images, %zu empty layers, "
           "maximum %zu images per layer.\n",
           images, empty, largest);
    if (image_batch) {
        printf("Native CPU image batches match %zu quads and %zu culls.\n", quads,
               culled);
    }
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
