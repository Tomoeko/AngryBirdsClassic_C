#ifndef AB_ASSET_TEXTURES_H
#define AB_ASSET_TEXTURES_H

#include "ab/resource_owners.h"
#include "ab/sprite_atlas.h"

typedef struct AbAssetTextures AbAssetTextures;

typedef struct {
    void *context;
    /* Upload copies pixels before returning. Failure leaves that empty texture
     * slot empty. The host chooses its explicit sampler policy using the view.
     * Remove keeps the slot intact on failure. Neither callback may mutate
     * resource owners, unload source images or reenter this cache. */
    bool (*upload)(void *context, uint32_t texture, AbSpriteAtlasImage image,
                   AbError *error);
    bool (*remove)(void *context, uint32_t texture, AbError *error);
} AbAssetTextureHost;

/* Own fixed texture slots and strong source references. IDs are one through
 * capacity and remain stable while an image stays loaded. allocation_limit
 * bounds cache storage plus uniquely retained CPU source storage; host GPU
 * storage is separately bounded by its own capacity. No source decoding occurs.
 * The host/render thread outlives the cache. Output starts NULL. */
bool ab_asset_textures_open(AbAssetTextureHost host, size_t capacity,
                            size_t allocation_limit, AbAssetTextures **output,
                            AbError *error);

/* Synchronize outside a frame after serialized CPU resource-group mutations.
 * Ordinary sources must be first-party sprite resources; composite sources do
 * not own images. Share images for aliased source-map entries, remove stale
 * textures before new uploads, and retain sources through texture removal.
 * Capacity/storage checks precede callbacks. Later host failure keeps prefix
 * effects and marks failed; clear/close before reuse. No allocations here. */
bool ab_asset_textures_sync(AbAssetTextures *textures, const AbResourceOwners *owners,
                            AbError *error);

/* Look up a borrowed image after a successful sync and before another CPU owner
 * mutation. No allocation, decoding or GPU calls. Failure preserves output.
 * The image pointer may be queried only while its CPU source remains loaded. */
bool ab_asset_textures_find(const AbAssetTextures *textures, const AbImage *image,
                            uint32_t *texture);
/* Change a cached image's sampling outside frames. Reupload only on a policy
 * change, retain its texture ID/source identity, and perform no decoding. Core
 * GLES2 repeat requires power-of-two dimensions. Failure stops this owner. */
bool ab_asset_textures_set_wrap(AbAssetTextures *textures, const AbImage *image,
                                bool repeat_s, bool repeat_t, AbError *error);
size_t ab_asset_textures_count(const AbAssetTextures *textures);
size_t ab_asset_textures_bytes(const AbAssetTextures *textures);
bool ab_asset_textures_failed(const AbAssetTextures *textures);

/* Release in texture-ID order outside a frame. A failed host removal preserves
 * that slot/reference while other removals may succeed; retry is allowed.
 * Successful clear resets failed state. Close frees the cache and sets its
 * handle NULL only after every removal succeeds. Empty close succeeds. */
bool ab_asset_textures_clear(AbAssetTextures *textures, AbError *error);
bool ab_asset_textures_close(AbAssetTextures **textures, AbError *error);

#endif
