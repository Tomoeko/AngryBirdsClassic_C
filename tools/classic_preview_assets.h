#ifndef AB_CLASSIC_PREVIEW_ASSETS_H
#define AB_CLASSIC_PREVIEW_ASSETS_H

#include "ab/camera_bootstrap.h"
#include "ab/camera_system.h"
#include "ab/camera_touch.h"
#include "ab/touch_snapshot.h"
#include "ab/classic_scene.h"
#include "ab/target_data.h"
#include "theme_preview_assets.h"
#include "sling_preview_assets.h"
#include "hud_preview_assets.h"

#include <string.h>

enum { PREVIEW_TOUCH_CAPACITY = 8 };

typedef struct {
    AbClassicLevel *level;
    AbSpriteAtlas *atlas;
    AbClassicScene *scene;
    AbClassicVisuals *visuals;
    PreviewTheme theme;
    PreviewSling sling;
    PreviewHud hud;
    AbCameraSystem *camera;
    AbCameraSnap snaps[2];
    AbTargetData sequence_data;
    AbCameraSequence sequence;
    AbCameraTouchHandler touch;
    AbCameraTouchShared shared_touch;
    AbCameraTouch filtered_touches[PREVIEW_TOUCH_CAPACITY];
    int32_t touch_keys[PREVIEW_TOUCH_CAPACITY];
    size_t touch_first_copy[PREVIEW_TOUCH_CAPACITY];
    size_t touch_indices[PREVIEW_TOUCH_CAPACITY];
    size_t touch_array_slots[PREVIEW_TOUCH_CAPACITY];
    AbIntegerOrderNode touch_nodes[PREVIEW_TOUCH_CAPACITY];
} PreviewLevel;

static void level_preview_free(PreviewLevel *level) {
    hud_preview_free(&level->hud);
    ab_camera_system_close(level->camera);
    ab_camera_sequence_free(&level->sequence);
    ab_target_data_close(&level->sequence_data);
    theme_preview_free(&level->theme);
    ab_classic_scene_close(level->scene);
    ab_classic_visuals_close(level->visuals);
    ab_sprite_atlas_close(level->atlas);
    ab_classic_level_close(level->level);
    *level = (PreviewLevel){0};
}

/* Fixed, explicitly recovered level-one resource selection. General resource
 * groups, gameplay scheduling, animation and complete level rendering remain
 * separate. The first bird is placed at the recovered idle sling endpoint;
 * other geometry/poses stay at their loaded state in this research view. */
static bool level_preview_open(const AbZip *archive, PreviewLevel *output,
                               AbError *error) {
    *output = (PreviewLevel){0};
    const char *paths[] = {"assets/data/images/base/INGAME_BLOCKS_1.dat",
                           "assets/data/images/base/INGAME_PIGS_NEW_SHEET_1.dat",
                           "assets/data/images/base/INGAME_BLOCKS_3.dat",
                           "assets/data/images/base/INGAME_BLOCKS_2.dat"};
    AbClassicLevelAssets assets = {.level_path = "assets/data/levels/pack1/Level1.lua",
                                   .catalog_path = "assets/data/scripts/blocks.lua",
                                   .sprite_paths = paths,
                                   .sprite_sheet_count = 4,
                                   .resource_bytes = 16 * 1024 * 1024,
                                   .placement_scale = 1,
                                   .physics_scale = 0.05f,
                                   .derive_ground_position = true,
                                   .derive_world_gravity = true};
    AbWorldDefinition definition = {.broadphase_limits = {128, 1024, 1024},
                                    .maximum_translation = 2,
                                    .maximum_translation_squared = 4,
                                    .warm_start = true,
                                    .clear_forces = true,
                                    .continuous = true};
    uint8_t key[32] = {0};
    bool okay = false;
    if (!ab_classic_level_load(archive, &assets, definition,
                               (AbClassicLevelLimits){.objects = 32,
                                                      .contacts = 128,
                                                      .storage_bytes = 1024 * 1024},
                               true, &output->level, error)) {
        goto release_level;
    }
    AbClassicLevelView view = ab_classic_level_view(output->level);
    if (!ab_sprite_atlas_open(archive, view.sprites, 16 * 1024 * 1024, 8 * 1024 * 1024,
                              &output->atlas, error) ||
        !sling_preview_open(view, output->atlas, &output->sling, error) ||
        !ab_classic_scene_open(output->level, output->atlas, 1, AB_BLEND_PREMULTIPLIED,
                               65536, &output->scene, error) ||
        !ab_classic_visuals_open(output->level,
                                 (AbClassicVisualResources){.atlas = output->atlas,
                                                            .storage_bytes = 65536,
                                                            .name_capacity = 256},
                                 &output->visuals, error) ||
        !ab_camera_museum_snaps(view.data->globals, (AbVec2){480, 320}, output->snaps,
                                error) ||
        !ab_camera_system_open(output->snaps, 2, (AbVec2){480, 320}, 200, 200, 65536,
                               &output->camera, error) ||
        !ab_target_resource_key(archive, 16 * 1024 * 1024, key, error) ||
        !theme_preview_open(archive, view.data->globals, (AbSpan){key, sizeof(key)},
                            (uint32_t)ab_sprite_atlas_count(output->atlas) + 1,
                            &output->theme, error) ||
        !hud_preview_open(archive, output->theme.white_texture + 1, &output->hud,
                          error) ||
        !ab_target_data_open(
            archive,
            "assets/data/scripts/cameraSequences/DefaultLevelStartCameraSequence.lua",
            (AbSpan){key, sizeof(key)}, 1024 * 1024, NULL, &output->sequence_data,
            error)) {
        goto release_level;
    }
    const AbDataValue *source =
        ab_data_get(output->sequence_data.data.globals,
                    (AbDataValue){.kind = AB_DATA_STRING,
                                  .string = {(const uint8_t *)"sequence", 8}});
    if (!source || source->kind != AB_DATA_TABLE ||
        !ab_camera_sequence_read(source->table, 65536, &output->sequence, error) ||
        !ab_camera_touch_init(&output->touch, &output->shared_touch) ||
        !ab_camera_system_start(output->camera, &output->sequence, NULL, NULL, error)) {
        if (!error || !error->message[0]) {
            ab_fail(error, "Cannot prepare level-one camera sequence");
        }
        goto release_level;
    }
    okay = true;

release_level:
    memset(key, 0, sizeof(key));
    if (!okay) {
        level_preview_free(output);
    }
    return okay;
}

