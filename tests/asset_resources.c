#include "ab/asset_resources.h"
#include "ab/android_assets.h"
#include "ab/composite_part.h"
#include "ab/target_resources.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Asset resource check failed at %d: %s\n", __LINE__,       \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbSpan text(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static AbDataValue number(unsigned index) {
    float value = (float)index;
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static AbAssetCall sheet_call(AbAssetOperation operation, const char *filename) {
    return (AbAssetCall){.operation = operation,
                         .profile = text("base"),
                         .image_root = text("assets/data/images"),
                         .filename = text(filename)};
}

static bool edge_checks(void) {
    const uint8_t empty_zip[22] = {'P', 'K', 5, 6};
    AbZip archive = {0};
    CHECK(ab_zip_open((AbSpan){empty_zip, sizeof(empty_zip)}, &archive, NULL));
    AbAssetResourceLimits limits = {2, 128, 1024 * 1024, 1024 * 1024};
    AbAssetResources *resources = NULL;
    CHECK(!ab_asset_resources_open(NULL, limits, &resources, NULL) && !resources);
    limits.sources = SIZE_MAX;
    CHECK(!ab_asset_resources_open(&archive, limits, &resources, NULL) && !resources);
    limits.sources = 2;
    limits.storage_bytes = 1;
    CHECK(!ab_asset_resources_open(&archive, limits, &resources, NULL) && !resources);
    limits.storage_bytes = 1024 * 1024;
    CHECK(ab_asset_resources_open(&archive, limits, &resources, NULL));
    size_t baseline = ab_asset_resources_bytes(resources);
    AbAssetCall call = sheet_call(AB_ASSET_RELEASE_SHEET, "missing.dat");
    CHECK(ab_asset_resources_host(resources, &call, NULL) &&
          ab_asset_resources_bytes(resources) == baseline &&
          !ab_asset_resources_owners(resources)->count);
    call.operation = AB_ASSET_SELECT_PROFILE;
    CHECK(!ab_asset_resources_host(resources, &call, NULL) &&
          !ab_asset_resources_failed(resources));
    call.operation = AB_ASSET_CREATE_SHEET;
    CHECK(!ab_asset_resources_host(resources, &call, NULL) &&
          ab_asset_resources_failed(resources) &&
          ab_asset_resources_bytes(resources) == baseline);
    call.operation = AB_ASSET_RELEASE_SHEET;
    CHECK(!ab_asset_resources_host(resources, &call, NULL));
    ab_asset_resources_close(resources);
    CHECK(!ab_asset_resources_bytes(NULL) && !ab_asset_resources_failed(NULL) &&
          !ab_asset_resources_owners(NULL) && !ab_asset_resources_registry(NULL));
    AbSpriteAtlasBinding sprite = {.image_index = 99};
    AbResolvedComposite composite = {.extended = true};
    CHECK(!ab_asset_resources_find_sprite(NULL, text("none"), &sprite) &&
          sprite.image_index == 99 &&
          !ab_asset_resources_find_composite(NULL, text("none"), &composite) &&
          composite.extended);
    ab_asset_resources_close(NULL);
    ab_zip_close(&archive);
    return true;
}

