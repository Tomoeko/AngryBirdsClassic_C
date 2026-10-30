#ifndef AB_TEST_CLASSIC_RESOURCE_SCENE_H
#define AB_TEST_CLASSIC_RESOURCE_SCENE_H

#include "ab/android_assets.h"
#include "ab/asset_sprite_provider.h"
#include "ab/camera_bootstrap.h"
#include "ab/target_resources.h"

#include <stdlib.h>
#if defined(AB_CLASSIC_SCENE_COMMON)
#include "classic_scene_common.h"
#endif

typedef struct {
    const AbImage *images[32];
    size_t uploads, removals;
#if defined(AB_CLASSIC_SCENE_COMMON)
    CcPlatform *platform;
    AbCommonRenderer *renderer;
#endif
} ResourceSceneHost;

static bool resource_scene_upload(void *context, uint32_t texture,
                                  AbSpriteAtlasImage image, AbError *error) {
    ResourceSceneHost *host = context;
    if (!texture || texture > 32 || !image.image || host->images[texture - 1]) {
        return ab_fail(error, "Invalid resource scene texture upload");
    }
#if defined(AB_CLASSIC_SCENE_COMMON)
    if (host->renderer && !ab_common_upload(host->renderer, texture, image.image,
                                            image.repeat_s, image.repeat_t, error)) {
        return false;
    }
#endif
    host->images[texture - 1] = image.image;
    ++host->uploads;
    return true;
}

static bool resource_scene_remove(void *context, uint32_t texture, AbError *error) {
    ResourceSceneHost *host = context;
    if (!texture || texture > 32 || !host->images[texture - 1]) {
        return ab_fail(error, "Invalid resource scene texture removal");
    }
#if defined(AB_CLASSIC_SCENE_COMMON)
    if (host->renderer && !ab_common_unload(host->renderer, texture, error)) {
        return false;
    }
#endif
    host->images[texture - 1] = NULL;
    ++host->removals;
    return true;
}

static bool resource_scene_load(AbAndroidAssets *assets, AbAssetResources *resources,
                                AbError *error) {
    static const char *const groups[] = {"OTHER", "MENU", "INGAME", "INGAME_PIGS_NEW",
                                         "INGAME_COMPOSPRITES"};
    AbDataEntry entries[5];
    for (size_t index = 0; index < 5; ++index) {
        float number = (float)(index + 1);
        AbDataValue key = {.kind = AB_DATA_NUMBER};
        memcpy(&key.bits, &number, sizeof(number));
        entries[index] = (AbDataEntry){
            key, {.kind = AB_DATA_STRING, .string = string(groups[index])}};
    }
    AbDataTable requested = {entries, 5, 5, NULL};
    return ab_android_assets_manage(assets, &requested, true, ab_asset_resources_host,
                                    resources, error);
}

static bool resource_scene_dimensions(AbAssetSpriteProvider *adapter) {
    /* Original LuaResources getters in LevelS039: absent, ordinary and composite
     * registry results. Private capture: reference-bounds-LevelS039.json. */
    static const struct {
        const char *name;
        float width, height;
    } expected[] = {{"PRESSURE_METER", 0, 0},
                    {"BIRD_RED", 46, 45},
                    {"", 0, 0},
                    {"ab_missing_bounds_520", 0, 0},
                    {"BLOCK_GREEN_SPANNER_2", 0, 0},
                    {"BIG_HELMET_MULTI_0", 136, 118}};
    for (size_t index = 0; index < sizeof(expected) / sizeof(*expected); ++index) {
        AbVec2 size = {-1, -1};
        CHECK(ab_asset_sprite_provider_size(adapter, string(expected[index].name),
                                            &size, NULL) &&
              bits(size.x) == bits(expected[index].width) &&
              bits(size.y) == bits(expected[index].height));
    }
    AbVec2 unchanged = {123, 456};
    CHECK(
        !ab_asset_sprite_provider_size(NULL, string(""), &unchanged, NULL) &&
        !ab_asset_sprite_provider_size(adapter, (AbSpan){NULL, 1}, &unchanged, NULL) &&
        !ab_asset_sprite_provider_size(adapter, string(""), NULL, NULL) &&
        unchanged.x == 123 && unchanged.y == 456);
    puts("All 12 native absent/ordinary/composite resource dimensions match; "
         "invalid queries preserve output.");
    return true;
}

