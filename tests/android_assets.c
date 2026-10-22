#include "ab/android_assets.h"
#include "ab/target_resources.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Android assets check failed at %d: %s\n", __LINE__,       \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue number(unsigned index) {
    float value = (float)index;
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(result.bits));
    return result;
}

static bool equal(AbSpan value, const char *other) {
    return value.size == strlen(other) &&
           (!value.size || memcmp(value.data, other, value.size) == 0);
}

typedef struct {
    AbAndroidAssets *assets;
    const AbZip *archive;
    const AbDataTable *requested;
    uint32_t hashes[256];
    AbAssetOperation kinds[256];
    size_t count, expected, fail_at;
    unsigned phase;
    bool reentry_checked;
} Host;

static bool observe(void *context, AbAssetCall *call, AbError *error) {
    Host *host = context;
    CHECK(call->operation >= AB_ASSET_CREATE_SHEET &&
          call->operation <= AB_ASSET_RELEASE_COMPOSITES && host->count < 256);
    if (!host->reentry_checked) {
        host->reentry_checked = true;
        CHECK(!ab_android_assets_manage(host->assets, host->requested, true, observe,
                                        host, NULL) &&
              !ab_android_assets_failed(host->assets));
    }
    char path[512];
    size_t length;
    CHECK(ab_asset_resource_path(call, path, sizeof(path), &length, error));
    if (!ab_zip_find(host->archive, path)) {
        return ab_fail(error, "Selected common resource is absent: %s", path);
    }
    uint32_t hash = UINT32_C(2166136261);
    for (size_t byte = 0; byte < length; ++byte) {
        hash = (hash ^ (uint8_t)path[byte]) * UINT32_C(16777619);
    }
    AbAssetOperation kind = call->operation;
    if (host->phase == 2) {
        CHECK(kind == AB_ASSET_RELEASE_SHEET || kind == AB_ASSET_RELEASE_COMPOSITES);
        kind = (AbAssetOperation)((unsigned)kind - 1);
    } else {
        CHECK(kind == AB_ASSET_CREATE_SHEET || kind == AB_ASSET_CREATE_COMPOSITES);
    }
    if (!host->phase) {
        host->hashes[host->count] = hash;
        host->kinds[host->count] = kind;
    } else {
        CHECK(host->count < host->expected && host->hashes[host->count] == hash &&
              host->kinds[host->count] == kind);
    }
    size_t index = host->count++;
    return index != host->fail_at ||
           ab_fail(error, "Controlled asset resource failure");
}

static bool edge_checks(void) {
    const uint8_t zip_bytes[22] = {'P', 'K', 5, 6};
    uint8_t key[32] = {0};
    AbZip archive = {0};
    AbAndroidAssets *assets = NULL;
    AbAndroidAssetLimits limits = {480, 320, false, false, false, 16, 8, 1024 * 1024};
    CHECK(ab_zip_open((AbSpan){zip_bytes, sizeof(zip_bytes)}, &archive, NULL));
    CHECK(!ab_android_assets_open(&archive, (AbSpan){key, 31}, limits, &assets, NULL) &&
          !assets);
    limits.groups = SIZE_MAX;
    CHECK(!ab_android_assets_open(&archive, (AbSpan){key, 32}, limits, &assets, NULL) &&
          !assets);
    limits.groups = 16;
    limits.screen_width = NAN;
    CHECK(!ab_android_assets_open(&archive, (AbSpan){key, 32}, limits, &assets, NULL) &&
          !assets);
    limits.screen_width = 480;
    CHECK(!ab_android_assets_open(&archive, (AbSpan){key, 32}, limits, &assets, NULL) &&
          !assets && !ab_android_assets_bytes(NULL) &&
          !ab_android_assets_group_count(NULL) && !ab_android_assets_failed(NULL));
    ab_android_assets_close(NULL);
    ab_zip_close(&archive);
    return true;
}