static bool check_bindings(const AbAssetResources *resources) {
    const AbSpriteRegistry *registry = ab_asset_resources_registry(resources);
    const AbResourceOwners *owners = ab_asset_resources_owners(resources);
    size_t sprites = 0, composites = 0, parts = 0;
    for (size_t index = 0; index < registry->count; ++index) {
        const AbSpriteRegistryEntry *entry = registry->entries + index;
        AbSpriteRegistryEntry active;
        CHECK(ab_sprite_registry_find(registry, entry->name, AB_SPRITE_REGISTRY_ANY,
                                      &active));
        if (active.resource != entry->resource || active.kind != entry->kind) {
            continue;
        }
        AbSpriteAtlasBinding sprite;
        AbResolvedComposite composite;
        if (entry->kind == AB_SPRITE_REGISTRY_SPRITE) {
            CHECK(
                ab_asset_resources_find_sprite(resources, entry->name, &sprite) &&
                sprite.sprite.region == entry->resource && sprite.image &&
                sprite.image->rgba.size &&
                !ab_asset_resources_find_composite(resources, entry->name, &composite));
            ++sprites;
        } else {
            CHECK(
                ab_asset_resources_find_composite(resources, entry->name, &composite) &&
                composite.source &&
                !ab_asset_resources_find_sprite(resources, entry->name, &sprite));
            for (size_t part = 0; part < composite.source->part_count; ++part) {
                const AbCompositePart *definition = composite.source->parts + part;
                AbResourceMemberView selected;
                AbCompositeBoundsPart state;
                CHECK(ab_resource_owners_find_member(owners, AB_SPRITE_REGISTRY_SPRITE,
                                                     definition->sprite, &selected) &&
                      composite.parts[part].region == selected.member->resource &&
                      ab_composite_part_state(definition, composite.extended,
                                              composite.parts[part].region, &state,
                                              NULL));
                ++parts;
            }
            ++composites;
        }
    }
    CHECK(sprites && composites && parts);
    printf("Verified %zu active ordinary bindings, %zu composites and %zu parts.\n",
           sprites, composites, parts);
    return true;
}

static bool managed_checks(const AbZip *archive, AbSpan key, float width,
                           float height) {
    static const char *const groups[] = {"INGAME",
                                         "INGAME_PIGS_NEW",
                                         "HUD",
                                         "THEME_01",
                                         "NEW_SHOP",
                                         "SPLASHES",
                                         "INGAME_COMPOSPRITES",
                                         "THEME_01_COMPOSPRITES"};
    AbDataEntry entries[8];
    for (unsigned index = 0; index < 8; ++index) {
        entries[index] = (AbDataEntry){
            number(index + 1), {.kind = AB_DATA_STRING, .string = text(groups[index])}};
    }
    AbDataTable requested = {entries, 8, 8, NULL};
    AbAndroidAssetLimits planning = {width, height, false, false,
                                     false, 32,     8,     1024 * 1024};
    AbAssetResourceLimits limits = {64, 8192, 128 * 1024 * 1024, 16 * 1024 * 1024};
    AbAndroidAssets *assets = NULL;
    AbAssetResources *resources = NULL;
    AbError error = {0};
    CHECK(ab_android_assets_open(archive, key, planning, &assets, &error) &&
          ab_asset_resources_open(archive, limits, &resources, &error));
    size_t baseline = ab_asset_resources_bytes(resources);
    if (!ab_android_assets_manage(assets, &requested, true, ab_asset_resources_host,
                                  resources, &error)) {
        fprintf(stderr, "%s\n", error.message);
        return false;
    }
    const AbResourceOwners *owners = ab_asset_resources_owners(resources);
    const AbSpriteRegistry *registry = ab_asset_resources_registry(resources);
    CHECK(owners->count && registry->count &&
          ab_android_assets_group_count(assets) == 9 && check_bindings(resources));
    size_t retained = ab_asset_resources_bytes(resources), sources = owners->count;
    CHECK(retained > baseline && retained <= limits.storage_bytes);
    AbSpriteRegistryEntry *snapshot = malloc(registry->count * sizeof(*snapshot));
    CHECK(snapshot);
    size_t snapshot_size = registry->count * sizeof(*snapshot);
    memcpy(snapshot, registry->entries, snapshot_size);
    CHECK(ab_android_assets_manage(assets, &requested, true, ab_asset_resources_host,
                                   resources, &error) &&
          owners->count == sources && ab_asset_resources_bytes(resources) == retained &&
          registry->count * sizeof(*snapshot) == snapshot_size &&
          memcmp(snapshot, registry->entries, snapshot_size) == 0);
    /* An existing key returns its source without opening a different path or
     * re-registering its members, matching the no-option native create calls. */
    AbAssetCall cached = sheet_call(AB_ASSET_CREATE_SHEET, "INGAME_BLOCKS_1.dat");
    cached.image_root = text("missing");
    CHECK(ab_asset_resources_host(resources, &cached, &error) &&
          ab_asset_resources_bytes(resources) == retained &&
          memcmp(snapshot, registry->entries, snapshot_size) == 0);
    free(snapshot);
    CHECK(ab_android_assets_manage(assets, &requested, false, ab_asset_resources_host,
                                   resources, &error) &&
          !owners->count && !registry->count &&
          !ab_android_assets_group_count(assets) &&
          ab_asset_resources_bytes(resources) == baseline);
    CHECK(ab_android_assets_manage(assets, &requested, true, ab_asset_resources_host,
                                   resources, &error) &&
          owners->count == sources && ab_asset_resources_bytes(resources) == retained &&
          check_bindings(resources));
    printf("Managed real %.0fx%.0f Android assets: %zu source maps, %zu registry "
           "entries, %zu retained bytes; reuse, release and reload pass.\n",
           (double)width, (double)height, sources, registry->count, retained);
    ab_asset_resources_close(resources);
    ab_android_assets_close(assets);
    return true;
}

