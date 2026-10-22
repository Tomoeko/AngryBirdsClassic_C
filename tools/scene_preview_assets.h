#ifndef AB_SCENE_PREVIEW_ASSETS_H
#define AB_SCENE_PREVIEW_ASSETS_H

#include "ab/common_renderer.h"
#include "ab/sprite_scene.h"
#include "font_preview_assets.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>

enum { SCENE_ITEM_LIMIT = 1024, SCENE_ATLAS_LIMIT = 16 };

typedef struct {
    char path[1024];
    AbBuffer bytes;
    AbSpriteSheet sheet;
    AbImage image;
} PreviewAtlas;

typedef struct {
    AbSceneSprite *items;
    size_t *order;
    size_t count;
    PreviewAtlas *atlases;
    size_t atlas_count;
    size_t asset_bytes;
    AbCoordinateCamera camera;
} PreviewScene;

static bool scene_token(FILE *file, char *text, size_t capacity, AbError *error) {
    size_t size = 0;
    int character;
    do {
        character = fgetc(file);
    } while (character != EOF && isspace((unsigned char)character));
    while (character != EOF && !isspace((unsigned char)character)) {
        if (!character || size + 1 >= capacity) {
            return ab_fail(error, "Scene token exceeds its bounded capacity");
        }
        text[size++] = (char)character;
        character = fgetc(file);
    }
    if (!size || ferror(file)) {
        return ab_fail(error, "Incomplete scene preview input");
    }
    text[size] = 0;
    return true;
}

static bool scene_word(FILE *file, int base, uint32_t maximum, uint32_t *output,
                       AbError *error) {
    char text[32], *end;
    if (!scene_token(file, text, sizeof(text), error)) {
        return false;
    }
    errno = 0;
    unsigned long value = strtoul(text, &end, base);
    if (errno || text[0] == '-' || text[0] == '+' || *end || value > maximum) {
        return ab_fail(error, "Invalid unsigned scene field");
    }
    *output = (uint32_t)value;
    return true;
}

static bool scene_layer(FILE *file, int32_t *output, AbError *error) {
    char text[32], *end;
    if (!scene_token(file, text, sizeof(text), error)) {
        return false;
    }
    errno = 0;
    intmax_t value = strtoimax(text, &end, 10);
    if (errno || *end || value < INT32_MIN || value > INT32_MAX) {
        return ab_fail(error, "Invalid signed scene layer");
    }
    *output = (int32_t)value;
    return true;
}

static float scene_float(uint32_t bits) {
    float number;
    memcpy(&number, &bits, sizeof(number));
    return number;
}

static bool scene_atlas(const AbZip *zip, const char *path, PreviewScene *scene,
                        size_t *index, AbError *error) {
    for (size_t item = 0; item < scene->atlas_count; ++item) {
        if (strcmp(scene->atlases[item].path, path) == 0) {
            *index = item;
            return true;
        }
    }
    if (scene->atlas_count == SCENE_ATLAS_LIMIT) {
        return ab_fail(error, "Scene exceeds the preview atlas capacity");
    }
    *index = scene->atlas_count++;
    PreviewAtlas *atlas = scene->atlases + *index;
    memcpy(atlas->path, path, strlen(path) + 1);
    AbBuffer encoded = {0};
    bool okay =
        read_entry(zip, path, &atlas->bytes, error) &&
        ab_sprite_sheet_read((AbSpan){atlas->bytes.data, atlas->bytes.size},
                             8 * 1024 * 1024, &atlas->sheet, error) &&
        metadata_image(zip, path, atlas->sheet.texture, &encoded, &atlas->image, error);
    ab_buffer_free(&encoded);
    if (!okay) {
        return false;
    }
    size_t used = atlas->bytes.size + atlas->sheet.count * sizeof(AbSpriteRegion) +
                  atlas->image.rgba.size;
    const size_t limit = 128 * 1024 * 1024;
    if (used > limit - scene->asset_bytes) {
        return ab_fail(error, "Scene exceeds the preview asset memory limit");
    }
    scene->asset_bytes += used;
    return true;
}