static bool resource_scene_zero_circle(const AbZip *archive, AbAndroidAssets *planning,
                                       AbAssetResources *resources,
                                       AbAssetSpriteProvider *adapter) {
    AbDataEntry entry = {{.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3f800000)},
                         {.kind = AB_DATA_STRING, .string = string("THEME_15")}};
    AbDataTable group = {&entry, 1, 1, NULL};
    CHECK(ab_android_assets_manage(planning, &group, true, ab_asset_resources_host,
                                   resources, NULL));
    AbClassicLevelAssets assets = {
        .level_path = "assets/data/levels/surf/LevelS039.lua",
        .catalog_path = "assets/data/scripts/blocks.lua",
        .resource_bytes = 32 * 1024 * 1024,
        .placement_scale = 1,
        .physics_scale = 0.05f,
        .derive_ground_position = true,
        .derive_world_gravity = true,
        .sprite_context = adapter,
        .sprite_size = ab_asset_sprite_provider_size,
    };
    AbWorldDefinition definition = {.broadphase_limits = {1024, 8192, 8192},
                                    .maximum_translation = 2,
                                    .maximum_translation_squared = 4,
                                    .warm_start = true,
                                    .clear_forces = true,
                                    .continuous = true};
    AbClassicLevelLimits limits = {224, 8192, 32 * 1024 * 1024, 0, 0};
    AbClassicLevel *level = NULL;
    AbError error = {0};
    bool okay = ab_classic_level_load(archive, &assets, definition, limits, true,
                                      &level, &error);
    AbClassicLevelView view = ab_classic_level_view(level);
    okay = okay && view.bodies->count == 223;
    size_t found = 0;
    for (size_t index = 0; okay && index < view.bodies->count; ++index) {
        const AbLevelObject *object = view.bodies->objects + index;
        AbSpan name = object->definition_name;
        if (name.size != 14 || memcmp(name.data, "PRESSURE_METER", 14) != 0) {
            continue;
        }
        const AbWorldFixture *fixture =
            view.world->storage.fixtures + object->physics.fixture;
        okay = fixture->shape.kind == AB_SHAPE_CIRCLE &&
               bits(fixture->shape.data.circle.radius) == 0 &&
               object->physics.dimensions.width == -1 &&
               object->physics.dimensions.height == -1 &&
               bits(object->physics.dimensions.radius) == 0 &&
               bits(fixture->density) == UINT32_C(0x3f000000) &&
               bits(fixture->friction) == UINT32_C(0x3f800000) &&
               bits(fixture->restitution) == 0;
        ++found;
    }
    okay = okay && found == 2 && ab_world_step(view.world, 1.0f / 30, 8, 3, &error);
    if (!okay && error.message[0]) {
        fprintf(stderr, "Zero-circle level failed: %s\n", error.message);
    }
    ab_classic_level_close(level);
    CHECK(okay);
    puts("LevelS039 loads all 223 objects and steps with both native zero-radius "
         "pressure meters; their unavailable sprites remain absent.");
    return true;
}