static bool plan_checks(const AbZip *archive, AbSpan key, float width, float height) {
    static const char *const requests[] = {"INGAME",   "INGAME_PIGS_NEW", "HUD",
                                           "THEME_01", "NEW_SHOP",        "SPLASHES"};
    static const char *const loaded[] = {
        "INGAME",   "INGAME_PIGS_NEW",       "HUD",     "THEME_01",
        "NEW_SHOP", "NEW_SHOP_SALE_BANNERS", "SPLASHES"};
    AbDataEntry entries[6];
    for (unsigned index = 0; index < 6; ++index) {
        entries[index] = (AbDataEntry){number(index + 1), text(requests[index])};
    }
    AbDataTable requested = {entries, 6, 6, NULL};
    AbAndroidAssets *assets = NULL;
    AbAndroidAssetLimits limits = {width, height, false, false,
                                   false, 16,     8,     1024 * 1024};
    CHECK(ab_android_assets_open(archive, key, limits, &assets, NULL) &&
          !ab_android_assets_group_count(assets) && !ab_android_assets_failed(assets));
    size_t baseline = ab_android_assets_bytes(assets);
    Host host = {.assets = assets,
                 .archive = archive,
                 .requested = &requested,
                 .fail_at = SIZE_MAX};
    CHECK(ab_android_assets_manage(assets, &requested, true, observe, &host, NULL) &&
          ab_android_assets_group_count(assets) == 7 && host.count);
    for (size_t index = 0; index < 7; ++index) {
        AbSpan group = {0};
        CHECK(ab_android_assets_group(assets, index, &group) &&
              equal(group, loaded[index]));
    }
    size_t retained = ab_android_assets_bytes(assets);
    CHECK(retained > baseline && retained <= limits.storage_bytes);
    host.expected = host.count;
    host.count = 0;
    host.phase = 1;
    CHECK(ab_android_assets_manage(assets, &requested, true, observe, &host, NULL) &&
          host.count == host.expected && ab_android_assets_group_count(assets) == 7 &&
          ab_android_assets_bytes(assets) == retained);
    host.count = 0;
    host.phase = 2;
    CHECK(ab_android_assets_manage(assets, &requested, false, observe, &host, NULL) &&
          host.count == host.expected && !ab_android_assets_group_count(assets) &&
          ab_android_assets_bytes(assets) == retained);
    AbSpan sentinel = text("unchanged").string;
    CHECK(!ab_android_assets_group(assets, 0, &sentinel) &&
          equal(sentinel, "unchanged"));
    printf("Android %.0fx%.0f: six requests expand to seven loaded groups, %zu "
           "packaged resource plans; repeated loads/releases agree, retaining %zu "
           "bytes.\n",
           (double)width, (double)height, host.expected, retained);
    ab_android_assets_close(assets);
    assets = NULL;
    CHECK(ab_android_assets_open(archive, key, limits, &assets, NULL));
    host = (Host){
        .assets = assets, .archive = archive, .requested = &requested, .fail_at = 1};
    AbError error = {0};
    CHECK(!ab_android_assets_manage(assets, &requested, true, observe, &host, &error) &&
          ab_android_assets_failed(assets) &&
          ab_android_assets_group_count(assets) == 1 && host.count == 2 &&
          strstr(error.message, "Controlled asset resource failure"));
    CHECK(!ab_android_assets_manage(assets, &requested, true, observe, &host, NULL) &&
          host.count == 2);
    ab_android_assets_close(assets);
    return true;
}

static bool apk_checks(const char *path) {
    AbBuffer bytes = {0};
    AbZip archive = {0};
    uint8_t key[32] = {0};
    AbError error = {0};
    bool okay = ab_file_read(path, 128 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
                ab_target_resource_key(&archive, 16 * 1024 * 1024, key, &error) &&
                plan_checks(&archive, (AbSpan){key, 32}, 480, 320) &&
                plan_checks(&archive, (AbSpan){key, 32}, 1024, 768);
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    memset(key, 0, sizeof(key));
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 3 || strcmp(argv[1], "--apk")) {
        return 2;
    }
    return apk_checks(argv[2]) ? 0 : 1;
}