static bool scene_item(FILE *file, const AbZip *zip, PreviewScene *scene,
                       AbSceneSprite *item, AbError *error) {
    char path[1024], name[512];
    uint32_t values[8];
    if (!scene_token(file, path, sizeof(path), error) ||
        !scene_token(file, name, sizeof(name), error) ||
        !scene_layer(file, &item->layer, error) ||
        !scene_word(file, 10, UINT32_MAX, &item->collider, error) ||
        !scene_word(file, 10, 1, values, error) ||
        !scene_word(file, 10, 1, values + 1, error) ||
        !scene_word(file, 10, AB_BLEND_MULTIPLY, values + 2, error)) {
        return false;
    }
    for (size_t field = 3; field < 8; ++field) {
        if (!scene_word(file, 16, UINT32_MAX, values + field, error)) {
            return false;
        }
    }
    size_t atlas = 0;
    if (!scene_atlas(zip, path, scene, &atlas, error)) {
        return false;
    }
    PreviewAtlas *source = scene->atlases + atlas;
    item->region =
        ab_sprite_find(&source->sheet, (AbSpan){(const uint8_t *)name, strlen(name)});
    if (!item->region) {
        return ab_fail(error, "Scene sprite is absent from its selected atlas");
    }
    item->texture = (uint32_t)atlas + 1;
    item->texture_width = source->image.width;
    item->texture_height = source->image.height;
    item->visible = values[0] != 0;
    item->special_animation = values[1] != 0;
    item->blend = (AbBlend)values[2];
    item->pose = (AbObjectSpritePose){{scene_float(values[3]), scene_float(values[4])},
                                      scene_float(values[5]),
                                      {scene_float(values[6]), scene_float(values[7])}};
    return true;
}

static bool scene_read(const char *path, const AbZip *zip, PreviewScene *output,
                       AbError *error) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return ab_fail(error, "Cannot open scene preview input");
    }
    bool okay = false;
    char header[32];
    uint32_t count, camera[4];
    if (!scene_token(file, header, sizeof(header), error) ||
        strcmp(header, "AB_SPRITE_SCENE_V1") != 0 ||
        !scene_word(file, 10, SCENE_ITEM_LIMIT, &count, error) || !count) {
        ab_fail(error, "Invalid scene preview header or item count");
        goto close_scene;
    }
    for (size_t field = 0; field < 4; ++field) {
        if (!scene_word(file, 16, UINT32_MAX, camera + field, error)) {
            goto close_scene;
        }
    }
    output->camera =
        (AbCoordinateCamera){{scene_float(camera[0]), scene_float(camera[1])},
                             {scene_float(camera[2]), scene_float(camera[3])}};
    output->count = count;
    output->items = calloc(count, sizeof(*output->items));
    output->order = calloc(count, sizeof(*output->order));
    output->atlases = calloc(SCENE_ATLAS_LIMIT, sizeof(*output->atlases));
    if (!output->items || !output->order || !output->atlases) {
        ab_fail(error, "Cannot allocate bounded scene preview storage");
        goto close_scene;
    }
    for (size_t index = 0; index < count; ++index) {
        if (!scene_item(file, zip, output, output->items + index, error)) {
            goto close_scene;
        }
    }
    int character;
    do {
        character = fgetc(file);
    } while (character != EOF && isspace((unsigned char)character));
    if (character != EOF || ferror(file)) {
        ab_fail(error, "Trailing data in scene preview input");
        goto close_scene;
    }
    okay = ab_sprite_scene_order(output->items, output->count, output->order,
                                 output->count, error);

close_scene:
    if (fclose(file)) {
        return ab_fail(error, "Cannot close scene preview input");
    }
    return okay;
}

static bool scene_upload(PreviewScene *scene, AbCommonRenderer *renderer,
                         AbError *error) {
    for (size_t index = 0; index < scene->atlas_count; ++index) {
        if (!ab_common_upload(renderer, (uint32_t)index + 1,
                              &scene->atlases[index].image, false, false, error)) {
            return false;
        }
        ab_image_free(&scene->atlases[index].image);
    }
    return true;
}

static void scene_free(PreviewScene *scene) {
    for (size_t index = 0; index < scene->atlas_count; ++index) {
        PreviewAtlas *atlas = scene->atlases + index;
        ab_image_free(&atlas->image);
        ab_sprite_sheet_free(&atlas->sheet);
        ab_buffer_free(&atlas->bytes);
    }
    free(scene->atlases);
    free(scene->items);
    free(scene->order);
    *scene = (PreviewScene){0};
}

#endif