static bool resource_scene_geometry(AbClassicLevel *level, const AbSpriteAtlas *atlas,
                                    AbClassicScene *scene,
                                    const AbAssetSpriteProvider *adapter,
                                    ResourceSceneHost *host) {
    AbClassicScene *reference = NULL;
    CHECK(ab_classic_scene_open(level, atlas, 7, AB_BLEND_PREMULTIPLIED, 16384,
                                &reference, NULL));
    AbDrawCommand actual[23] = {0}, expected[23] = {0};
    AbDrawList actual_list = {actual, 0, 23}, expected_list = {expected, 0, 23};
    AbClassicLevelView level_view = ab_classic_level_view(level);
    AbCameraSnap snaps[2];
    AbCameraViewport viewport;
    bool okay = ab_camera_museum_snaps(level_view.data->globals, (AbVec2){480, 320},
                                       snaps, NULL) &&
                ab_camera_viewport(&(AbCameraViewportInput){.center = snaps[1].position,
                                                            .pixel_size = {480, 320},
                                                            .zoom = snaps[1].zoom},
                                   &viewport);
    AbCoordinateCamera camera = okay ? viewport.rendering : (AbCoordinateCamera){0};
    okay = okay && ab_classic_scene_draw(scene, camera, 480, 320, &actual_list, NULL) &&
           ab_classic_scene_draw(reference, camera, 480, 320, &expected_list, NULL) &&
           actual_list.count > 0 && actual_list.count <= 22 &&
           actual_list.count == expected_list.count;
    AbClassicSceneView view = ab_classic_scene_view(scene);
    size_t different_textures = 0;
    for (size_t index = 0; okay && index < view.count; ++index) {
        const AbSceneSprite *item = view.items + index;
        if (!item->region) {
            continue;
        }
        AbSpriteAtlasBinding binding;
        uint32_t texture;
        okay = adapter->provider.sprite(adapter->provider.context,
                                        level_view.initial_sprites[index].sprite.string,
                                        &binding) &&
               adapter->provider.texture(adapter->provider.context, binding.image,
                                         &texture) &&
               item->region == binding.sprite.region && item->texture == texture &&
               item->texture_width == binding.image->width &&
               item->texture_height == binding.image->height;
    }
    for (size_t index = 0; okay && index < actual_list.count; ++index) {
        different_textures += actual[index].texture != expected[index].texture;
        uint32_t texture = actual[index].texture;
        actual[index].texture = expected[index].texture;
        okay = memcmp(actual + index, expected + index, sizeof(*actual)) == 0;
        actual[index].texture = texture;
    }
#if defined(AB_CLASSIC_SCENE_COMMON)
    okay = okay && resource_scene_gpu_compare(host->platform, host->renderer,
                                              &actual_list, &expected_list, NULL);
#else
    (void)host;
#endif
    ab_classic_scene_close(reference);
    CHECK(okay && different_textures);
    return true;
}

static bool resource_scene_actions(AbClassicLevel *level, AbClassicScene *scene,
                                   AbClassicVisuals *visuals,
                                   AbAssetSpriteProvider *adapter,
                                   AbAssetTextures *textures, VisualNotices *notices) {
    AbClassicSceneView view = ab_classic_scene_view(scene);
    CHECK(view.count == 23);
    AbSceneSprite before[23];
    memcpy(before, view.items, sizeof(before));
    AbAssetSpriteProvider other;
    CHECK(ab_asset_sprite_provider_init(adapter->resources, textures, &other, NULL));
    AbClassicVisualResources resources = {
        .provider = &other.provider, .storage_bytes = 16384, .name_capacity = 128};
    AbClassicVisuals *other_visuals = NULL;
    CHECK(ab_classic_visuals_open(level, resources, &other_visuals, NULL));
    bool rejected = !ab_classic_scene_sync_visuals(scene, other_visuals, NULL);
    ab_classic_visuals_close(other_visuals);
    CHECK(rejected && memcmp(before, view.items, sizeof(before)) == 0);
    AbVec2 size = {0};
    CHECK(ab_asset_textures_clear(textures, NULL) &&
          ab_asset_sprite_provider_size(adapter, string("BIRD_RED"), &size, NULL) &&
          size.x > 0 && size.y > 0 &&
          !ab_classic_scene_sync_visuals(scene, visuals, NULL) &&
          memcmp(before, view.items, sizeof(before)) == 0 &&
          ab_asset_textures_sync(textures,
                                 ab_asset_resources_owners(adapter->resources), NULL) &&
          ab_classic_scene_sync_visuals(scene, visuals, NULL));
    const AbSpriteRegistry *registry = ab_asset_resources_registry(adapter->resources);
    AbSpan composite = {0};
    for (size_t index = 0; index < registry->count; ++index) {
        if (registry->entries[index].kind == AB_SPRITE_REGISTRY_COMPOSITE) {
            composite = registry->entries[index].name;
            break;
        }
    }
    AbSpan original = ab_classic_visuals_object(visuals, 0)->sprite;
    uint8_t name[128];
    CHECK(original.size <= sizeof(name));
    memcpy(name, original.data, original.size);
    original.data = name;
    memcpy(before, view.items, sizeof(before));
    size = (AbVec2){123, 456};
    AbResolvedComposite bounds;
    CHECK(
        composite.size &&
        ab_asset_resources_find_composite(adapter->resources, composite, &bounds) &&
        ab_asset_sprite_provider_size(adapter, composite, &size, NULL) &&
        size.x == (float)bounds.bounds.width && size.y == (float)bounds.bounds.height &&
        ab_classic_visuals_set_sprite(visuals, 0, composite, NULL) &&
        ab_classic_visuals_object(visuals, 0)->attached->composite &&
        ab_classic_scene_sync_visuals(scene, visuals, NULL) && !view.items[0].unbound);
    AbDrawCommand *commands = calloc(bounds.source->part_count, sizeof(*commands));
    CHECK(commands && bounds.source->part_count);
    AbDrawList list = {commands, 0, bounds.source->part_count};
    AbObjectSpritePose pose = view.items[0].pose;
    AbCoordinateCamera camera = {
        {20 * pose.physics_position.x - 240, 20 * pose.physics_position.y - 160},
        {1, 1}};
    bool compound_draw =
        ab_classic_scene_draw_object(scene, 0, camera, 480, 320, &list, NULL) &&
        list.count > 0;
    list.count = list.capacity = 0;
    compound_draw =
        compound_draw &&
        !ab_classic_scene_draw_object(scene, 0, camera, 480, 320, &list, NULL) &&
        list.count == 0;
    free(commands);
    CHECK(compound_draw && ab_classic_visuals_set_sprite(visuals, 0, original, NULL) &&
          ab_classic_scene_sync_visuals(scene, visuals, NULL));
    *notices = (VisualNotices){0};
    CHECK(visual_action_checks(scene, visuals, notices));
    return true;
}

