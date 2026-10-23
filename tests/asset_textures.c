#include "ab/asset_resources.h"
#include "ab/asset_textures.h"
#include "ab/android_assets.h"
#include "ab/sprite_resource.h"
#include "ab/target_resources.h"
#if defined(AB_ASSET_TEXTURES_COMMON)
#include "ab/common_renderer.h"
#include "asset_textures_common.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Asset texture check failed at %d: %s\n", __LINE__,        \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    const AbImage *images[32];
    bool repeat_s[32], repeat_t[32];
    size_t uploads, removals, attempts;
    uint32_t fail_upload, fail_remove;
#if defined(AB_ASSET_TEXTURES_COMMON)
    AbCommonRenderer *renderer;
#endif
} Host;

static AbSpan text(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static bool upload(void *context, uint32_t texture, AbSpriteAtlasImage image,
                   AbError *error) {
    Host *host = context;
    ++host->attempts;
    if (texture == host->fail_upload) {
        return ab_fail(error, "Controlled texture upload failure");
    }
    if (!texture || texture > 32 || host->images[texture - 1] || !image.image ||
        !image.image->rgba.data || !image.image->rgba.size || !image.path.size) {
        return ab_fail(error, "Invalid controlled texture upload/view");
    }
#if defined(AB_ASSET_TEXTURES_COMMON)
    if (host->renderer && !ab_common_upload(host->renderer, texture, image.image,
                                            image.repeat_s, image.repeat_t, error)) {
        return false;
    }
#endif
    host->images[texture - 1] = image.image;
    host->repeat_s[texture - 1] = image.repeat_s;
    host->repeat_t[texture - 1] = image.repeat_t;
    ++host->uploads;
    return true;
}

static bool remove_texture(void *context, uint32_t texture, AbError *error) {
    Host *host = context;
    if (texture == host->fail_remove) {
        return ab_fail(error, "Controlled texture removal failure");
    }
    if (!texture || texture > 32 || !host->images[texture - 1]) {
        return ab_fail(error, "Invalid controlled texture removal");
    }
    /* Pixel views expire on CPU image unload. The upload boundary copies data;
     * texture removal must never dereference this old CPU pointer. */
#if defined(AB_ASSET_TEXTURES_COMMON)
    if (host->renderer && !ab_common_unload(host->renderer, texture, error)) {
        return false;
    }
#endif
    host->images[texture - 1] = NULL;
    ++host->removals;
    return true;
}

static AbAssetTextureHost callbacks(Host *host) {
    return (AbAssetTextureHost){host, upload, remove_texture};
}

static bool edge_checks(void) {
    Host host = {0};
    AbAssetTextures *textures = NULL;
    CHECK(!ab_asset_textures_open((AbAssetTextureHost){0}, 1, 4096, &textures, NULL) &&
          !textures &&
          !ab_asset_textures_open(callbacks(&host), SIZE_MAX, 4096, &textures, NULL) &&
          !ab_asset_textures_open(callbacks(&host), 1, 1, &textures, NULL));
    CHECK(ab_asset_textures_open(callbacks(&host), 4, 4096, &textures, NULL));
    size_t baseline = ab_asset_textures_bytes(textures);
    AbResourceOwners owners = {0};
    CHECK(ab_asset_textures_sync(textures, &owners, NULL) &&
          !ab_asset_textures_count(textures) && !host.attempts && !host.removals);
    AbImage image = {0};
    uint32_t texture = 99;
    CHECK(!ab_asset_textures_find(textures, &image, &texture) && texture == 99);
    AbResourceSource foreign = {.kind = AB_SPRITE_REGISTRY_SPRITE};
    AbResourceOwnerEntry entry = {.source = &foreign, .used = true};
    owners = (AbResourceOwners){.entries = &entry, .count = 1, .capacity = 1};
    CHECK(!ab_asset_textures_sync(textures, &owners, NULL) &&
          ab_asset_textures_failed(textures) && !host.attempts &&
          ab_asset_textures_bytes(textures) == baseline);
    CHECK(ab_asset_textures_clear(textures, NULL) &&
          !ab_asset_textures_failed(textures) &&
          ab_asset_textures_close(&textures, NULL) && !textures &&
          ab_asset_textures_close(&textures, NULL));
    return true;
}

static AbDataValue number(unsigned value) {
    float real = (float)value;
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &real, sizeof(real));
    return result;
}

