#include "ab/sprite_atlas.h"
#include "ab/webp.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    const AbSpriteSheet *sheet;
    size_t image_index;
} SheetImage;

typedef struct {
    AbSpan path;
    AbImage image;
    AbSpriteAlpha alpha;
} AtlasImage;

struct AbSpriteAtlas {
    const AbSpriteSet *sprites;
    SheetImage *sheets;
    AtlasImage *images;
    size_t sheet_count;
    size_t image_count;
    size_t bytes;
};

static const AbZipEntry *find_image(const AbZip *archive, AbSpriteSheetView view,
                                    AbError *error) {
    AbSpan texture = view.sheet->texture;
    if (!texture.size || !texture.data || memchr(texture.data, 0, texture.size) ||
        memchr(texture.data, '/', texture.size) ||
        memchr(texture.data, '\\', texture.size) ||
        (texture.size == 1 && texture.data[0] == '.') ||
        (texture.size == 2 && memcmp(texture.data, "..", 2) == 0)) {
        ab_fail(error, "Sprite texture reference must be a basename");
        return NULL;
    }
    size_t prefix = view.metadata_path.size;
    while (prefix && view.metadata_path.data[prefix - 1] != '/') {
        --prefix;
    }
    if (texture.size > SIZE_MAX - prefix) {
        ab_fail(error, "Sprite texture path exceeds bounds");
        return NULL;
    }
    for (size_t index = 0; index < archive->count; ++index) {
        const AbZipEntry *entry = archive->entries + index;
        if (entry->name.size == prefix + texture.size &&
            (!prefix ||
             memcmp(entry->name.data, view.metadata_path.data, prefix) == 0) &&
            memcmp(entry->name.data + prefix, texture.data, texture.size) == 0) {
            return entry;
        }
    }
    ab_fail(error, "Missing sprite texture archive entry");
    return NULL;
}

static bool decode_image(const AbZip *archive, const AbZipEntry *entry,
                         size_t allocation_limit, size_t entry_limit, AbImage *output,
                         AbSpriteAlpha *alpha, AbError *error) {
    AbSpan name = entry->name;
    bool png = name.size >= 4 && memcmp(name.data + name.size - 4, ".png", 4) == 0;
    bool pvr = name.size >= 4 && memcmp(name.data + name.size - 4, ".pvr", 4) == 0;
    bool webp = name.size >= 5 && memcmp(name.data + name.size - 5, ".webp", 5) == 0;
    if (!png && !pvr && !webp) {
        return ab_fail(error, "Sprite texture codec requires recovery");
    }
    AbBuffer payload = {0};
    bool okay = ab_zip_read(archive, (size_t)(entry - archive->entries), entry_limit,
                            &payload, error);
    if (okay) {
        AbSpan bytes = {payload.data, payload.size};
        bool present = false;
        bool metadata = false, alpha_known = !png;
        if (png) {
            AbPngInfo image;
            metadata = ab_png_read_info(bytes, &image, error);
            /* All 29 target SPRT PNGs use RGBA8; the original reader selects
             * alpha-bearing surface format 6 even when pixels happen opaque.
             * Other PNG upload-format policies require separate evidence. */
            if (metadata && image.color_type == 6) {
                present = alpha_known = true;
            }
        } else if (pvr) {
            metadata = ab_pvr_has_alpha(bytes, &present, error);
        } else if (webp) {
            AbWebpStill image;
            metadata = ab_webp_still_read(bytes, &image, error);
            if (metadata) {
                present = image.has_alpha;
            }
        }
        bool (*decode)(AbSpan, size_t, AbImage *, AbError *) = png   ? ab_png_decode
                                                               : pvr ? ab_pvr_decode
                                                                     : ab_webp_decode;
        okay = metadata && decode(bytes, allocation_limit, output, error);
        if (okay) {
            *alpha = !alpha_known ? AB_SPRITE_ALPHA_UNKNOWN
                     : present    ? AB_SPRITE_ALPHA_PRESENT
                                  : AB_SPRITE_ALPHA_OPAQUE;
        }
    }
    ab_buffer_free(&payload);
    return okay;
}