static bool failure_checks(const AbZip *archive) {
    AbAssetResourceLimits limits = {2, 1, 32 * 1024 * 1024, 16 * 1024 * 1024};
    AbAssetResources *resources = NULL;
    CHECK(ab_asset_resources_open(archive, limits, &resources, NULL));
    size_t baseline = ab_asset_resources_bytes(resources);
    AbAssetCall call = sheet_call(AB_ASSET_CREATE_SHEET, "INGAME_BLOCKS_1.dat");
    CHECK(!ab_asset_resources_host(resources, &call, NULL) &&
          ab_asset_resources_failed(resources) &&
          !ab_asset_resources_owners(resources)->count &&
          !ab_asset_resources_registry(resources)->count &&
          ab_asset_resources_bytes(resources) == baseline);
    ab_asset_resources_close(resources);
    limits.sources = 1;
    limits.bindings = 4096;
    CHECK(ab_asset_resources_open(archive, limits, &resources, NULL));
    CHECK(ab_asset_resources_host(resources, &call, NULL));
    size_t retained = ab_asset_resources_bytes(resources);
    call.filename = text("INGAME_BLOCKS_2.dat");
    CHECK(!ab_asset_resources_host(resources, &call, NULL) &&
          ab_asset_resources_failed(resources) &&
          ab_asset_resources_owners(resources)->count == 1 &&
          ab_asset_resources_registry(resources)->count == 247 &&
          ab_asset_resources_bytes(resources) == retained);
    ab_asset_resources_close(resources);
    CHECK(ab_asset_resources_open(archive, limits, &resources, NULL));
    baseline = ab_asset_resources_bytes(resources);
    call.operation = AB_ASSET_CREATE_COMPOSITES;
    call.filename = text("INGAME_COMPOSPRITES.dat");
    CHECK(!ab_asset_resources_host(resources, &call, NULL) &&
          ab_asset_resources_failed(resources) &&
          !ab_asset_resources_owners(resources)->count &&
          ab_asset_resources_bytes(resources) == baseline);
    ab_asset_resources_close(resources);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 3 || strcmp(argv[1], "--apk")) {
        return 2;
    }
    AbBuffer bytes = {0};
    AbZip archive = {0};
    uint8_t key[32] = {0};
    AbError error = {0};
    bool okay = ab_file_read(argv[2], 128 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
                ab_target_resource_key(&archive, 16 * 1024 * 1024, key, &error) &&
                managed_checks(&archive, (AbSpan){key, 32}, 480, 320) &&
                managed_checks(&archive, (AbSpan){key, 32}, 1024, 768) &&
                failure_checks(&archive);
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    memset(key, 0, sizeof(key));
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay ? 0 : 1;
}