static bool manage(AbAndroidAssets *assets, AbAssetResources *resources,
                   const char *const *groups, size_t count, bool load, AbError *error) {
    AbDataEntry entries[16];
    CHECK(count <= 16);
    for (size_t index = 0; index < count; ++index) {
        entries[index] =
            (AbDataEntry){number((unsigned)index + 1),
                          {.kind = AB_DATA_STRING, .string = text(groups[index])}};
    }
    AbDataTable requested = {entries, count, 16, NULL};
    return ab_android_assets_manage(assets, &requested, load, ab_asset_resources_host,
                                    resources, error);
}

static bool failure_checks(const AbResourceOwners *owners) {
    Host host = {.fail_upload = 3};
    AbAssetTextures *textures = NULL;
    CHECK(ab_asset_textures_open(callbacks(&host), 32, 128 * 1024 * 1024, &textures,
                                 NULL) &&
          !ab_asset_textures_sync(textures, owners, NULL) &&
          ab_asset_textures_failed(textures) &&
          ab_asset_textures_count(textures) == 2 && host.uploads == 2 &&
          host.attempts == 3 && ab_asset_textures_close(&textures, NULL) && !textures &&
          host.removals == 2);
    host = (Host){0};
    CHECK(ab_asset_textures_open(callbacks(&host), 1, 128 * 1024 * 1024, &textures,
                                 NULL) &&
          !ab_asset_textures_sync(textures, owners, NULL) && !host.attempts &&
          ab_asset_textures_close(&textures, NULL));
    CHECK(ab_asset_textures_open(callbacks(&host), 32, 4096, &textures, NULL) &&
          !ab_asset_textures_sync(textures, owners, NULL) && !host.attempts &&
          ab_asset_textures_close(&textures, NULL));
    CHECK(ab_asset_textures_open(callbacks(&host), 32, 128 * 1024 * 1024, &textures,
                                 NULL) &&
          ab_asset_textures_sync(textures, owners, NULL));
    host.fail_remove = 1;
    CHECK(!ab_asset_textures_close(&textures, NULL) && textures &&
          ab_asset_textures_failed(textures) &&
          ab_asset_textures_count(textures) == 1 && host.removals == 21);
    host.fail_remove = 0;
    CHECK(ab_asset_textures_close(&textures, NULL) && !textures && host.removals == 22);
    return true;
}

static bool wrap_checks(const AbResourceOwners *owners) {
    for (size_t scenario = 0; scenario < 3; ++scenario) {
        Host host = {0};
        AbAssetTextures *textures = NULL;
        CHECK(ab_asset_textures_open(callbacks(&host), 32, 128 * 1024 * 1024, &textures,
                                     NULL) &&
              ab_asset_textures_sync(textures, owners, NULL));
        size_t index = 0;
        while (index < 32 &&
               (!host.images[index] ||
                ((host.images[index]->width - 1) & host.images[index]->width) ||
                ((host.images[index]->height - 1) & host.images[index]->height)))
            ++index;
        CHECK(index < 32);
        const AbImage *image = host.images[index];
        uint32_t texture = (uint32_t)index + 1, actual = 0;
        size_t uploads = host.uploads, retained = ab_asset_textures_bytes(textures);
        CHECK(ab_asset_textures_set_wrap(textures, image, false, false, NULL) &&
              host.uploads == uploads && !host.removals);
        if (scenario) {
            host.fail_remove = scenario == 1 ? texture : 0;
            host.fail_upload = scenario == 2 ? texture : 0;
            CHECK(!ab_asset_textures_set_wrap(textures, image, true, true, NULL) &&
                  ab_asset_textures_failed(textures) &&
                  !ab_asset_textures_find(textures, image, &actual));
            host.fail_remove = host.fail_upload = 0;
            CHECK(ab_asset_textures_close(&textures, NULL) && !textures &&
                  host.removals == uploads);
            continue;
        }
        CHECK(ab_asset_textures_set_wrap(textures, image, true, true, NULL) &&
              host.repeat_s[index] && host.repeat_t[index] &&
              host.uploads == uploads + 1 && host.removals == 1 &&
              ab_asset_textures_find(textures, image, &actual) && actual == texture &&
              ab_asset_textures_bytes(textures) == retained &&
              ab_asset_textures_set_wrap(textures, image, true, true, NULL) &&
              ab_asset_textures_sync(textures, owners, NULL) &&
              host.uploads == uploads + 1 && host.removals == 1 &&
              host.repeat_s[index] && host.repeat_t[index]);
        CHECK(ab_asset_textures_set_wrap(textures, image, false, true, NULL) &&
              !host.repeat_s[index] && host.repeat_t[index] &&
              host.uploads == uploads + 2 && host.removals == 2 &&
              ab_asset_textures_close(&textures, NULL) && !textures &&
              host.removals == uploads + 2);
    }
    return true;
}

