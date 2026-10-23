#include "ab/asset_textures.h"
#include "ab/sprite_resource.h"

#include <stdlib.h>

typedef struct {
    const AbResourceSource *source;
    const AbImage *image;
    size_t image_index;
    bool wanted, uploaded;
    bool repeat_s, repeat_t;
} TextureSlot;

struct AbAssetTextures {
    AbAssetTextureHost host;
    TextureSlot *slots;
    size_t capacity, count, base_bytes, limit;
    bool busy, failed;
};

bool ab_asset_textures_open(AbAssetTextureHost host, size_t capacity,
                            size_t allocation_limit, AbAssetTextures **output,
                            AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing asset texture cache output");
    }
    *output = NULL;
    if (!host.upload || !host.remove || !capacity || capacity > UINT32_MAX ||
        allocation_limit < sizeof(AbAssetTextures) ||
        capacity > (allocation_limit - sizeof(AbAssetTextures)) / sizeof(TextureSlot)) {
        return ab_fail(error, "Invalid asset texture capacity/host/storage limit");
    }
    AbAssetTextures *textures = calloc(1, sizeof(*textures));
    if (!textures) {
        return ab_fail(error, "Cannot allocate asset texture cache");
    }
    textures->slots = calloc(capacity, sizeof(*textures->slots));
    if (!textures->slots) {
        free(textures);
        return ab_fail(error, "Cannot allocate asset texture slots");
    }
    textures->host = host;
    textures->capacity = capacity;
    textures->base_bytes = sizeof(*textures) + capacity * sizeof(*textures->slots);
    textures->limit = allocation_limit;
    *output = textures;
    return true;
}

static bool first_source(const AbResourceOwners *owners, size_t index) {
    for (size_t previous = 0; previous < index; ++previous) {
        if (owners->entries[previous].used &&
            owners->entries[previous].source == owners->entries[index].source) {
            return false;
        }
    }
    return true;
}

static bool check_sources(const AbAssetTextures *textures,
                          const AbResourceOwners *owners, AbError *error) {
    if (!owners || owners->count > owners->capacity ||
        (owners->capacity && !owners->entries) || owners->busy || owners->failed) {
        return ab_fail(error, "Invalid or failed asset texture source owners");
    }
    size_t images = 0, bytes = textures->base_bytes, sources = 0;
    for (size_t index = 0; index < owners->capacity; ++index) {
        const AbResourceOwnerEntry *entry = owners->entries + index;
        if (!entry->used) {
            continue;
        }
        ++sources;
        if (!entry->source) {
            return ab_fail(error, "Missing asset texture source descriptor");
        }
        if (entry->source->kind == AB_SPRITE_REGISTRY_COMPOSITE ||
            !first_source(owners, index)) {
            continue;
        }
        const AbSpriteResource *resource =
            ab_sprite_resource_from_source(entry->source);
        if (!resource || !entry->source->retain || !entry->source->release) {
            return ab_fail(error, "Unsupported asset texture source provider");
        }
        const AbSpriteAtlas *atlas = ab_sprite_resource_atlas(resource);
        size_t count = ab_sprite_atlas_count(atlas);
        if (count > textures->capacity - images) {
            return ab_fail(error, "Loaded asset images exceed texture slot capacity");
        }
        images += count;
        if (count) {
            size_t retained = ab_sprite_resource_bytes(resource);
            if (retained > textures->limit - bytes) {
                return ab_fail(error,
                               "Retained asset images exceed texture storage limit");
            }
            bytes += retained;
        }
    }
    return sources == owners->count ||
           ab_fail(error, "Asset texture source-map count mismatch");
}

static TextureSlot *find_slot(AbAssetTextures *textures, const AbResourceSource *source,
                              size_t image_index) {
    for (size_t index = 0; index < textures->capacity; ++index) {
        TextureSlot *slot = textures->slots + index;
        if (slot->source == source && slot->image_index == image_index) {
            return slot;
        }
    }
    return NULL;
}

static bool visit_images(AbAssetTextures *textures, const AbResourceOwners *owners,
                         bool upload, AbError *error) {
    for (size_t owner = 0; owner < owners->capacity; ++owner) {
        const AbResourceOwnerEntry *entry = owners->entries + owner;
        if (!entry->used || entry->source->kind != AB_SPRITE_REGISTRY_SPRITE ||
            !first_source(owners, owner)) {
            continue;
        }
        const AbSpriteResource *resource =
            ab_sprite_resource_from_source(entry->source);
        const AbSpriteAtlas *atlas = ab_sprite_resource_atlas(resource);
        size_t count = ab_sprite_atlas_count(atlas);
        for (size_t index = 0; index < count; ++index) {
            AbSpriteAtlasImage image;
            if (!ab_sprite_atlas_image(atlas, index, &image)) {
                return ab_fail(error, "Cannot resolve asset texture image view");
            }
            TextureSlot *slot = find_slot(textures, entry->source, index);
            if (slot) {
                if (slot->image != image.image) {
                    return ab_fail(error,
                                   "Loaded asset texture image identity changed");
                }
                slot->wanted = true;
            } else if (upload) {
                size_t free_slot = 0;
                while (free_slot < textures->capacity &&
                       textures->slots[free_slot].source) {
                    ++free_slot;
                }
                if (free_slot == textures->capacity) {
                    return ab_fail(error,
                                   "No available asset texture slot after checks");
                }
                entry->source->retain(entry->source);
                if (!textures->host.upload(textures->host.context,
                                           (uint32_t)free_slot + 1, image, error)) {
                    entry->source->release(entry->source);
                    return false;
                }
                textures->slots[free_slot] = (TextureSlot){.source = entry->source,
                                                           .image = image.image,
                                                           .image_index = index,
                                                           .wanted = true,
                                                           .uploaded = true};
                ++textures->count;
            }
        }
    }
    return true;
}

