#ifndef AB_THEME_PREVIEW_ASSETS_H
#define AB_THEME_PREVIEW_ASSETS_H

#include "ab/fill_rectangle.h"
#include "ab/image_points.h"
#include "ab/sprite_atlas.h"
#include "ab/target_data.h"
#include "ab/theme_draw.h"

#include <string.h>

enum { PREVIEW_THEME_BINDINGS = 32, PREVIEW_THEME_DRAWS = 512 };

typedef struct {
    const AbSpriteRegion *region;
    uint32_t texture;
    float uv[8];
    bool alpha;
} PreviewThemeImage;

typedef struct {
    AbTargetData data;
    AbSpriteSet *sprites;
    AbSpriteAtlas *atlas;
    AbCompositeResources *composites;
    AbThemeLayers *layers;
    PreviewThemeImage images[PREVIEW_THEME_BINDINGS];
    size_t image_count;
    uint32_t first_texture, white_texture, background;
    uint8_t white_rgba[4];
    AbImage white;
    AbThemeLayerRenderState render;
    AbSpriteProvider provider;
    AbThemeDraw draw;
} PreviewTheme;

static bool theme_preview_source_sprite(void *context, AbSpan name,
                                        AbSpriteAtlasBinding *output);
static bool theme_preview_source_texture(void *context, const AbImage *image,
                                         uint32_t *texture);

static void theme_preview_free(PreviewTheme *theme) {
    ab_theme_layers_close(theme->layers);
    ab_composite_resources_close(theme->composites);
    ab_sprite_atlas_close(theme->atlas);
    ab_sprite_set_close(theme->sprites);
    ab_target_data_close(&theme->data);
    *theme = (PreviewTheme){0};
}

static bool theme_preview_bind_images(PreviewTheme *theme, AbError *error) {
    size_t count = ab_sprite_set_count(theme->sprites);
    for (size_t sheet_index = 0; sheet_index < count; ++sheet_index) {
        AbSpriteSheetView sheet;
        if (!ab_sprite_set_sheet(theme->sprites, sheet_index, &sheet)) {
            return ab_fail(error, "Cannot enumerate theme preview sprites");
        }
        for (size_t index = 0; index < sheet.sheet->count; ++index) {
            const AbSpriteRegion *region = sheet.sheet->sprites + index;
            AbSpriteAtlasBinding binding;
            AbDrawQuad quad;
            if (theme->image_count == PREVIEW_THEME_BINDINGS ||
                !ab_sprite_atlas_find(theme->atlas, region->name, &binding) ||
                binding.sprite.region != region ||
                binding.alpha == AB_SPRITE_ALPHA_UNKNOWN ||
                !ab_sprite_quad(region, binding.image->width, binding.image->height,
                                (AbSpritePlacement){.scale_x = 1, .scale_y = 1}, &quad,
                                error)) {
                return ab_fail(error, "Unsupported theme preview image binding");
            }
            PreviewThemeImage *image = theme->images + theme->image_count++;
            image->region = region;
            image->texture = theme->first_texture + (uint32_t)binding.image_index;
            image->alpha = binding.alpha == AB_SPRITE_ALPHA_PRESENT;
            for (size_t vertex = 0; vertex < 4; ++vertex) {
                image->uv[vertex * 2] = quad.vertices[vertex].u;
                image->uv[vertex * 2 + 1] = quad.vertices[vertex].v;
            }
        }
    }
    return true;
}

/* Explicit theme1 resource selection for the fixed level-one research view.
 * Native group/variant selection, settings, dynamic subtypes and effects are
 * separate. All five selected layers are static and have zero speed/phase. */