static bool alias_unload_checks(const AbZip *archive) {
    AbSpriteResource *resource = NULL;
    CHECK(ab_sprite_resource_open(archive,
                                  "assets/data/images/480x320/MENU_GRADIENTS.dat",
                                  32 * 1024 * 1024, 8 * 1024 * 1024, &resource, NULL));
    const AbResourceSource *source = ab_sprite_resource_source(resource);
    AbSpriteRegistryEntry *bindings = calloc(source->count * 2, sizeof(*bindings));
    CHECK(bindings);
    AbSpriteRegistry registry = {.entries = bindings, .capacity = source->count * 2};
    AbResourceOwnerEntry entries[2];
    AbResourceOwners owners;
    CHECK(ab_resource_owners_init(&owners, entries, 2, &registry, NULL) &&
          ab_resource_owners_put(&owners, text("one"), source, NULL) &&
          ab_resource_owners_put(&owners, text("two"), source, NULL));
    Host host = {0};
    AbAssetTextures *textures = NULL;
    CHECK(
        ab_asset_textures_open(callbacks(&host), 2, 32 * 1024 * 1024, &textures, NULL));
    size_t baseline = ab_asset_textures_bytes(textures);
    CHECK(ab_asset_textures_sync(textures, &owners, NULL) && host.uploads == 1 &&
          ab_asset_textures_count(textures) == 1);
    const AbSpriteAtlas *atlas = ab_sprite_resource_atlas(resource);
    AbSpriteAtlasImage image;
    uint32_t texture;
    CHECK(ab_sprite_atlas_image(atlas, 0, &image) &&
          ab_asset_textures_find(textures, image.image, &texture) && texture == 1);
    ab_sprite_resource_close(resource);
    resource = NULL;
    CHECK(ab_resource_owners_remove(&owners, AB_SPRITE_REGISTRY_SPRITE, text("one"),
                                    false, NULL) &&
          ab_asset_textures_sync(textures, &owners, NULL) && host.uploads == 1 &&
          !host.removals);
    CHECK(ab_resource_owners_remove(&owners, AB_SPRITE_REGISTRY_SPRITE, text("two"),
                                    true, NULL) &&
          owners.count == 1 && ab_asset_textures_sync(textures, &owners, NULL) &&
          host.removals == 1 && !ab_asset_textures_count(textures) &&
          ab_asset_textures_bytes(textures) == baseline);
    CHECK(ab_asset_textures_close(&textures, NULL));
    CHECK(ab_resource_owners_clear(&owners, NULL));
    free(bindings);
    return true;
}