static bool remove_slots(AbAssetTextures *textures, bool all, AbError *error) {
    bool okay = true;
    for (size_t index = 0; index < textures->capacity; ++index) {
        TextureSlot *slot = textures->slots + index;
        if (!slot->source || (!all && slot->wanted)) {
            continue;
        }
        if (slot->uploaded && !textures->host.remove(textures->host.context,
                                                     (uint32_t)index + 1, error)) {
            okay = false;
            continue;
        }
        slot->source->release(slot->source);
        *slot = (TextureSlot){0};
        --textures->count;
    }
    return okay;
}

bool ab_asset_textures_sync(AbAssetTextures *textures, const AbResourceOwners *owners,
                            AbError *error) {
    if (!textures || textures->busy || textures->failed) {
        return ab_fail(error, "Missing, busy or failed asset texture cache");
    }
    textures->busy = true;
    bool okay = check_sources(textures, owners, error);
    if (okay) {
        for (size_t index = 0; index < textures->capacity; ++index) {
            textures->slots[index].wanted = false;
        }
        okay = visit_images(textures, owners, false, error) &&
               remove_slots(textures, false, error) &&
               visit_images(textures, owners, true, error);
    }
    textures->failed = !okay;
    textures->busy = false;
    return okay;
}

bool ab_asset_textures_find(const AbAssetTextures *textures, const AbImage *image,
                            uint32_t *texture) {
    if (!textures || textures->busy || textures->failed || !image || !texture) {
        return false;
    }
    for (size_t index = 0; index < textures->capacity; ++index) {
        const TextureSlot *slot = textures->slots + index;
        if (slot->source && slot->image == image) {
            *texture = (uint32_t)index + 1;
            return true;
        }
    }
    return false;
}

bool ab_asset_textures_set_wrap(AbAssetTextures *textures, const AbImage *image,
                                bool repeat_s, bool repeat_t, AbError *error) {
    uint32_t texture;
    if (!ab_asset_textures_find(textures, image, &texture))
        return ab_fail(error, "Missing cached image for texture sampling change");
    if ((repeat_s || repeat_t) &&
        (((image->width - 1) & image->width) || ((image->height - 1) & image->height)))
        return ab_fail(error, "Core GLES2 terrain repeat requires power-of-two pixels");
    TextureSlot *slot = textures->slots + texture - 1;
    if (slot->repeat_s == repeat_s && slot->repeat_t == repeat_t)
        return true;
    const AbSpriteResource *resource = ab_sprite_resource_from_source(slot->source);
    AbSpriteAtlasImage view;
    if (!ab_sprite_atlas_image(ab_sprite_resource_atlas(resource), slot->image_index,
                               &view) ||
        view.image != image)
        return ab_fail(error, "Changed cached image during texture sampling change");
    view.repeat_s = repeat_s;
    view.repeat_t = repeat_t;
    textures->busy = true;
    bool okay = textures->host.remove(textures->host.context, texture, error);
    if (okay) {
        slot->uploaded = false;
        okay = textures->host.upload(textures->host.context, texture, view, error);
        slot->uploaded = okay;
    }
    textures->busy = false;
    textures->failed = !okay;
    if (okay) {
        slot->repeat_s = repeat_s;
        slot->repeat_t = repeat_t;
    }
    return okay;
}

size_t ab_asset_textures_count(const AbAssetTextures *textures) {
    return textures ? textures->count : 0;
}

size_t ab_asset_textures_bytes(const AbAssetTextures *textures) {
    if (!textures) {
        return 0;
    }
    size_t bytes = textures->base_bytes;
    for (size_t index = 0; index < textures->capacity; ++index) {
        const AbResourceSource *source = textures->slots[index].source;
        if (!source) {
            continue;
        }
        size_t previous = 0;
        while (previous < index && textures->slots[previous].source != source) {
            ++previous;
        }
        if (previous == index) {
            bytes += ab_sprite_resource_bytes(ab_sprite_resource_from_source(source));
        }
    }
    return bytes;
}

bool ab_asset_textures_failed(const AbAssetTextures *textures) {
    return textures && textures->failed;
}

bool ab_asset_textures_clear(AbAssetTextures *textures, AbError *error) {
    if (!textures || textures->busy) {
        return ab_fail(error, "Missing or busy asset texture cache clear");
    }
    textures->busy = true;
    bool okay = remove_slots(textures, true, error);
    textures->failed = !okay;
    textures->busy = false;
    return okay;
}

bool ab_asset_textures_close(AbAssetTextures **textures, AbError *error) {
    if (!textures || !*textures) {
        return true;
    }
    if (!ab_asset_textures_clear(*textures, error)) {
        return false;
    }
    free((*textures)->slots);
    free(*textures);
    *textures = NULL;
    return true;
}
