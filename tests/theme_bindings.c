#include "ab/composite_resources.h"
#include "ab/draw.h"
#include "ab/sprite_atlas.h"
#include "ab/target_data.h"
#include "ab/theme_layers.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Live theme binding failed at %d\n", __LINE__);            \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t bits(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static const AbDataValue *property(const AbDataTable *table, const char *name) {
    return ab_data_get(table, text(name));
}

static AbDataValue integer(size_t value) {
    return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits((float)value)};
}

typedef struct {
    size_t images, fail_at;
} ImageCalls;

static bool image_call(void *context, const AbSpriteBinding *sprite,
                       const AbVec2 vertices[4], const float tint[4], AbError *error) {
    ImageCalls *calls = context;
    CHECK(sprite && sprite->sheet && sprite->region && vertices && tint[0] == 255);
    return ++calls->images != calls->fail_at ||
           ab_fail(error, "Prepared image failure");
}

static bool schema_edges(const AbDataTable *theme, const AbSpriteSet *sprites,
                         const AbCompositeResources *composites) {
    AbDataEntry entries[64];
    CHECK(theme->count <= 64);
    memcpy(entries, theme->entries, theme->count * sizeof(*entries));
    AbDataTable root = {.entries = entries, .count = theme->count, .capacity = 64};
    AbDataEntry layer_entries[] = {{.key = integer(2), .value = text("absent")},
                                   {.key = text("type"), .value = text("unknown")}};
    AbDataTable layer = {.entries = layer_entries, .count = 2, .capacity = 2};
    AbDataEntry background_entries[] = {
        {.key = integer(1), .value = {.kind = AB_DATA_TABLE, .table = &layer}},
        {.key = integer(2), .value = {.kind = AB_DATA_BOOLEAN}},
        {.key = integer(3), .value = {.kind = AB_DATA_TABLE, .table = NULL}}};
    AbDataTable background = {.entries = background_entries, .count = 3, .capacity = 3};
    AbDataTable foreground = {0};
    size_t replaced = 0;
    for (size_t index = 0; index < root.count; ++index) {
        AbDataEntry *entry = entries + index;
        const char *const names[] = {"bgLayers", "fgLayers"};
        for (size_t name_index = 0; name_index < 2; ++name_index) {
            if (entry->key.kind == AB_DATA_STRING &&
                entry->key.string.size == strlen(names[name_index]) &&
                memcmp(entry->key.string.data, names[name_index],
                       entry->key.string.size) == 0) {
                entry->value =
                    (AbDataValue){.kind = AB_DATA_TABLE,
                                  .table = name_index ? &foreground : &background};
                ++replaced;
            }
        }
    }
    CHECK(replaced == 2);
    AbError error = {0};
    AbThemeLayers *trial = NULL;
    CHECK(ab_theme_layers_open(&root, sprites, composites, 4096, &trial, &error));
    const AbThemeLayerBinding *binding;
    AbThemeForeground view;
    CHECK(ab_theme_layers_count(trial) == 1 &&
          ab_theme_layers_binding(trial, 1, &binding) &&
          binding->kind == AB_THEME_SPRITE_MISSING &&
          !binding->definition.has_base_sprite &&
          ab_theme_layers_foreground(trial, &view) && !view.count);
    AbThemeLayerRenderState state = {.alpha = -7};
    uint8_t retained[sizeof(state)];
    memcpy(retained, &state, sizeof(state));
    AbThemeLayerDrawInput input = {0};
    ImageCalls calls = {0};
    CHECK(ab_theme_layers_draw_base(trial, 1, &input, &state, image_call, &calls,
                                    &error) &&
          !calls.images && memcmp(retained, &state, sizeof(state)) == 0 &&
          !ab_theme_layers_draw_base(trial, 2, &input, &state, image_call, &calls,
                                     &error) &&
          memcmp(retained, &state, sizeof(state)) == 0);
    ab_theme_layers_close(trial);
    trial = NULL;
    layer_entries[1].value = text("rect");
    bool okay = ab_theme_layers_open(&root, sprites, composites, 4096, &trial, &error);
    bool cleared = trial == NULL;
    ab_theme_layers_close(trial);
    CHECK(!okay && cleared);
    layer_entries[1].value = text("unknown");
    layer.count = 3;
    okay = ab_theme_layers_open(&root, sprites, composites, 4096, &trial, &error);
    cleared = trial == NULL;
    ab_theme_layers_close(trial);
    CHECK(!okay && cleared);
    puts("Theme owner stops at the first non-table slot, preserves missing-layer "
         "state and rejects unsupported subtypes/malformed tables.");
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool name(FILE *file, AbSpan expected) {
    char line[160];
    CHECK(fgets(line, sizeof(line), file) && strlen(line) == expected.size + 1 &&
          line[expected.size] == '\n' &&
          memcmp(line, expected.data, expected.size) == 0);
    return true;
}

static bool sprite(FILE *file, const AbSpriteAtlasBinding *binding) {
    uint32_t expected[12], actual[12];
    CHECK(words(file, expected, 12));
    const AbSpriteRegion *region = binding->sprite.region;
    actual[0] = (uint16_t)region->x | ((uint32_t)(uint16_t)region->y << 16);
    actual[1] = (uint16_t)region->width | ((uint32_t)(uint16_t)region->height << 16);
    actual[2] = bits((float)region->pivot_x);
    actual[3] = bits((float)region->pivot_y);
    AbDrawQuad quad;
    AbError error = {0};
    CHECK(ab_sprite_quad(region, binding->image->width, binding->image->height,
                         (AbSpritePlacement){.scale_x = 1, .scale_y = 1}, &quad,
                         &error));
    for (size_t index = 0; index < 4; ++index) {
        actual[4 + index * 2] = bits(quad.vertices[index].u);
        actual[5 + index * 2] = bits(quad.vertices[index].v);
    }
    CHECK(memcmp(actual, expected, sizeof(actual)) == 0);
    return true;
}

static bool composite(FILE *file, const AbResolvedComposite *resolved,
                      const AbSpriteAtlas *atlas, uint32_t expected_count) {
    const AbComposite *source = resolved->source;
    CHECK(source && source->part_count == expected_count && expected_count <= 32);
    uint32_t expected_bounds[4];
    CHECK(words(file, expected_bounds, 4));
    for (size_t index = 0; index < source->part_count; ++index) {
        const AbCompositePart *part = source->parts + index;
        CHECK(name(file, part->sprite));
        uint32_t expected[8];
        CHECK(words(file, expected, 8));
        /* The enabled flag is one byte; the remaining word is allocator padding. */
        expected[7] &= 255;
        uint32_t actual[] = {bits((float)part->x),
                             bits((float)part->y),
                             part->scale_x_bits,
                             part->scale_y_bits,
                             bits(part->flip_x ? -1.0f : 1.0f),
                             bits(part->flip_y ? -1.0f : 1.0f),
                             part->rotation_bits,
                             1};
        CHECK(memcmp(actual, expected, sizeof(actual)) == 0);
        AbSpriteAtlasBinding binding;
        CHECK(ab_sprite_atlas_find(atlas, part->sprite, &binding) &&
              binding.sprite.region == resolved->parts[index].region &&
              sprite(file, &binding));
    }
    AbCompositeBounds bounds = resolved->bounds;
    uint32_t actual[] = {(uint32_t)bounds.width, (uint32_t)bounds.height,
                         (uint32_t)bounds.pivot_x, (uint32_t)bounds.pivot_y};
    CHECK(memcmp(actual, expected_bounds, sizeof(actual)) == 0);
    return true;
}

static bool compare(FILE *file, const AbSpriteAtlas *atlas,
                    const AbThemeLayers *layers) {
    CHECK(name(file, (AbSpan){(const uint8_t *)"AB_NATIVE_THEME1_BINDINGS_V1 5", 30}));
    const char *const names[] = {"BACKGROUND_1_LAYER_1", "THEME_1_BG_LAYER_2",
                                 "THEME_1_BG_LAYER_3", "FOREGROUND_1_LAYER_1",
                                 "FOREGROUND_1_LAYER_2"};
    size_t ordinary = 0, compounds = 0;
    for (size_t index = 0; index < 5; ++index) {
        AbSpan key = {(const uint8_t *)names[index], strlen(names[index])};
        uint32_t kind[2];
        CHECK(name(file, key) && words(file, kind, 2));
        const AbThemeLayerBinding *layer;
        CHECK(ab_theme_layers_binding(layers, index + 1, &layer) &&
              layer->definition.has_base_sprite &&
              layer->definition.sprite.size == key.size &&
              memcmp(layer->definition.sprite.data, key.data, key.size) == 0);
        AbSpriteAtlasBinding binding;
        if (layer->kind == AB_THEME_SPRITE_ORDINARY) {
            CHECK(kind[0] == 1 && !kind[1] &&
                  ab_sprite_atlas_find(atlas, key, &binding) &&
                  binding.sprite.region == layer->ordinary.region &&
                  sprite(file, &binding));
            ++ordinary;
            continue;
        }
        CHECK(kind[0] == 2 && layer->kind == AB_THEME_SPRITE_COMPOSITE &&
              composite(file, &layer->composite, atlas, kind[1]));
        ++compounds;
    }
    const char *marker = "AB native theme1 bindings complete";
    CHECK(name(file, (AbSpan){(const uint8_t *)marker, strlen(marker)}) &&
          fgetc(file) == EOF && !ferror(file) && ordinary == 3 && compounds == 2);
    puts("All five live theme1 bindings match: three ordinary sprites, two "
         "composites, eleven ordered parts, exact metadata/UVs and composite bounds.");
    return true;
}

static bool image_formats(FILE *file, const AbSpriteAtlas *atlas) {
    char line[128];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_NATIVE_THEME1_IMAGE_FORMATS_V1 14\n") == 0);
    size_t opaque = 0, alpha = 0;
    for (size_t index = 0; index < 14; ++index) {
        CHECK(fgets(line, sizeof(line), file));
        size_t length = strlen(line);
        CHECK(length && line[length - 1] == '\n');
        uint32_t expected[6];
        AbSpriteAtlasBinding binding;
        CHECK(words(file, expected, 6) &&
              ab_sprite_atlas_find(atlas, (AbSpan){(const uint8_t *)line, length - 1},
                                   &binding) &&
              binding.image->width == expected[0] &&
              binding.image->height == expected[1] && expected[2] == expected[3] &&
              expected[4] == UINT32_C(0xe5900020) &&
              expected[5] == UINT32_C(0xe12fff1e));
        if (expected[2] == 33) {
            CHECK(binding.alpha == AB_SPRITE_ALPHA_OPAQUE);
            ++opaque;
        } else {
            CHECK(expected[2] == 18 && binding.alpha == AB_SPRITE_ALPHA_PRESENT);
            ++alpha;
        }
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB native theme1 image formats complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file) && opaque == 2 && alpha == 12);
    puts("Fourteen live theme1 image bindings match dimensions and native ETC1/"
         "RGBA4444 alpha presence; selected getter reads SurfaceFormat at0x20.");
    return true;
}