static bool managed_checks(const AbZip *archive, AbSpan key) {
    static const char *const groups[15] = {"INGAME",
                                           "INGAME_PIGS_NEW",
                                           "OTHER",
                                           "MENU",
                                           "POPUPS",
                                           "BUTTONS",
                                           "TUTORIALS",
                                           "HINT_POPUP",
                                           "HUD",
                                           "INGAME_COMPOSPRITES",
                                           "BUTTONS_COMPOSPRITES",
                                           "MENU_COMPOSPRITES",
                                           "THEME_01",
                                           "THEME_01_COMPOSPRITES",
                                           "NEW_SHOP_SALE_BANNERS"};
    AbAndroidAssetLimits planning = {480, 320, false, false, false, 64, 8, 1024 * 1024};
    AbAssetResourceLimits limits = {64, 8192, 128 * 1024 * 1024, 16 * 1024 * 1024};
    AbAndroidAssets *assets = NULL;
    AbAssetResources *resources = NULL;
    AbAssetTextures *textures = NULL;
    AbError error = {0};
    Host host = {0};
#if defined(AB_ASSET_TEXTURES_COMMON)
    CcPlatform *platform = NULL;
    AbCommonRenderer *renderer = NULL;
#endif
    bool okay = false;
    if (!ab_android_assets_open(archive, key, planning, &assets, &error) ||
        !ab_asset_resources_open(archive, limits, &resources, &error) ||
        !manage(assets, resources, groups, 15, true, &error) ||
        !ab_asset_textures_open(callbacks(&host), 32, 128 * 1024 * 1024, &textures,
                                &error)) {
        goto release;
    }
    const AbResourceOwners *owners = ab_asset_resources_owners(resources);
#if defined(AB_ASSET_TEXTURES_COMMON)
    platform = cc_platform_create("Angry Birds resource texture validation", 120, 80);
    if (!platform ||
        !ab_common_create(platform, 1, 32, 1024 * 1024, &renderer, &error)) {
        goto release;
    }
    host.renderer = renderer;
#endif
    CHECK(owners->count == 28 && ab_asset_textures_sync(textures, owners, &error) &&
          host.uploads == 22 && ab_asset_textures_count(textures) == 22 &&
          ab_asset_textures_sync(textures, owners, &error) && host.uploads == 22 &&
          !host.removals);
#if defined(AB_ASSET_TEXTURES_COMMON)
    CHECK(asset_texture_gpu_menu(platform, renderer, textures, owners, true, &error));
#endif
    size_t retained = ab_asset_textures_bytes(textures);
    const AbImage *initial[32];
    memcpy(initial, host.images, sizeof(initial));
    uint32_t menu_ids[2];
    for (size_t index = 0; index < 2; ++index) {
        const char *name = index ? "MENU_GRADIENTS" : "MENU_ELEMENTS_1";
        const AbResourceSource *source =
            ab_resource_owners_find(owners, AB_SPRITE_REGISTRY_SPRITE, text(name));
        const AbSpriteAtlas *atlas =
            ab_sprite_resource_atlas(ab_sprite_resource_from_source(source));
        AbSpriteAtlasImage image;
        CHECK(ab_sprite_atlas_image(atlas, 0, &image) &&
              ab_asset_textures_find(textures, image.image, menu_ids + index));
    }
    static const char *const release_menu[2] = {"MENU_COMPOSPRITES", "MENU"};
    CHECK(manage(assets, resources, release_menu, 2, false, &error) &&
          ab_asset_textures_sync(textures, owners, &error) && host.removals == 2 &&
          host.uploads == 22 && ab_asset_textures_count(textures) == 20);
#if defined(AB_ASSET_TEXTURES_COMMON)
    CHECK(asset_texture_gpu_removed(renderer, menu_ids[0], &error));
#endif
    for (size_t index = 0; index < 22; ++index) {
        if (index + 1 == menu_ids[0] || index + 1 == menu_ids[1]) {
            CHECK(!host.images[index]);
        } else {
            uint32_t texture;
            CHECK(ab_asset_textures_find(textures, initial[index], &texture) &&
                  texture == index + 1);
        }
    }
    static const char *const reload_menu[2] = {"MENU", "MENU_COMPOSPRITES"};
    CHECK(manage(assets, resources, reload_menu, 2, true, &error) &&
          ab_asset_textures_sync(textures, owners, &error) && host.uploads == 24 &&
          ab_asset_textures_count(textures) == 22 &&
          ab_asset_textures_bytes(textures) == retained &&
          host.images[menu_ids[0] - 1] && host.images[menu_ids[1] - 1]);
#if defined(AB_ASSET_TEXTURES_COMMON)
    CHECK(asset_texture_gpu_menu(platform, renderer, textures, owners, true, &error));
#endif
    CHECK(ab_asset_textures_close(&textures, &error) && host.removals == 24 &&
          failure_checks(owners) && wrap_checks(owners) &&
          alias_unload_checks(archive));
    host = (Host){0};
#if defined(AB_ASSET_TEXTURES_COMMON)
    host.renderer = renderer;
#endif
    CHECK(ab_asset_textures_open(callbacks(&host), 32, 128 * 1024 * 1024, &textures,
                                 &error) &&
          ab_asset_textures_sync(textures, owners, &error));
#if defined(AB_ASSET_TEXTURES_COMMON)
    CHECK(asset_texture_gpu_menu(platform, renderer, textures, owners, false, &error));
#endif
    /* Retire every CPU source map first. Cache references must keep their
     * descriptors alive until the final texture removal releases them. */
    ab_asset_resources_close(resources);
    resources = NULL;
    CHECK(ab_asset_textures_close(&textures, &error) && host.removals == 22);
#if defined(AB_ASSET_TEXTURES_COMMON)
    CHECK(ab_common_wait(renderer, &error));
#endif
    okay = true;
    puts("Verified real 22-image resource texture synchronization, reuse, group "
         "release/reload, aliases, image unload and retryable host failures.");
#if defined(AB_ASSET_TEXTURES_COMMON)
    puts("Common GPU uploads, menu pixel sampling and texture retirement passed.");
#endif
release:
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    if (textures) {
        host.fail_remove = 0;
        ab_asset_textures_close(&textures, NULL);
    }
#if defined(AB_ASSET_TEXTURES_COMMON)
    ab_common_free(renderer);
    cc_platform_destroy(platform);
#endif
    ab_asset_resources_close(resources);
    ab_android_assets_close(assets);
    return okay;
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
    AbError error = {0};
    uint8_t key[32] = {0};
    bool okay = ab_file_read(argv[2], 128 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
                ab_target_resource_key(&archive, 16 * 1024 * 1024, key, &error) &&
                managed_checks(&archive, (AbSpan){key, sizeof(key)});
    memset(key, 0, sizeof(key));
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay ? 0 : 1;
}
