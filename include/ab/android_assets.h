#ifndef AB_ANDROID_ASSETS_H
#define AB_ANDROID_ASSETS_H

#include "ab/asset_groups.h"
#include "ab/zip.h"

typedef struct AbAndroidAssets AbAndroidAssets;

typedef struct {
    float screen_width, screen_height;
    bool adventure_mode, freemium_china, sugotoku;
    size_t groups, profiles, storage_bytes;
} AbAndroidAssetLimits;

/* Own Android bundle metadata, decoded AssetGroup data, loaded membership and a
 * lazy profile load-list cache. Borrow the archive until close; the supplied
 * 32-byte key is copied only into the cache and cleared there on close. Finite
 * dimensions/mode flags are resolved inputs, fixed for this owner's lifetime.
 * Retained owner/data/array/cache bytes share storage_bytes; codec workspace is
 * separately bounded. No VM, image decoding, GPU work or service calls occur.
 * Output is NULL on failure; every partial allocation is released. */
bool ab_android_assets_open(const AbZip *archive, AbSpan key,
                            AbAndroidAssetLimits limits, AbAndroidAssets **output,
                            AbError *error);
void ab_android_assets_close(AbAndroidAssets *assets);
size_t ab_android_assets_bytes(const AbAndroidAssets *assets);
size_t ab_android_assets_group_count(const AbAndroidAssets *assets);
bool ab_android_assets_group(const AbAndroidAssets *assets, size_t index,
                             AbSpan *output);
bool ab_android_assets_failed(const AbAndroidAssets *assets);

/* Expand bundles, select profiles and query cached load lists before dispatching
 * CREATE/RELEASE_SHEET/COMPOSITES to the host. Resource path spans use the literal
 * APK entry root assets/data/images; join them with ab_asset_resource_path.
 * Preserve authored declarations even if their path is absent. The host owns
 * resource creation/release and must report failures; none are simulated here.
 * Input tables stay immutable during dispatch; loaded group-name bytes must
 * remain valid until their membership is removed or the owner closes. Host call
 * views borrow input/cache data. Serialize calls/close; reentry is rejected.
 * Failure can retain prefix effects and loaded membership; stop/discard a failed
 * owner. Configuration acquisition and requireFile/global aux are separate. */
bool ab_android_assets_manage(AbAndroidAssets *assets, const AbDataTable *requested,
                              bool loading, AbAssetHost host, void *context,
                              AbError *error);

#endif
