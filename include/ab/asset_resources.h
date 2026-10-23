#ifndef AB_ASSET_RESOURCES_H
#define AB_ASSET_RESOURCES_H

#include "ab/asset_groups.h"
#include "ab/composite_resource.h"
#include "ab/sprite_resource.h"

typedef struct AbAssetResources AbAssetResources;

typedef struct {
    size_t sources, bindings, storage_bytes, entry_bytes;
} AbAssetResourceLimits;

/* Own fixed source-map/registry storage and real decoded SPRT/composite sources.
 * Borrow the archive until close. Retained owner, arrays, metadata and pixels
 * share storage_bytes; temporary ZIP/codec/bounds workspaces are separately
 * bounded, so this is not a peak-memory limit. entry_bytes bounds each payload.
 * No Lua VM, GPU, service or per-frame decoding. Output is NULL on failure.
 * Calls/queries/close must be serialized; retained views expire on mutation. */
bool ab_asset_resources_open(const AbZip *archive, AbAssetResourceLimits limits,
                             AbAssetResources **output, AbError *error);
void ab_asset_resources_close(AbAssetResources *resources);
size_t ab_asset_resources_bytes(const AbAssetResources *resources);
bool ab_asset_resources_failed(const AbAssetResources *resources);
const AbResourceOwners *ab_asset_resources_owners(const AbAssetResources *resources);
const AbSpriteRegistry *ab_asset_resources_registry(const AbAssetResources *resources);

/* Resource host for ab_android_assets_manage: join the literal APK path, derive
 * its native two-pass key, reuse an existing source on create, otherwise decode
 * and register it. Default release erases the source; image-only unload/forced
 * reload options are outside this no-option manageAssets boundary. Composite
 * parts borrow sheets in first-key order and are not retained dependencies.
 * The caller must load their sheets first and avoid using dangling part views
 * during/after sheet release. Missing files/dependencies are errors. Failure
 * retains prior effects and marks the host failed; stop/discard it. Reentry is
 * rejected. SELECT_PROFILE/LOAD_LIST belong to the Android planning owner. */
bool ab_asset_resources_host(void *context, AbAssetCall *call, AbError *error);

/* Resolve active bindings to owned decoded pixels or composite metadata without
 * allocation, decoding or I/O. Ordinary/composite masking follows the registry.
 * Returned views borrow sources and weak dependencies through the next mutation.
 * Missing/invalid queries preserve output. No GPU upload/drawing occurs. */
bool ab_asset_resources_find_sprite(const AbAssetResources *resources, AbSpan name,
                                    AbSpriteAtlasBinding *output);
/* Native getSpriteSheet(name).getImage(), independent of sprite-name masking.
 * Borrow the single decoded sheet image until the next resource mutation. */
bool ab_asset_resources_find_image(const AbAssetResources *resources, AbSpan name,
                                   AbSpriteAtlasImage *output);
bool ab_asset_resources_find_composite(const AbAssetResources *resources, AbSpan name,
                                       AbResolvedComposite *output);
/* Resolve an existing cached part's exact sheet/region into live pixels. This
 * keeps composite selections stable when a newer registry binding masks a name.
 * A released sheet or unloaded image cannot be resolved. No allocation/I/O. */
bool ab_asset_resources_resolve_sprite(const AbAssetResources *resources,
                                       AbSpriteBinding sprite,
                                       AbSpriteAtlasBinding *output);

#endif