static bool layer_owner_edges(const AbDataTable *theme, const AbSpriteSet *sprites,
                              const AbCompositeResources *composites,
                              const AbThemeLayers *layers) {
    AbThemeBackground background;
    AbThemeForeground foreground;
    CHECK(ab_theme_layers_count(layers) == 5 &&
          ab_theme_layers_background(layers, &background) && background.count == 3 &&
          background.layers[0] == 1 && background.layers[2] == 3 &&
          ab_theme_layers_foreground(layers, &foreground) && foreground.count == 2 &&
          foreground.layers[0] == 4 && foreground.layers[1] == 5 &&
          foreground.first.is_base_layer && foreground.first.scale == 1);
    const float parallax[] = {0.125f, 0.5f, 1, 1, 1};
    const float scale[] = {2, 2, 1.5f, 1, 1};
    for (size_t index = 0; index < 5; ++index) {
        const AbThemeLayerBinding *binding;
        CHECK(ab_theme_layers_binding(layers, index + 1, &binding) &&
              binding->definition.parallax == parallax[index] &&
              binding->definition.base.scale == scale[index]);
    }
    uint32_t color;
    AbError error = {0};
    CHECK(ab_background_color_rgb(background.color, &color, &error) &&
          color == UINT32_C(0xff92ccde));
    const AbThemeColors *colors = ab_theme_layers_colors(layers);
    CHECK(colors && colors->tint[0] == 255 && colors->tint[1] == 255 &&
          colors->tint[2] == 255);
    AbThemeLayerDrawInput input = {
        .camera = {{0, 0}, {1, 1}}, .viewport_width = 480, .placement_limit = 128};
    AbThemeLayerRenderState state = {0};
    ImageCalls calls = {.fail_at = 1};
    CHECK(!ab_theme_layers_draw_base(layers, 1, &input, &state, image_call, &calls,
                                     &error) &&
          calls.images == 1 && state.alpha == 1 && state.geometry.scale_x == 2);
    size_t bytes = ab_theme_layers_bytes(layers);
    AbThemeLayers *trial = NULL;
    bool okay = ab_theme_layers_open(theme, sprites, composites, bytes, &trial, &error);
    size_t count = ab_theme_layers_count(trial);
    ab_theme_layers_close(trial);
    trial = NULL;
    CHECK(okay && count == 5 && bytes > 1);
    okay = ab_theme_layers_open(theme, sprites, composites, bytes - 1, &trial, &error);
    bool cleared = trial == NULL;
    ab_theme_layers_close(trial);
    CHECK(!okay && cleared);
    printf("Theme layer owner retains %zu bytes for three background/two foreground "
           "bindings and colors.\n",
           bytes);
    return schema_edges(theme, sprites, composites);
}

