#include "ab/asset_sprite_provider.h"

static bool sprite(void *context, AbSpan name, AbSpriteAtlasBinding *output) {
    const AbAssetSpriteProvider *adapter = context;
    return ab_asset_resources_find_sprite(adapter->resources, name, output);
}

static bool composite(void *context, AbSpan name, AbResolvedComposite *output) {
    const AbAssetSpriteProvider *adapter = context;
    return ab_asset_resources_find_composite(adapter->resources, name, output);
}

static bool texture(void *context, const AbImage *image, uint32_t *output) {
    const AbAssetSpriteProvider *adapter = context;
    return !ab_asset_resources_failed(adapter->resources) &&
           ab_asset_textures_find(adapter->textures, image, output);
}

static bool binding(void *context, AbSpriteBinding sprite,
                    AbSpriteAtlasBinding *output) {
    const AbAssetSpriteProvider *adapter = context;
    return ab_asset_resources_resolve_sprite(adapter->resources, sprite, output);
}

bool ab_asset_sprite_provider_init(const AbAssetResources *resources,
                                   const AbAssetTextures *textures,
                                   AbAssetSpriteProvider *output, AbError *error) {
    if (!resources || !textures || !output || ab_asset_resources_failed(resources) ||
        ab_asset_textures_failed(textures)) {
        return ab_fail(error, "Invalid asset sprite provider resources");
    }
    *output = (AbAssetSpriteProvider){
        resources, textures, {output, sprite, composite, texture, binding}};
    return true;
}

bool ab_asset_sprite_provider_size(void *context, AbSpan name, AbVec2 *output,
                                   AbError *error) {
    const AbAssetSpriteProvider *adapter = context;
    if (!adapter || !adapter->resources || !output || (name.size && !name.data) ||
        ab_asset_resources_failed(adapter->resources)) {
        return ab_fail(error, "Invalid classic sprite dimension request");
    }
    AbSpriteAtlasBinding binding;
    AbResolvedComposite composite;
    AbVec2 size = {0};
    if (ab_asset_resources_find_sprite(adapter->resources, name, &binding)) {
        size = (AbVec2){binding.sprite.region->width, binding.sprite.region->height};
    } else if (ab_asset_resources_find_composite(adapter->resources, name,
                                                 &composite)) {
        size = (AbVec2){(float)composite.bounds.width, (float)composite.bounds.height};
    }
    *output = size;
    return true;
}
