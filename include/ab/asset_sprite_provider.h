#ifndef AB_ASSET_SPRITE_PROVIDER_H
#define AB_ASSET_SPRITE_PROVIDER_H

#include "ab/asset_resources.h"
#include "ab/asset_textures.h"
#include "ab/sprite_provider.h"

typedef struct {
    const AbAssetResources *resources;
    const AbAssetTextures *textures;
    AbSpriteProvider provider;
} AbAssetSpriteProvider;

/* Initialize a caller-owned adapter over the active resource registry and a
 * synchronized texture cache. Keep the adapter at this address and immutable
 * while borrowing its provider. Resources/cache outlive every borrowed binding.
 * Serialized group mutations require texture sync and explicit visual rebinds
 * before rendering. No ownership, allocation or GPU calls; failure preserves
 * output. An empty healthy resource/cache pair is valid. */
bool ab_asset_sprite_provider_init(const AbAssetResources *resources,
                                   const AbAssetTextures *textures,
                                   AbAssetSpriteProvider *output, AbError *error);

/* Construction-time resource dimensions for AbClassicLevelAssets or
 * AbLevelBodySource. Context points to a stable initialized adapter; no texture
 * query occurs. Resolve the active ordinary/composite bounds. Missing names
 * return zero dimensions, matching Resources getSpriteWidth/getSpriteHeight;
 * this allows the original zero-radius circle path. Invalid/failed resources
 * preserve output and fail. Geometry validation belongs to object construction. */
bool ab_asset_sprite_provider_size(void *context, AbSpan name, AbVec2 *output,
                                   AbError *error);

#endif
