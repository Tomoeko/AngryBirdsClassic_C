#include "ab/android_assets.h"
#include "ab/asset_load_lists.h"
#include "ab/asset_profile.h"
#include "ab/target_data.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct AbAndroidAssets {
    AbAndroidAssetLimits limits;
    AbAndroidAssetBundles bundles;
    AbTargetData data;
    const AbDataTable *asset_groups;
    AbAssetLoadLists *lists;
    AbAssetGroups groups;
    size_t bytes;
    bool busy;
};

static AbSpan text(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

bool ab_android_assets_open(const AbZip *archive, AbSpan key,
                            AbAndroidAssetLimits limits, AbAndroidAssets **output,
                            AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing Android asset owner output");
    }
    *output = NULL;
    if (!archive || !archive->input.data || key.size != 32 || !key.data ||
        !isfinite(limits.screen_width) || !isfinite(limits.screen_height) ||
        limits.storage_bytes < sizeof(AbAndroidAssets) ||
        limits.groups >
            (limits.storage_bytes - sizeof(AbAndroidAssets)) / (2 * sizeof(AbSpan))) {
        return ab_fail(error, "Invalid Android asset archive/key/storage bounds");
    }
    AbAndroidAssets *assets = calloc(1, sizeof(*assets));
    if (!assets) {
        return ab_fail(error, "Cannot allocate Android asset owner");
    }
    assets->limits = limits;
    assets->bytes = sizeof(*assets) + limits.groups * 2 * sizeof(AbSpan);
    assets->groups.capacity = assets->groups.scratch_capacity = limits.groups;
    if (limits.groups) {
        assets->groups.loaded = calloc(limits.groups, sizeof(AbSpan));
        assets->groups.scratch = calloc(limits.groups, sizeof(AbSpan));
        if (!assets->groups.loaded || !assets->groups.scratch) {
            ab_fail(error, "Cannot allocate Android loaded group storage");
            goto release_assets;
        }
    }
    if (!ab_target_data_open(archive, "assets/data/scripts/platform/asset_groups.lua",
                             key, limits.storage_bytes - assets->bytes, NULL,
                             &assets->data, error)) {
        goto release_assets;
    }
    assets->bytes += assets->data.bytes.size + assets->data.chunk.allocated_bytes +
                     assets->data.data.allocated_bytes;
    const AbDataValue *groups = ab_data_get(
        assets->data.data.globals,
        (AbDataValue){.kind = AB_DATA_STRING, .string = text("AssetGroup")});
    if (!groups || groups->kind != AB_DATA_TABLE || !groups->table) {
        ab_fail(error, "Android asset membership data has no AssetGroup table");
        goto release_assets;
    }
    assets->asset_groups = groups->table;
    if (!ab_android_asset_bundles(limits.freemium_china, limits.sugotoku,
                                  &assets->bundles) ||
        !ab_asset_load_lists_open(archive, key, limits.profiles,
                                  limits.storage_bytes - assets->bytes, &assets->lists,
                                  error)) {
        goto release_assets;
    }
    *output = assets;
    return true;

release_assets:
    ab_android_assets_close(assets);
    return false;
}

void ab_android_assets_close(AbAndroidAssets *assets) {
    if (assets) {
        ab_asset_load_lists_close(assets->lists);
        ab_target_data_close(&assets->data);
        free(assets->groups.scratch);
        free(assets->groups.loaded);
        free(assets);
    }
}

size_t ab_android_assets_bytes(const AbAndroidAssets *assets) {
    return assets ? assets->bytes + ab_asset_load_lists_bytes(assets->lists) : 0;
}

size_t ab_android_assets_group_count(const AbAndroidAssets *assets) {
    return assets ? assets->groups.count : 0;
}

bool ab_android_assets_group(const AbAndroidAssets *assets, size_t index,
                             AbSpan *output) {
    if (!assets || !output || index >= assets->groups.count) {
        return false;
    }
    *output = assets->groups.loaded[index];
    return true;
}

bool ab_android_assets_failed(const AbAndroidAssets *assets) {
    return assets && assets->groups.failed;
}

typedef struct {
    AbAndroidAssets *assets;
    AbAssetHost host;
    void *context;
} AssetPass;

static bool dispatch_asset(void *context, AbAssetCall *call, AbError *error) {
    AssetPass *pass = context;
    AbAndroidAssets *assets = pass->assets;
    if (call->operation == AB_ASSET_SELECT_PROFILE) {
        AbAssetSplashProfile splash = {0};
        if (call->group.size == 8 && memcmp(call->group.data, "SPLASHES", 8) == 0 &&
            !ab_android_splash_profile(assets->limits.screen_width,
                                       assets->limits.screen_height, &splash, error)) {
            return false;
        }
        AbAssetProfileSource source = {.asset_groups = assets->asset_groups,
                                       .screen_width = assets->limits.screen_width,
                                       .adventure_mode = assets->limits.adventure_mode,
                                       .splash_profile = splash.profile};
        return ab_android_asset_profile(&source, call->group, &call->profile, error);
    }
    if (call->operation == AB_ASSET_LOAD_LIST) {
        return ab_asset_load_lists_get(assets->lists, call->profile, &call->groups,
                                       error);
    }
    if (call->operation < AB_ASSET_CREATE_SHEET ||
        call->operation > AB_ASSET_RELEASE_COMPOSITES) {
        return ab_fail(error, "Unexpected Android asset host operation");
    }
    return pass->host(pass->context, call, error);
}

bool ab_android_assets_manage(AbAndroidAssets *assets, const AbDataTable *requested,
                              bool loading, AbAssetHost host, void *context,
                              AbError *error) {
    if (!assets || assets->busy || !host) {
        return ab_fail(error, "Invalid/reentrant Android asset management call");
    }
    AssetPass pass = {assets, host, context};
    assets->busy = true;
    bool okay = ab_asset_group_manage_many(
        &assets->groups, requested, &assets->bundles.profiles, loading,
        text("assets/data/images"), dispatch_asset, &pass, error);
    assets->busy = false;
    return okay;
}