static size_t level_preview_texture_count(const PreviewLevel *level) {
    return ab_sprite_atlas_count(level->atlas) +
           theme_preview_texture_count(&level->theme) + 2;
}

static bool level_preview_texture(const PreviewLevel *level, size_t index,
                                  uint32_t *texture, const AbImage **image) {
    size_t objects = ab_sprite_atlas_count(level->atlas);
    if (index < objects) {
        AbSpriteAtlasImage view;
        if (!ab_sprite_atlas_image(level->atlas, index, &view)) {
            return false;
        }
        *texture = (uint32_t)index + 1;
        *image = view.image;
        return true;
    }
    index -= objects;
    size_t themes = theme_preview_texture_count(&level->theme);
    if (index < themes) {
        return theme_preview_texture(&level->theme, index, texture, image);
    }
    if (index == themes) {
        *texture = level->hud.texture;
        *image = &level->hud.image;
        return true;
    }
    if (index == themes + 1) {
        AbSpriteAtlasImage view;
        if (!ab_sprite_atlas_image(level->hud.atlas, 0, &view)) {
            return false;
        }
        *texture = level->hud.texture + 1;
        *image = view.image;
        return true;
    }
    return false;
}

static bool level_preview_draw(PreviewLevel *level, float seconds, AbDrawList *list,
                               const AbCameraTouchInput *touch_input, AbError *error) {
    AbCameraTouchInput empty = {.ingame = true, .released_after_specialty = true};
    AbCameraTouchInput resolved = touch_input ? *touch_input : empty;
    AbTouchSnapshotWorkspace workspace = {
        level->touch_keys,
        level->touch_first_copy,
        PREVIEW_TOUCH_CAPACITY,
        {level->touch_indices, PREVIEW_TOUCH_CAPACITY, level->touch_array_slots,
         PREVIEW_TOUCH_CAPACITY, level->touch_nodes, PREVIEW_TOUCH_CAPACITY}};
    size_t count;
    if (!ab_touch_snapshot(resolved.touches, resolved.count, true, &workspace,
                           level->filtered_touches, PREVIEW_TOUCH_CAPACITY, &count,
                           error)) {
        return false;
    }
    resolved.touches = level->filtered_touches;
    resolved.count = count;
    AbCameraTouchOutput touch;
    if (!ab_camera_touch_update(&level->touch, &level->shared_touch, level->camera,
                                seconds, &resolved, &touch, error)) {
        return false;
    }
    AbCameraSystemInput input = {.targets = touch.targets,
                                 .targets_present = touch.targets_present,
                                 .user_input = touch.user_input};
    if (!ab_camera_system_update(level->camera, seconds, &input, error)) {
        return false;
    }
    AbCameraSystemView view = ab_camera_system_view(level->camera);
    list->count = 0;
    return view.viewport &&
           theme_preview_draw(&level->theme, view.viewport->rendering, false, list,
                              error) &&
           sling_preview_draw(&level->sling, view.viewport->rendering,
                              AB_SLING_DRAW_BACK, list, error) &&
           ab_classic_scene_sync_visuals(level->scene, level->visuals, error) &&
           ab_classic_scene_draw(level->scene, view.viewport->rendering, 480, 320, list,
                                 error) &&
           sling_preview_draw(&level->sling, view.viewport->rendering,
                              AB_SLING_DRAW_HOLDER, list, error) &&
           theme_preview_draw(&level->theme, view.viewport->rendering, true, list,
                              error) &&
           sling_preview_draw(&level->sling, view.viewport->rendering,
                              AB_SLING_DRAW_FRONT, list, error) &&
           hud_preview_draw(&level->hud, list, error);
}

#endif
