#include "ab/image_points.h"
#include "ab/fill_rectangle.h"
#include "ab/sprite_atlas.h"
#include "ab/target_data.h"
#include "ab/theme_layers.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Theme scene check failed at %d\n", __LINE__);             \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t bits(float number) {
    uint32_t value;
    memcpy(&value, &number, sizeof(value));
    return value;
}

static float number(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool marker(FILE *file, const char *expected) {
    char line[128];
    CHECK(fgets(line, sizeof(line), file) && strcmp(line, expected) == 0);
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
    const AbSpriteAtlas *atlas;
    const AbThemeLayers *layers;
    AbThemeLayerDrawInput input;
    AbThemeLayerRenderState state;
    uint32_t background;
    uint32_t width, height;
    size_t frame, images, fills, quads, culled;
    size_t fill_quads, fill_culled;
    bool fill_batch;
} Comparison;

static bool equal_words(Comparison *comparison, const char *label,
                        const uint32_t *actual, const uint32_t *expected,
                        size_t count) {
    for (size_t index = 0; index < count; ++index) {
        if (actual[index] != expected[index]) {
            fprintf(stderr,
                    "Theme frame %zu image %zu %s[%zu]: %08" PRIx32 " != %08" PRIx32
                    "\n",
                    comparison->frame, comparison->images, label, index, actual[index],
                    expected[index]);
            return false;
        }
    }
    return true;
}

static bool compare_state(Comparison *comparison) {
    const AbThemeLayerRenderState *state = &comparison->state;
    const AbSpriteRenderState *geometry = &state->geometry;
    uint32_t actual[] = {bits(geometry->translation_x),
                         bits(geometry->translation_y),
                         bits(geometry->scale_x),
                         bits(geometry->scale_y),
                         bits(state->alpha),
                         bits(geometry->angle_radians)};
    uint32_t expected[6];
    return words(comparison->file, expected, 6) &&
           equal_words(comparison, "state", actual, expected, 6);
}

static bool compare_constructor(Comparison *comparison) {
    const AbThemeColors *colors = ab_theme_layers_colors(comparison->layers);
    uint32_t expected[19], actual[19] = {0};
    CHECK(colors && words(comparison->file, expected, 9));
    for (size_t index = 0; index < 3; ++index) {
        actual[index] = bits(colors->background[index]);
        actual[3 + index] = bits(colors->ground[index]);
        actual[6 + index] = bits(colors->tint[index]);
    }
    CHECK(equal_words(comparison, "colors", actual, expected, 9) &&
          ab_theme_layers_count(comparison->layers) == 5);
    for (size_t index = 0; index < 5; ++index) {
        const AbThemeLayerBinding *binding;
        CHECK(ab_theme_layers_binding(comparison->layers, index + 1, &binding) &&
              words(comparison->file, expected, 19));
        const AbThemeLayerDefinition *definition = &binding->definition;
        const AbThemeLayerBase *base = &definition->base;
        memset(actual, 0, sizeof(actual));
        actual[0] = definition->has_base_sprite;
        actual[1] = bits(base->width);
        actual[2] = bits(definition->height);
        actual[3] = bits(base->pivot_x);
        actual[4] = bits(definition->pivot_y);
        actual[5] = bits(definition->parallax);
        actual[6] = bits(base->scale);
        actual[7] = base->repeats;
        actual[8] = bits(base->x_offset);
        actual[9] = bits(base->speed);
        for (size_t channel = 0; channel < 4; ++channel) {
            actual[10 + channel] = bits(base->color[channel]);
        }
        actual[14] = bits(base->phase);
        actual[15] = bits(base->vertical_offset);
        /* Native bool stores leave upper allocator padding outside the value. */
        expected[0] &= 255;
        expected[7] &= 255;
        CHECK(equal_words(comparison, "constructor", actual, expected, 19));
    }
    return true;
}

static bool compare_batch(Comparison *comparison, const AbVec2 vertices[4],
                          const float uv[8], const float tint[4],
                          AbImagePointMaterial material, AbError *error) {
    AbDrawCommand command = {0};
    bool visible;
    uint32_t drawn;
    CHECK(ab_image_points_command(vertices, comparison->width, comparison->height, uv,
                                  material, &command, &visible, error) &&
          words(comparison->file, &drawn, 1) && drawn == (uint32_t)visible);
    if (!visible) {
        ++comparison->culled;
        return true;
    }
    ++comparison->quads;
    const AbDrawQuad *quad = &command.quad;
    CHECK(command.coordinates == AB_DRAW_CLIP && command.texture == material.texture &&
          command.opacity == (material.has_alpha ? material.render_alpha : 1));
    uint32_t actual[54], expected[54];
    CHECK(words(comparison->file, expected, 18) &&
          words(comparison->file, expected + 18, 12) &&
          words(comparison->file, expected + 30, 24));
    const size_t order[] = {0, 1, 2, 2, 1, 3};
    for (size_t index = 0; index < 6; ++index) {
        const AbDrawVertex *vertex = quad->vertices + order[index];
        actual[index * 3] = bits(vertex->x);
        actual[index * 3 + 1] = bits(vertex->y);
        actual[index * 3 + 2] = 0;
        actual[18 + index * 2] = bits(vertex->u);
        actual[19 + index * 2] = bits(vertex->v);
        for (size_t channel = 0; channel < 4; ++channel) {
            actual[30 + index * 4 + channel] = bits(tint[channel]);
        }
    }
    return equal_words(comparison, "CPU batch", actual, expected, 54);
}

static bool compare_image(void *context, const AbSpriteBinding *sprite,
                          const AbVec2 vertices[4], const float tint[4],
                          AbError *error) {
    Comparison *comparison = context;
    AbSpriteAtlasBinding atlas;
    AbDrawQuad quad;
    const AbSpriteRegion *region = sprite->region;
    CHECK(comparison->images < 4096 &&
          ab_sprite_atlas_find(comparison->atlas, region->name, &atlas) &&
          atlas.sprite.region == region && atlas.alpha != AB_SPRITE_ALPHA_UNKNOWN &&
          atlas.image_index < UINT32_MAX &&
          ab_sprite_quad(region, atlas.image->width, atlas.image->height,
                         (AbSpritePlacement){.scale_x = 1, .scale_y = 1}, &quad,
                         error) &&
          marker(comparison->file, "I\n"));
    uint32_t actual[48], expected[48];
    float uv[8];
    actual[0] = (uint16_t)region->x | ((uint32_t)(uint16_t)region->y << 16);
    actual[1] = (uint16_t)region->width | ((uint32_t)(uint16_t)region->height << 16);
    actual[2] = bits((float)region->pivot_x);
    actual[3] = bits((float)region->pivot_y);
    for (size_t index = 0; index < 4; ++index) {
        uv[index * 2] = quad.vertices[index].u;
        uv[index * 2 + 1] = quad.vertices[index].v;
        actual[4 + index * 2] = actual[24 + index * 2] = bits(uv[index * 2]);
        actual[5 + index * 2] = actual[25 + index * 2] = bits(uv[index * 2 + 1]);
        actual[12 + index * 3] = bits(vertices[index].x);
        actual[13 + index * 3] = bits(vertices[index].y);
        actual[14 + index * 3] = 0;
        for (size_t channel = 0; channel < 4; ++channel) {
            actual[32 + index * 4 + channel] = bits(tint[channel]);
        }
    }
    CHECK(words(comparison->file, expected, 12) &&
          words(comparison->file, expected + 12, 12) &&
          words(comparison->file, expected + 24, 8) &&
          words(comparison->file, expected + 32, 16) &&
          equal_words(comparison, "image", actual, expected, 48) &&
          compare_state(comparison) &&
          compare_batch(comparison, vertices, uv, tint,
                        (AbImagePointMaterial){(uint32_t)atlas.image_index + 1,
                                               atlas.alpha == AB_SPRITE_ALPHA_PRESENT,
                                               comparison->state.alpha},
                        error));
    ++comparison->images;
    return true;
}

static bool compare_fill_batch(Comparison *comparison, const AbGameRectanglePlan *plan,
                               AbError *error) {
    CHECK(marker(comparison->file, "R\n"));
    uint32_t summary[2];
    AbDrawCommand command = {0};
    bool visible;
    CHECK(words(comparison->file, summary, 2) &&
          ab_fill_rectangle_reset_command(
              (AbFillRectangle){plan->argb, plan->left, plan->top, plan->width,
                                plan->height, AB_FILL_BLEND_AUTO},
              comparison->width, comparison->height, 5, &command, &visible, error) &&
          (uint32_t)visible == summary[0]);
    if (!visible) {
        ++comparison->fill_culled;
        CHECK(!summary[1]);
        return true;
    }
    CHECK(summary[1] == 1 && command.blend == AB_BLEND_OPAQUE);
    uint32_t actual[42], expected[42];
    CHECK(words(comparison->file, expected, 18) &&
          words(comparison->file, expected + 18, 24));
    const size_t order[] = {0, 1, 2, 2, 1, 3};
    for (size_t index = 0; index < 6; ++index) {
        const AbDrawVertex *vertex = command.quad.vertices + order[index];
        actual[index * 3] = bits(vertex->x);
        actual[index * 3 + 1] = bits(vertex->y);
        actual[index * 3 + 2] = UINT32_C(0x3a83126f);
        for (size_t channel = 0; channel < 4; ++channel) {
            actual[18 + index * 4 + channel] = bits(command.color[channel] * 255);
        }
    }
    CHECK(equal_words(comparison, "ground CPU batch", actual, expected, 42));
    ++comparison->fill_quads;
    return true;
}

static bool compare_fill(void *context, const AbGameRectanglePlan *plan,
                         AbError *error) {
    Comparison *comparison = context;
    CHECK(!comparison->fills && plan->reset_state && marker(comparison->file, "F\n"));
    /* Compare the reset's resolved geometry/alpha boundary; other native state
     * fields and actual fill rasterization remain outside this checker. */
    comparison->state =
        (AbThemeLayerRenderState){.geometry = {.scale_x = 1, .scale_y = 1}, .alpha = 1};
    uint32_t actual[] = {plan->argb, (uint32_t)plan->left, (uint32_t)plan->top,
                         (uint32_t)plan->width, (uint32_t)plan->height};
    uint32_t expected[5];
    CHECK(words(comparison->file, expected, 5) &&
          equal_words(comparison, "ground fill", actual, expected, 5) &&
          compare_state(comparison));
    CHECK(!comparison->fill_batch || compare_fill_batch(comparison, plan, error));
    ++comparison->fills;
    return true;
}

static bool compare_layer(void *context, uintptr_t layer, uint32_t filter,
                          AbError *error) {
    (void)filter;
    Comparison *comparison = context;
    return ab_theme_layers_draw_base(comparison->layers, layer, &comparison->input,
                                     &comparison->state, compare_image, comparison,
                                     error);
}

static bool compare_summary(Comparison *comparison, const char *phase) {
    CHECK(marker(comparison->file, phase));
    uint32_t expected[3];
    uint32_t actual[] = {(uint32_t)comparison->images, (uint32_t)comparison->fills,
                         comparison->background};
    return words(comparison->file, expected, 3) &&
           equal_words(comparison, "composition", actual, expected, 3) &&
           compare_state(comparison);
}

static bool compare(Comparison *comparison, AbError *error) {
    char line[80];
    CHECK(fgets(line, sizeof(line), comparison->file));
    comparison->fill_batch = strcmp(line, "AB_ANDROID_THEME_SCENE_V2 256\n") == 0;
    CHECK((comparison->fill_batch ||
           strcmp(line, "AB_ANDROID_THEME_SCENE_V1 256\n") == 0) &&
          compare_constructor(comparison));
    AbThemeBackground background;
    AbThemeForeground foreground;
    CHECK(ab_theme_layers_background(comparison->layers, &background) &&
          ab_theme_layers_foreground(comparison->layers, &foreground));
    size_t images = 0, fills = 0;
    for (comparison->frame = 0; comparison->frame < 256; ++comparison->frame) {
        uint32_t input[9];
        CHECK(words(comparison->file, input, 9) && input[0] <= INT32_MAX &&
              input[1] <= INT32_MAX);
        comparison->width = input[0];
        comparison->height = input[1];
        comparison->input =
            (AbThemeLayerDrawInput){.camera = {{number(input[2]), number(input[3])},
                                               {number(input[4]), number(input[5])}},
                                    .viewport_width = (int32_t)input[0],
                                    .time = number(input[6]),
                                    .placement_limit = 4096};
        comparison->state = (AbThemeLayerRenderState){.alpha = number(input[8])};
        comparison->images = comparison->fills = 0;
        CHECK(ab_theme_background_draw(
                  &background,
                  (AbThemeBackgroundState){&comparison->background, &comparison->state},
                  input[7], compare_layer, comparison, error) &&
              compare_summary(comparison, "B\n"));
        AbThemeForegroundInput foreground_input = {
            comparison->input.camera, (int32_t)input[0], (int32_t)input[1], input[7]};
        AbThemeForegroundBoundaries boundaries = {comparison, compare_fill,
                                                  compare_layer};
        CHECK(ab_theme_foreground_draw(&foreground, &foreground_input,
                                       &comparison->state, &boundaries, error) &&
              compare_summary(comparison, "G\n"));
        images += comparison->images;
        fills += comparison->fills;
    }
    CHECK(marker(comparison->file, "AB theme scene reference complete\n") &&
          fgetc(comparison->file) == EOF && !ferror(comparison->file) &&
          images == 22335 && fills == 256 &&
          comparison->quads + comparison->culled == images);
    printf("Direct APK theme1 matches original construction and 256 compositions: "
           "%zu images, %zu fills, %zu CPU quads, %zu culls; no GPU pixel claim.\n",
           images, fills, comparison->quads, comparison->culled);
    if (comparison->fill_batch) {
        CHECK(comparison->fill_quads == 222 && comparison->fill_culled == 34);
        printf("Ground context fill also matches %zu opaque CPU quads and %zu culls.\n",
               comparison->fill_quads, comparison->fill_culled);
    }
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        return 2;
    }
    AbError error = {0};
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbSpriteSet *sprites = NULL;
    AbSpriteAtlas *atlas = NULL;
    AbCompositeResources *composites = NULL;
    AbThemeLayers *layers = NULL;
    AbTargetData themes = {0};
    uint8_t key[32] = {0};
    const char *const paths[] = {
        "assets/data/images/themes_android/THEME_01_ALT.dat",
        "assets/data/images/themes_android/THEME_01_PARALLAX_1.dat",
        "assets/data/images/themes_android/THEME_01_THEME_1.dat",
        "assets/data/images/themes_android/THEME_01_THEME_GROUND_1.dat"};
    const char *composite =
        "assets/data/images/themes_android/THEME_01_COMPOSPRITES.dat";
    bool okay =
        ab_file_read(argv[1], 64 * 1024 * 1024, &bytes, &error) &&
        ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
        ab_sprite_set_open(&archive, paths, 4, 2 * 1024 * 1024, &sprites, &error) &&
        ab_sprite_atlas_open(&archive, sprites, 64 * 1024 * 1024, 8 * 1024 * 1024,
                             &atlas, &error) &&
        ab_composite_resources_open(&archive, sprites, &composite, 1, 2 * 1024 * 1024,
                                    1024 * 1024, &composites, &error) &&
        ab_target_resource_key(&archive, 16 * 1024 * 1024, key, &error) &&
        ab_target_data_open(&archive, "assets/data/scripts/themes.lua",
                            (AbSpan){key, sizeof(key)}, 4 * 1024 * 1024, NULL, &themes,
                            &error);
    if (okay) {
        const AbDataValue *theme =
            ab_data_get(themes.data.globals,
                        (AbDataValue){.kind = AB_DATA_STRING,
                                      .string = {(const uint8_t *)"theme1", 6}});
        okay = theme && theme->kind == AB_DATA_TABLE &&
               ab_theme_layers_open(theme->table, sprites, composites, 1024 * 1024,
                                    &layers, &error);
    }
    if (okay) {
        FILE *file = fopen(argv[2], "rb");
        Comparison comparison = {.file = file, .atlas = atlas, .layers = layers};
        okay = file && compare(&comparison, &error);
        if (file && fclose(file) != 0) {
            okay = false;
        }
    }
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    ab_theme_layers_close(layers);
    ab_target_data_close(&themes);
    memset(key, 0, sizeof(key));
    ab_composite_resources_close(composites);
    ab_sprite_atlas_close(atlas);
    ab_sprite_set_close(sprites);
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay ? 0 : 1;
}