static bool theme_preview_open(const AbZip *archive, const AbDataTable *level,
                               AbSpan key, uint32_t first_texture, PreviewTheme *output,
                               AbError *error) {
    *output = (PreviewTheme){0};
    const AbDataValue *name =
        ab_data_get(level, (AbDataValue){.kind = AB_DATA_STRING,
                                         .string = {(const uint8_t *)"theme", 5}});
    if (!name || name->kind != AB_DATA_STRING || name->string.size != 6 ||
        !name->string.data || memcmp(name->string.data, "theme1", 6) != 0 ||
        !first_texture) {
        return ab_fail(error, "Loaded preview requires explicit theme1 data");
    }
    const char *const paths[] = {
        "assets/data/images/themes_android/THEME_01_ALT.dat",
        "assets/data/images/themes_android/THEME_01_PARALLAX_1.dat",
        "assets/data/images/themes_android/THEME_01_THEME_1.dat",
        "assets/data/images/themes_android/THEME_01_THEME_GROUND_1.dat"};
    const char *composite =
        "assets/data/images/themes_android/THEME_01_COMPOSPRITES.dat";
    bool okay = false;
    output->first_texture = first_texture;
    if (!ab_target_data_open(archive, "assets/data/scripts/themes.lua", key,
                             4 * 1024 * 1024, NULL, &output->data, error) ||
        !ab_sprite_set_open(archive, paths, 4, 2 * 1024 * 1024, &output->sprites,
                            error) ||
        !ab_sprite_atlas_open(archive, output->sprites, 64 * 1024 * 1024,
                              8 * 1024 * 1024, &output->atlas, error) ||
        !ab_composite_resources_open(archive, output->sprites, &composite, 1,
                                     2 * 1024 * 1024, 1024 * 1024, &output->composites,
                                     error)) {
        goto release_theme;
    }
    const AbDataValue *definition = ab_data_get(output->data.data.globals, *name);
    size_t texture_count = ab_sprite_atlas_count(output->atlas);
    if (!definition || definition->kind != AB_DATA_TABLE ||
        texture_count > UINT32_MAX - first_texture ||
        !ab_theme_layers_open(definition->table, output->sprites, output->composites,
                              1024 * 1024, &output->layers, error) ||
        !theme_preview_bind_images(output, error)) {
        goto release_theme;
    }
    for (size_t index = 0; index < ab_theme_layers_count(output->layers); ++index) {
        const AbThemeLayerBinding *binding;
        if (!ab_theme_layers_binding(output->layers, index + 1, &binding) ||
            binding->definition.base.speed != 0 ||
            binding->definition.base.phase != 0) {
            ab_fail(error, "Theme preview timing requires separate recovery");
            goto release_theme;
        }
    }
    output->white_texture = first_texture + (uint32_t)texture_count;
    memset(output->white_rgba, 255, sizeof(output->white_rgba));
    output->white = (AbImage){1, 1, {output->white_rgba, sizeof(output->white_rgba)}};
    output->provider = (AbSpriteProvider){.context = output,
                                          .sprite = theme_preview_source_sprite,
                                          .composite = NULL,
                                          .texture = theme_preview_source_texture};
    okay = ab_theme_draw_initialize(
        (AbThemeDrawConfig){output->layers, &output->provider, &output->render,
                            &output->background, output->white_texture,
                            PREVIEW_THEME_DRAWS},
        &output->draw, error);

release_theme:
    if (!okay) {
        theme_preview_free(output);
    }
    return okay;
}

static size_t theme_preview_texture_count(const PreviewTheme *theme) {
    return ab_sprite_atlas_count(theme->atlas) + 1;
}

static bool theme_preview_texture(const PreviewTheme *theme, size_t index,
                                  uint32_t *texture, const AbImage **image) {
    size_t count = ab_sprite_atlas_count(theme->atlas);
    if (index > count) {
        return false;
    }
    if (index == count) {
        *texture = theme->white_texture;
        *image = &theme->white;
        return true;
    }
    AbSpriteAtlasImage view;
    if (!ab_sprite_atlas_image(theme->atlas, index, &view)) {
        return false;
    }
    *texture = theme->first_texture + (uint32_t)index;
    *image = view.image;
    return true;
}

static bool theme_preview_source_sprite(void *context, AbSpan name,
                                        AbSpriteAtlasBinding *output) {
    PreviewTheme *theme = context;
    return ab_sprite_atlas_find(theme->atlas, name, output);
}

static bool theme_preview_source_texture(void *context, const AbImage *image,
                                         uint32_t *texture) {
    PreviewTheme *theme = context;
    for (size_t index = 0; index < ab_sprite_atlas_count(theme->atlas); ++index) {
        AbSpriteAtlasImage item;
        if (!ab_sprite_atlas_image(theme->atlas, index, &item))
            return false;
        if (item.image == image) {
            *texture = theme->first_texture + (uint32_t)index;
            return true;
        }
    }
    return false;
}

static bool theme_preview_draw(PreviewTheme *theme, AbCoordinateCamera camera,
                               bool foreground, AbDrawList *list, AbError *error) {
    AbThemeDrawInput input = {camera, 480, 320, 0, 0};
    return foreground ? ab_theme_draw_foreground(&theme->draw, input, list, error)
                      : ab_theme_draw_background(&theme->draw, input, list, error);
}

static void theme_preview_clear(const PreviewTheme *theme, float color[4]) {
    const float reciprocal = 1.0f / 255.0f;
    color[0] = (float)((theme->background >> 16) & 255) * reciprocal;
    color[1] = (float)((theme->background >> 8) & 255) * reciprocal;
    color[2] = (float)(theme->background & 255) * reciprocal;
    color[3] = 1;
}

#endif