static bool owner_edges(const AbZip *archive, const AbSpriteSet *sprites,
                        const char *path, const AbCompositeResources *resources) {
    AbError error = {0};
    CHECK(ab_composite_resources_count(resources) == 2);
    AbResolvedComposite view;
    CHECK(ab_composite_resources_item(resources, 0, &view));
    uint8_t retained[sizeof(view)];
    memcpy(retained, &view, sizeof(view));
    CHECK(!ab_composite_resources_item(resources, 2, &view) &&
          memcmp(retained, &view, sizeof(view)) == 0 &&
          !ab_composite_resources_find(resources, (AbSpan){0}, &view) &&
          memcmp(retained, &view, sizeof(view)) == 0);
    size_t bytes = ab_composite_resources_bytes(resources);
    CHECK(bytes > 1);
    AbCompositeResources *trial = NULL;
    bool okay = ab_composite_resources_open(archive, sprites, &path, 1, bytes,
                                            1024 * 1024, &trial, &error);
    size_t count = ab_composite_resources_count(trial);
    ab_composite_resources_close(trial);
    trial = NULL;
    CHECK(okay && count == 2);
    const size_t limits[] = {0, bytes - 1};
    for (size_t index = 0; index < 2; ++index) {
        okay = ab_composite_resources_open(archive, sprites, &path, 1, limits[index],
                                           1024 * 1024, &trial, &error);
        bool cleared = trial == NULL;
        ab_composite_resources_close(trial);
        trial = NULL;
        CHECK(!okay && cleared);
    }
    AbSpriteSet *empty = NULL;
    CHECK(ab_sprite_set_open(archive, NULL, 0, 65536, &empty, &error));
    okay = ab_composite_resources_open(archive, empty, &path, 1, 2 * 1024 * 1024,
                                       1024 * 1024, &trial, &error);
    bool cleared = trial == NULL;
    ab_composite_resources_close(trial);
    ab_sprite_set_close(empty);
    CHECK(!okay && cleared);
    printf("Composite owner retains %zu bytes; exact/short budgets and missing-part "
           "cleanup pass.\n",
           bytes);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 && argc != 4) {
        return 2;
    }
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbSpriteSet *sprites = NULL;
    AbSpriteAtlas *atlas = NULL;
    AbCompositeResources *composites = NULL;
    AbTargetData themes = {0};
    AbThemeLayers *layers = NULL;
    uint8_t resource_key[32] = {0};
    AbError error = {0};
    const char *const paths[] = {
        "assets/data/images/themes_android/THEME_01_ALT.dat",
        "assets/data/images/themes_android/THEME_01_PARALLAX_1.dat",
        "assets/data/images/themes_android/THEME_01_THEME_1.dat",
        "assets/data/images/themes_android/THEME_01_THEME_GROUND_1.dat"};
    const char *composite_path =
        "assets/data/images/themes_android/THEME_01_COMPOSPRITES.dat";
    bool okay = ab_file_read(argv[1], 64 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error);
    okay = okay &&
           ab_sprite_set_open(&archive, paths, 4, 2 * 1024 * 1024, &sprites, &error) &&
           ab_sprite_atlas_open(&archive, sprites, 64 * 1024 * 1024, 8 * 1024 * 1024,
                                &atlas, &error) &&
           ab_composite_resources_open(&archive, sprites, &composite_path, 1,
                                       2 * 1024 * 1024, 1024 * 1024, &composites,
                                       &error) &&
           owner_edges(&archive, sprites, composite_path, composites) &&
           ab_target_resource_key(&archive, 16 * 1024 * 1024, resource_key, &error) &&
           ab_target_data_open(&archive, "assets/data/scripts/themes.lua",
                               (AbSpan){resource_key, sizeof(resource_key)},
                               4 * 1024 * 1024, NULL, &themes, &error);
    if (okay) {
        const AbDataValue *theme = property(themes.data.globals, "theme1");
        okay = theme && theme->kind == AB_DATA_TABLE &&
               ab_theme_layers_open(theme->table, sprites, composites, 1024 * 1024,
                                    &layers, &error) &&
               layer_owner_edges(theme->table, sprites, composites, layers);
    }
    if (okay) {
        FILE *file = fopen(argv[2], "rb");
        okay = file && compare(file, atlas, layers);
        if (file && fclose(file) != 0) {
            okay = false;
        }
    }
    if (okay && argc == 4) {
        FILE *file = fopen(argv[3], "rb");
        okay = file && image_formats(file, atlas);
        if (file && fclose(file) != 0) {
            okay = false;
        }
    }
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    ab_theme_layers_close(layers);
    ab_target_data_close(&themes);
    memset(resource_key, 0, sizeof(resource_key));
    ab_composite_resources_close(composites);
    ab_sprite_atlas_close(atlas);
    ab_sprite_set_close(sprites);
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay ? 0 : 1;
}