bool ab_sprite_atlas_open(const AbZip *archive, const AbSpriteSet *sprites,
                          size_t allocation_limit, size_t entry_limit,
                          AbSpriteAtlas **output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing sprite atlas output");
    }
    *output = NULL;
    size_t count = ab_sprite_set_count(sprites);
    if (!archive || !sprites || allocation_limit < sizeof(AbSpriteAtlas) ||
        count > (allocation_limit - sizeof(AbSpriteAtlas)) /
                    (sizeof(SheetImage) + sizeof(AtlasImage))) {
        return ab_fail(error, "Invalid sprite atlas input or allocation limit");
    }
    AbSpriteAtlas *atlas = calloc(1, sizeof(*atlas));
    if (!atlas) {
        return ab_fail(error, "Cannot allocate sprite atlas owner");
    }
    atlas->sprites = sprites;
    atlas->sheet_count = count;
    atlas->bytes = sizeof(*atlas) + count * (sizeof(SheetImage) + sizeof(AtlasImage));
    if (count) {
        atlas->sheets = calloc(count, sizeof(*atlas->sheets));
        atlas->images = calloc(count, sizeof(*atlas->images));
        if (!atlas->sheets || !atlas->images) {
            ab_fail(error, "Cannot allocate sprite atlas entries");
            goto release_atlas;
        }
    }
    for (size_t index = 0; index < count; ++index) {
        AbSpriteSheetView view;
        if (!ab_sprite_set_sheet(sprites, index, &view)) {
            ab_fail(error, "Cannot read sprite atlas sheet");
            goto release_atlas;
        }
        const AbZipEntry *entry = find_image(archive, view, error);
        if (!entry) {
            goto release_atlas;
        }
        size_t image_index = 0;
        while (image_index < atlas->image_count &&
               atlas->images[image_index].path.data != entry->name.data) {
            ++image_index;
        }
        if (image_index == atlas->image_count) {
            AtlasImage *item = atlas->images + image_index;
            if (!decode_image(archive, entry, allocation_limit - atlas->bytes,
                              entry_limit, &item->image, &item->alpha, error)) {
                goto release_atlas;
            }
            item->path = entry->name;
            atlas->bytes += item->image.rgba.size;
            ++atlas->image_count;
        }
        atlas->sheets[index] = (SheetImage){view.sheet, image_index};
    }
    *output = atlas;
    return true;

release_atlas:
    ab_sprite_atlas_close(atlas);
    return false;
}

void ab_sprite_atlas_close(AbSpriteAtlas *atlas) {
    if (atlas) {
        for (size_t index = 0; index < atlas->image_count; ++index) {
            ab_image_free(&atlas->images[index].image);
        }
        free(atlas->sheets);
        free(atlas->images);
        free(atlas);
    }
}

size_t ab_sprite_atlas_count(const AbSpriteAtlas *atlas) {
    return atlas ? atlas->image_count : 0;
}

size_t ab_sprite_atlas_bytes(const AbSpriteAtlas *atlas) {
    return atlas ? atlas->bytes : 0;
}

bool ab_sprite_atlas_image(const AbSpriteAtlas *atlas, size_t index,
                           AbSpriteAtlasImage *output) {
    if (!atlas || !output || index >= atlas->image_count) {
        return false;
    }
    const AtlasImage *image = atlas->images + index;
    *output = (AbSpriteAtlasImage){
        .path = image->path, .image = &image->image, .alpha = image->alpha};
    return true;
}

static bool atlas_binding(const AbSpriteAtlas *atlas, AbSpriteBinding sprite,
                          AbSpriteAtlasBinding *output) {
    for (size_t index = atlas->sheet_count; index; --index) {
        const SheetImage *sheet = atlas->sheets + index - 1;
        if (sheet->sheet == sprite.sheet) {
            const AtlasImage *image = atlas->images + sheet->image_index;
            *output = (AbSpriteAtlasBinding){sprite, sheet->image_index, &image->image,
                                             image->alpha};
            return true;
        }
    }
    return false;
}

bool ab_sprite_atlas_find(const AbSpriteAtlas *atlas, AbSpan name,
                          AbSpriteAtlasBinding *output) {
    AbSpriteBinding sprite;
    return atlas && output && ab_sprite_set_find(atlas->sprites, name, &sprite) &&
           atlas_binding(atlas, sprite, output);
}

bool ab_sprite_atlas_resolve(const AbSpriteAtlas *atlas, AbSpriteBinding sprite,
                             AbSpriteAtlasBinding *output) {
    if (!atlas || !output || !sprite.sheet || !sprite.region)
        return false;
    for (size_t index = 0; index < atlas->sheet_count; ++index) {
        if (atlas->sheets[index].sheet != sprite.sheet)
            continue;
        for (size_t region = 0; region < sprite.sheet->count; ++region)
            if (sprite.sheet->sprites + region == sprite.region)
                return atlas_binding(atlas, sprite, output);
        return false;
    }
    return false;
}

bool ab_sprite_atlas_find_registered(const AbSpriteAtlas *atlas,
                                     const AbSpriteRegistry *registry, AbSpan name,
                                     AbSpriteAtlasBinding *output) {
    AbSpriteBinding sprite;
    return atlas && output &&
           ab_sprite_set_find_registered(atlas->sprites, registry, name, &sprite) &&
           atlas_binding(atlas, sprite, output);
}