static bool resource_scene_empty(AbClassicScene *scene, AbClassicVisuals *visuals,
                                 VisualNotices notices) {
    AbClassicSceneView view = ab_classic_scene_view(scene);
    CHECK(view.count == 23 && notices.count == 22);
    CHECK(ab_classic_scene_sync_visuals(scene, visuals, NULL) &&
          !ab_classic_scene_set_sprite(scene, 22, string("BIRD_RED"), NULL) &&
          !ab_classic_scene_set_visible(scene, 22, true, NULL) &&
          ab_classic_scene_set_visible(scene, 0, false, NULL) &&
          ab_classic_scene_set_visible(scene, 0, true, NULL));
    for (size_t index = 0; index < view.count; ++index) {
        CHECK(view.items[index].unbound);
    }
    AbDrawCommand commands[23] = {0};
    AbDrawList list = {commands, 0, 23};
    CHECK(ab_classic_scene_draw(scene, (AbCoordinateCamera){{0, 0}, {1, 1}}, 480, 320,
                                &list, NULL) &&
          !list.count);
    return true;
}

static bool resource_scene_checks(const AbZip *archive, AbClassicLevel *level,
                                  const AbSpriteAtlas *atlas, FILE *native,
                                  AbError *error) {
    AbAndroidAssets *assets = NULL;
    AbAssetResources *resources = NULL;
    AbAssetTextures *textures = NULL;
    AbClassicScene *scene = NULL;
    AbClassicLevel *resolved_level = NULL;
    AbClassicVisuals *visuals = NULL;
    AbAssetSpriteProvider adapter = {0};
    ResourceSceneHost host = {0};
    VisualNotices notices = {0};
    uint8_t key[32] = {0};
    bool okay = false;
    AbAndroidAssetLimits planning = {480, 320, false, false, false, 64, 8, 1024 * 1024};
    AbAssetResourceLimits limits = {64, 8192, 128 * 1024 * 1024, 16 * 1024 * 1024};
    if (!ab_target_resource_key(archive, 16 * 1024 * 1024, key, error) ||
        !ab_android_assets_open(archive, (AbSpan){key, sizeof(key)}, planning, &assets,
                                error) ||
        !ab_asset_resources_open(archive, limits, &resources, error) ||
        !ab_asset_textures_open(
            (AbAssetTextureHost){&host, resource_scene_upload, resource_scene_remove},
            32, 128 * 1024 * 1024, &textures, error) ||
        !ab_asset_textures_sync(textures, ab_asset_resources_owners(resources),
                                error) ||
        !ab_asset_sprite_provider_init(resources, textures, &adapter, error)) {
        goto release;
    }
    AbClassicVisualResources visual_resources = {.provider = &adapter.provider,
                                                 .context = &notices,
                                                 .missing = visual_missing,
                                                 .storage_bytes = 16384,
                                                 .name_capacity = 128};
    AbSpriteProvider invalid = {0};
    if (ab_classic_scene_open_resources(level, &invalid, AB_BLEND_PREMULTIPLIED, 16384,
                                        &scene, NULL) ||
        scene || ab_asset_sprite_provider_init(NULL, textures, &adapter, NULL)) {
        goto release;
    }
    visual_resources.atlas = atlas;
    if (ab_classic_visuals_open(level, visual_resources, &visuals, NULL) || visuals) {
        goto release;
    }
    visual_resources.atlas = NULL;
    AbVec2 missing_size = {123, 456};
    if (!ab_asset_sprite_provider_size(&adapter, string("missing"), &missing_size,
                                       NULL) ||
        missing_size.x != 0 || missing_size.y != 0 ||
        open_level(archive, &adapter, ab_asset_sprite_provider_size, &resolved_level,
                   NULL) ||
        resolved_level) {
        goto release;
    }
    if (!ab_classic_scene_open_resources(
            level, &adapter.provider, AB_BLEND_PREMULTIPLIED, 16384, &scene, error) ||
        !ab_classic_visuals_open(level, visual_resources, &visuals, error) ||
        !resource_scene_empty(scene, visuals, notices) ||
        !resource_scene_load(assets, resources, error) ||
        !resource_scene_dimensions(&adapter)) {
        goto release;
    }
#if defined(AB_CLASSIC_SCENE_COMMON)
    if (!resource_scene_gpu_open(atlas, &host.platform, &host.renderer, error)) {
        goto release;
    }
#endif
    if (!ab_asset_textures_sync(textures, ab_asset_resources_owners(resources),
                                error) ||
        !resource_scene_empty(scene, visuals, notices)) {
        goto release;
    }
    for (size_t index = 0; index < 23; ++index) {
        AbSpan name = ab_classic_visuals_object(visuals, index)->sprite;
        if (!ab_classic_visuals_set_sprite(visuals, index, name, error)) {
            goto release;
        }
    }
    if (!ab_classic_scene_sync_visuals(scene, visuals, error) ||
        !resource_scene_geometry(level, atlas, scene, &adapter, &host) ||
        !compare_native(scene, ab_classic_level_view(level), native, visuals) ||
        fseek(native, 0, SEEK_SET) != 0 ||
        !resource_scene_actions(level, scene, visuals, &adapter, textures, &notices)) {
        goto release;
    }
    ab_classic_scene_close(scene);
    scene = NULL;
    if (!open_level(archive, &adapter, ab_asset_sprite_provider_size, &resolved_level,
                    error) ||
        ab_classic_level_view(resolved_level).sprites ||
        !ab_classic_scene_open_resources(resolved_level, &adapter.provider,
                                         AB_BLEND_PREMULTIPLIED, 16384, &scene,
                                         error) ||
        !resource_scene_geometry(level, atlas, scene, &adapter, &host) ||
        !resource_scene_zero_circle(archive, assets, resources, &adapter)) {
        goto release;
    }
    puts("Resource-group scene bindings match all 22 native selections and ordinary "
         "geometry; cached missing names, texture loss, composite drawing and "
         "partial-command rollback pass.");
    okay = true;

release:
    memset(key, 0, sizeof(key));
    ab_classic_visuals_close(visuals);
    ab_classic_scene_close(scene);
    ab_classic_level_close(resolved_level);
    if (!ab_asset_textures_close(&textures, error)) {
        okay = false;
    }
    ab_asset_resources_close(resources);
    ab_android_assets_close(assets);
#if defined(AB_CLASSIC_SCENE_COMMON)
    if (host.renderer && !ab_common_wait(host.renderer, error)) {
        okay = false;
    }
    ab_common_free(host.renderer);
    cc_platform_destroy(host.platform);
#endif
    return okay;
}

#endif
