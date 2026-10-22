#ifndef AB_TEST_LEVEL_CAMERA_H
#define AB_TEST_LEVEL_CAMERA_H

#include "ab/camera_bootstrap.h"
#include "ab/classic_camera.h"
#include "ab/classic_behaviors.h"
#include "ab/classic_render_scene.h"
#include "ab/trajectory_follow.h"
#include "ab/museum_impact.h"
#include "ab/target_data.h"
#include "ab/lua_asset_sprite.h"
#include "ab/lua_asset_renderer.h"
#include "ab/scene_id.h"
#include "ab/scene_membership.h"
#include "camera_state_words.h"
#include "../tools/gpu_scene_write.h"

typedef struct {
    AbTargetData data[2];
    AbCameraSequence sources[2];
    AbClassicCamera *owner;
    AbClassicBehaviors *behaviors;
    AbDataValue flying_value;
    AbTrajectoryFollow trajectory;
    AbTrajectoryTrack tracks[23];
    AbTrajectoryPoint points[128];
    const AbZip *archive;
    AbSpriteAtlas *atlas;
    AbSpriteProvider provider;
    AbClassicRenderScene *render_scene;
    uint32_t scene_counter;
    size_t point_commands;
    AbDrawCommand most_point_commands[128];
    AbDrawCommand most_scene_commands[128];
    size_t most_point_command_count;
    size_t most_scene_command_count;
    size_t point_count, collision_notifications, trajectory_removes, trajectory_keeps;
    bool compare_trajectory, compare_scene;
    AbClassicLevel *level;
    AbCameraSnap snaps[2];
    float gravity;
    size_t flying, loads, logs, predictions, rays, trajectory_adds;
} LevelCamera;

static void level_camera_close(LevelCamera *camera) {
    ab_classic_render_scene_close(camera->render_scene);
    ab_sprite_atlas_close(camera->atlas);
    ab_classic_behaviors_close(camera->behaviors);
    ab_classic_camera_close(camera->owner);
    for (size_t index = 0; index < 2; ++index) {
        ab_camera_sequence_free(camera->sources + index);
        ab_target_data_close(camera->data + index);
    }
    *camera = (LevelCamera){0};
}

static bool level_camera_assets(LevelCamera *camera, const AbZip *archive,
                                AbError *error) {
    camera->archive = archive;
    uint8_t key[32] = {0};
    const char *const paths[] = {
        "assets/data/scripts/cameraSequences/FollowShotBirdCameraSequence.lua",
        "assets/data/scripts/cameraSequences/FollowShotBirdEndCameraSequence.lua"};
    bool okay = ab_target_resource_key(archive, 16 * 1024 * 1024, key, error);
    for (size_t index = 0; okay && index < 2; ++index) {
        okay = ab_target_data_open(archive, paths[index], (AbSpan){key, sizeof(key)},
                                   1024 * 1024, NULL, camera->data + index, error);
        const AbDataValue *sequence =
            okay ? ab_data_get(camera->data[index].data.globals, string("sequence"))
                 : NULL;
        okay = okay && sequence && sequence->kind == AB_DATA_TABLE &&
               ab_camera_sequence_read(sequence->table, 65536, camera->sources + index,
                                       error);
    }
    memset(key, 0, sizeof(key));
    return okay;
}

static bool level_camera_load(void *context, const char *name,
                              const AbCameraSequence **output, AbError *error) {
    (void)error;
    LevelCamera *camera = context;
    bool shot = strcmp(name, "FollowShotBirdCameraSequence") == 0;
    CHECK(shot || strcmp(name, "FollowShotBirdEndCameraSequence") == 0);
    if (!camera->loads) {
        AbClassicLevelView level = ab_classic_level_view(camera->level);
        const AbDataValue *launch =
            ab_data_get(&level.properties->objects[camera->flying].physical,
                        string("launchVelocity"));
        CHECK(shot && (!launch || launch->kind == AB_DATA_NIL));
    }
    *output = camera->sources + (shot ? 0 : 1);
    ++camera->loads;
    return true;
}

static bool level_camera_log(void *context, AbSpan message, AbError *error) {
    (void)error;
    LevelCamera *camera = context;
    const char *expected =
        camera->logs % 2u    ? "<CameraSequenceManager> starting new camera sequence"
        : camera->loads == 1 ? "<CameraSequenceManager> loading sequence from file: "
                               "FollowShotBirdCameraSequence"
                             : "<CameraSequenceManager> loading sequence from file: "
                               "FollowShotBirdEndCameraSequence";
    CHECK(message.size == strlen(expected) &&
          !memcmp(message.data, expected, message.size));
    ++camera->logs;
    return true;
}

static bool level_camera_prediction(void *context, size_t rays, AbError *error) {
    (void)error;
    LevelCamera *camera = context;
    CHECK(rays && rays % 3u == 0);
    ++camera->predictions;
    return true;
}

static bool level_camera_ray(void *context, AbVec2 first, AbVec2 second,
                             const AbLevelRaycast *result, AbError *error) {
    (void)first;
    (void)second;
    (void)result;
    (void)error;
    ++((LevelCamera *)context)->rays;
    return true;
}

static bool level_camera_behavior(void *context, const AbMuseumBirdCall *call,
                                  AbError *error) {
    LevelCamera *camera = context;
    if (call->operation == AB_MUSEUM_CAMERA_SEQUENCE)
        return ab_classic_camera_start_sequence(camera->owner, call->sequence, error);
    CHECK(call->object.kind == AB_DATA_TABLE &&
          call->object.table == camera->flying_value.table);
    if (call->operation == AB_MUSEUM_TRAJECTORY_REMOVE) {
        ++camera->trajectory_removes;
        return ab_trajectory_follow_remove(&camera->trajectory, call->object.table,
                                           error);
    }
    if (call->operation == AB_MUSEUM_TRAJECTORY_KEEP) {
        /* The reference path keeps this bird's twelve ordinary points. This
         * retained point observation is not a renderer/disposal implementation. */
        ++camera->trajectory_keeps;
        for (size_t index = 0; index < camera->point_count; ++index)
            CHECK(camera->points[index].object == call->object.table);
        return true;
    }
    CHECK(call->operation == AB_MUSEUM_TRAJECTORY_ADD);
    if (!ab_trajectory_follow_add(&camera->trajectory, call->object.table, NULL, 0,
                                  false, false, error))
        return false;
    ++camera->trajectory_adds;
    return true;
}

static bool level_camera_sprite(void *context, AbSpan name,
                                AbSpriteAtlasBinding *output) {
    LevelCamera *camera = context;
    return ab_sprite_atlas_find(camera->atlas, name, output);
}

static bool level_camera_texture(void *context, const AbImage *image,
                                 uint32_t *output) {
    LevelCamera *camera = context;
    for (size_t index = 0; index < ab_sprite_atlas_count(camera->atlas); ++index) {
        AbSpriteAtlasImage view;
        if (ab_sprite_atlas_image(camera->atlas, index, &view) && view.image == image) {
            *output = (uint32_t)index + 1;
            return true;
        }
    }
    return false;
}

static bool level_camera_point(void *context, const AbTrajectoryPoint *point,
                               AbError *error) {
    LevelCamera *camera = context;
    size_t index = camera->point_count;
    CHECK(index < 128);
    AbLuaAssetRenderer *renderer = NULL;
    if (!ab_classic_render_scene_create_lua(camera->render_scene, point->sprite,
                                            (float)point->layer, &renderer, error))
        return false;
    renderer->pose.world_position = point->position;
    renderer->pose.angle_radians = point->angle;
    camera->points[camera->point_count++] = *point;
    return true;
}

static bool level_camera_draw_points(LevelCamera *camera) {
    AbClassicCameraView view = ab_classic_camera_view(camera->owner);
    CHECK(view.system.viewport &&
          ab_classic_render_scene_sync(camera->render_scene, NULL));
    AbClassicRenderSceneView scene = ab_classic_render_scene_view(camera->render_scene);
    CHECK(scene.extra_count == camera->point_count);
    AbDrawCommand commands[128];
    AbDrawList list = {commands, 0, 128};
    for (size_t index = 0; index < scene.extra_count; ++index) {
        const AbLuaAssetRenderer *renderer = scene.extra_renderers + index;
        CHECK(renderer->id.value == (int32_t)index + 23);
        AbDrawCommand command = {0};
        bool visible;
        CHECK(ab_lua_asset_renderer_command(renderer, view.system.viewport->rendering,
                                            false, 480, 320, &command, &visible, NULL));
        if (visible) {
            CHECK(command.coordinates == AB_DRAW_CLIP &&
                  command.blend == AB_BLEND_PREMULTIPLIED && command.opacity == 1 &&
                  command.texture == renderer->sprite.texture &&
                  ab_draw_list_push(&list, &command, NULL));
        }
    }
    camera->point_commands += list.count;
    AbDrawCommand mixed_commands[256];
    AbDrawList mixed = {mixed_commands, 0, 256};
    CHECK(ab_classic_render_scene_draw(camera->render_scene,
                                       view.system.viewport->rendering, 480, 320,
                                       &mixed, NULL) &&
          mixed.count >= list.count);
    if (list.count > camera->most_point_command_count) {
        CHECK(mixed.count <= 128);
        memcpy(camera->most_point_commands, list.commands,
               list.count * sizeof(*list.commands));
        camera->most_point_command_count = list.count;
        memcpy(camera->most_scene_commands, mixed.commands,
               mixed.count * sizeof(*mixed.commands));
        camera->most_scene_command_count = mixed.count;
    }
    return true;
}

static bool level_scene_compare(FILE *file, LevelCamera *camera, size_t frame) {
    AbClassicRenderSceneView scene = ab_classic_render_scene_view(camera->render_scene);
    AbClassicSceneView objects =
        ab_classic_scene_view(ab_classic_render_scene_objects(camera->render_scene));
    uint32_t header[2], actual[] = {camera->scene_counter, (uint32_t)scene.count};
    CHECK(read_body_words(file, header, 2) &&
          compare_level_words(actual, header, 2, frame, "scene", "counts", 0));
    for (size_t index = 0; index < scene.count; ++index) {
        AbClassicRenderRoute route = scene.routes[index];
        AbSceneMember member = scene.members[index];
        uint32_t wanted[12], row[12] = {(uint32_t)member.id, (uint32_t)route.kind,
                                        (uint32_t)route.index, (uint32_t)member.layer};
        if (route.kind == AB_CLASSIC_RENDER_OBJECT) {
            const AbSceneSprite *sprite = objects.items + route.index;
            CHECK(member.object == sprite);
            row[4] = number(sprite->pose.physics_position.x).bits;
            row[5] = number(sprite->pose.physics_position.y).bits;
            row[6] = number(sprite->pose.angle_radians).bits;
            row[7] = number(sprite->pose.scale.x).bits;
            row[8] = number(sprite->pose.scale.y).bits;
            row[9] = sprite->visible;
            row[10] = !sprite->unbound && sprite->region;
            row[11] = sprite->special_animation;
        } else {
            const AbLuaAssetRenderer *renderer = scene.extra_renderers + route.index;
            CHECK(member.object == renderer);
            row[4] = number(renderer->pose.world_position.x).bits;
            row[5] = number(renderer->pose.world_position.y).bits;
            row[6] = number(renderer->pose.angle_radians).bits;
            row[7] = number(renderer->pose.scale.x).bits;
            row[8] = number(renderer->pose.scale.y).bits;
            row[9] = renderer->pose.visible;
            row[10] = renderer->sprite.region != NULL;
        }
        CHECK(read_body_words(file, wanted, 12) &&
              compare_level_words(row, wanted, 12, frame, "scene", "member", index));
    }
    return true;
}

static bool level_camera_write_gpu(LevelCamera *camera, const char *path,
                                   AbError *error) {
    size_t count = ab_sprite_atlas_count(camera->atlas);
    if (!camera->compare_trajectory || !camera->most_point_command_count || count > 16)
        return ab_fail(error, "No bounded trajectory command frame to export");
    const AbImage *images[16];
    for (size_t index = 0; index < count; ++index) {
        AbSpriteAtlasImage image;
        if (!ab_sprite_atlas_image(camera->atlas, index, &image))
            return ab_fail(error, "Missing trajectory image export binding");
        images[index] = image.image;
    }
    AbDrawList list = camera->compare_scene
                          ? (AbDrawList){camera->most_scene_commands,
                                         camera->most_scene_command_count, 128}
                          : (AbDrawList){camera->most_point_commands,
                                         camera->most_point_command_count, 128};
    const float clear[] = {0, 0, 0, 1};
    return gpu_scene_write(path, images, count, &list, clear, error);
}

static bool level_camera_collision(void *context, const AbMuseumImpactBirdCall *call,
                                   AbError *error) {
    LevelCamera *camera = context;
    CHECK(call->bird == camera->flying && !call->contact.point_present);
    AbBlockCollisionNotice notice = {.force = call->force,
                                     .damage = call->damage,
                                     .normal = call->contact.normal,
                                     .normal_present = true,
                                     .point_present = call->contact.point_present,
                                     .point = call->contact.point};
    ++camera->collision_notifications;
    return ab_classic_behaviors_collision(camera->behaviors, call->bird, call->target,
                                          &notice, error);
}

static bool level_trajectory_compare(FILE *file, LevelCamera *camera, size_t frame) {
    uint32_t expected[4];
    CHECK(read_body_words(file, expected, 4));
    AbClassicLevelView level = ab_classic_level_view(camera->level);
    const AbDataValue *group =
        ab_data_get(&level.properties->objects[camera->flying].behaviors,
                    string("MuseumBirdTrajectoryRenderer"));
    CHECK(group && group->kind == AB_DATA_TABLE && group->table->count == 1);
    AbDataValue instance = group->table->entries[0].value;
    CHECK(instance.kind == AB_DATA_TABLE);
    const AbDataValue *flying = ab_data_get(instance.table, string("flying"));
    uint32_t actual[] = {
        (uint32_t)camera->trajectory.count, (uint32_t)camera->point_count,
        flying ? (uint32_t)flying->kind : 0, flying ? flying->bits : 0};
    CHECK(compare_level_words(actual, expected, 4, frame, "trajectory", "counts", 0));
    for (size_t index = 0; index < camera->trajectory.count; ++index) {
        AbTrajectoryTrack *track = camera->tracks + index;
        uint32_t wanted[7];
        CHECK(read_body_words(file, wanted, 7));
        CHECK(track->object == camera->flying_value.table);
        uint32_t row[] = {(uint32_t)camera->flying + 1,
                          number(track->last_position.x).bits,
                          number(track->last_position.y).bits,
                          (uint32_t)track->latest_sprite,
                          (uint32_t)track->sprite_count,
                          track->world_coordinates,
                          track->velocity_angle};
        CHECK(
            compare_level_words(row, wanted, 7, frame, "trajectory", "follow", index));
    }
    for (size_t index = 0; index < camera->point_count; ++index) {
        const AbTrajectoryPoint *point = camera->points + index;
        uint32_t wanted[4];
        CHECK(read_body_words(file, wanted, 4));
        CHECK(point->object == camera->flying_value.table && point->layer == 2);
        uint32_t row[] = {(uint32_t)camera->flying + 1, number(point->position.x).bits,
                          number(point->position.y).bits, number(point->angle).bits};
        CHECK(compare_level_words(row, wanted, 4, frame, "trajectory", "point", index));
    }
    return level_camera_draw_points(camera) &&
           (!camera->compare_scene || level_scene_compare(file, camera, frame));
}

static bool level_camera_compare(FILE *file, LevelCamera *camera, size_t frame) {
    uint32_t expected[80], actual[80] = {0};
    CHECK(read_body_words(file, expected, 80));
    AbClassicCameraView view = ab_classic_camera_view(camera->owner);
    uint32_t target =
        view.target_object == SIZE_MAX ? 0 : (uint32_t)view.target_object + 1u;
    CHECK(camera_state_words(view.system, target, actual) &&
          camera_sequence_words(view.system.sequence, NULL, false, actual));
    /* No native viewport readback or retained inactive sequence stages in this
     * capture. Their independent constructed comparisons remain separate. */
    actual[43] = actual[44] = actual[45] = actual[51] = 0;
    CHECK(compare_level_words(actual, expected, 80, frame, "camera", "state", 0));
    return !camera->compare_trajectory || level_trajectory_compare(file, camera, frame);
}

static bool level_camera_initialize(FILE *file, LevelCamera *camera,
                                    AbClassicLevel *level) {
    uint32_t input[13];
    CHECK(camera && camera->sources[0].stages && camera->sources[1].stages &&
          read_body_words(file, input, 13) && input[8] < 23);
    camera->level = level;
    camera->flying = input[8];
    AbClassicLevelView view = ab_classic_level_view(level);
    if (camera->compare_scene) {
        uint32_t count;
        CHECK(read_body_words(file, &count, 1) && count == view.frame->count);
        /* The replay's manual physics-only settling does not run native render
         * history sampling/interpolation. Restore that observed starting cache;
         * body transforms and broadphase state remain independently simulated. */
        for (size_t index = 0; index < count; ++index) {
            uint32_t poses[9];
            CHECK(read_body_words(file, poses, 9));
            AbGameFrameObject *object = view.frame->objects + index;
            for (size_t sample = 0; sample < 2; ++sample)
                object->history[sample] = (AbBodyPose){
                    {from_bits(poses[sample * 3]), from_bits(poses[sample * 3 + 1])},
                    from_bits(poses[sample * 3 + 2])};
            object->rendered = (AbBodyPose){{from_bits(poses[6]), from_bits(poses[7])},
                                            from_bits(poses[8])};
        }
    }
    if (camera->compare_trajectory) {
        CHECK(camera->archive &&
              ab_sprite_atlas_open(camera->archive, view.sprites, 16 * 1024 * 1024,
                                   8 * 1024 * 1024, &camera->atlas, NULL));
        camera->provider = (AbSpriteProvider){.context = camera,
                                              .sprite = level_camera_sprite,
                                              .texture = level_camera_texture};
        AbClassicRenderSceneConfig render_config = {
            .provider = &camera->provider,
            .id_counter = &camera->scene_counter,
            .extra_renderers = 128,
            .storage_bytes = 128 * 1024,
            .object_blend = AB_BLEND_PREMULTIPLIED};
        CHECK(ab_classic_render_scene_open(level, render_config, &camera->render_scene,
                                           NULL) &&
              camera->scene_counter == 22);
    }
    camera->gravity = view.world->definition.gravity.y;
    AbVec2 pixels = {from_bits(input[0]), from_bits(input[1])};
    CHECK(ab_camera_museum_snaps(view.data->globals, pixels, camera->snaps, NULL));
    AbClassicCameraConfig config = {
        .level = level,
        .snaps = camera->snaps,
        .snap_count = 2,
        .pixel_size = pixels,
        .absolute_bottom = from_bits(input[2]),
        .horizontal_leeway = from_bits(input[3]),
        .gravity = &camera->gravity,
        .shot_sequence = camera->sources,
        .end_sequence = camera->sources + 1,
        .resource_context = camera,
        .load_sequence = level_camera_load,
        .sequence_log = level_camera_log,
        .prediction = {camera, 8192, level_camera_prediction, level_camera_ray},
        .storage_bytes = 128 * 1024};
    AbError error = {0};
    CHECK(ab_classic_camera_open(&config, &camera->owner, &error));
    camera->flying_value =
        (AbDataValue){.kind = AB_DATA_TABLE,
                      .table = &view.properties->objects[camera->flying].physical};
    camera->trajectory = (AbTrajectoryFollow){.tracks = camera->tracks,
                                              .capacity = 23,
                                              .context = camera,
                                              .emit = level_camera_point};
    AbClassicBehaviorConfig behavior_config = {.flying_bird = &camera->flying_value,
                                               .context = camera,
                                               .call = level_camera_behavior,
                                               .storage_bytes = 128 * 1024};
    CHECK(
        ab_classic_behaviors_open(level, behavior_config, &camera->behaviors, &error));
    AbCameraSystemInput initial = {
        .targets = {.values = {.position = {from_bits(input[4]), from_bits(input[5])},
                               .zoom = from_bits(input[6]),
                               .present = AB_CAMERA_VALUE_X | AB_CAMERA_VALUE_Y |
                                          AB_CAMERA_VALUE_ZOOM}},
        .targets_present = true};
    CHECK(ab_classic_camera_update(camera->owner, 0, &initial, &error));
    initial.targets.values.present = AB_CAMERA_VALUE_ZOOM;
    initial.targets.zoom_duration = 1;
    CHECK(ab_classic_camera_update(camera->owner, 1, &initial, &error));
    view.character_objects[camera->flying].jump.position =
        (AbVec2){from_bits(input[9]), from_bits(input[10])};
    CHECK(ab_level_properties_publish_position(
              view.properties, camera->flying,
              view.character_objects[camera->flying].jump.position, &error) &&
          ab_classic_camera_set_flying(camera->owner, camera->flying, &error) &&
          ab_classic_behaviors_launch(
              camera->behaviors, camera->flying,
              (AbVec2){from_bits(input[11]), from_bits(input[12])}, &error) &&
          camera->trajectory_adds == 1 && camera->trajectory.count == 1 &&
          ab_level_properties_publish_launch(
              view.properties, camera->flying,
              (AbVec2){from_bits(input[11]), from_bits(input[12])}, &error));
    AbCameraSystemInput resolved = {.moving_objects = true,
                                    .specialty_available = true};
    if (!ab_classic_camera_update(camera->owner, from_bits(input[7]), &resolved,
                                  &error)) {
        fprintf(stderr, "First-level camera initialization: %s\n", error.message);
        return false;
    }
    return level_camera_compare(file, camera, 0);
}

static bool level_camera_advance(FILE *file, LevelCamera *camera,
                                 const AbGameFrame *frame, float seconds,
                                 size_t frame_index) {
    uint32_t flags[2];
    CHECK(read_body_words(file, flags, 2) && flags[0] <= 1 && flags[1] <= 1);
    AbClassicLevelView level = ab_classic_level_view(camera->level);
    CHECK(level.frame->count == frame->count);
    memcpy(level.frame->objects, frame->objects,
           frame->count * sizeof(*frame->objects));
    level.frame->clock = frame->clock;
    level.frame->history_index = frame->history_index;
    for (size_t index = 0; index < level.bodies->count; ++index) {
        if (level.removed_objects[index])
            continue;
        AbObjectPropertyPlan plan;
        CHECK(
            ab_classic_level_property_plan(camera->level, index, -49, 89, &plan, NULL));
        if (plan.publish)
            CHECK(ab_classic_level_publish_properties(camera->level, index,
                                                      &plan.values, NULL));
        CHECK(ab_classic_level_finish_object(camera->level, index, NULL));
    }
    AbCameraSystemInput resolved = {.moving_objects = flags[0] != 0,
                                    .specialty_available = flags[1] != 0};
    AbError error = {0};
    if (camera->compare_trajectory &&
        !ab_trajectory_follow_update(&camera->trajectory, &error)) {
        fprintf(stderr, "First-level trajectory frame %zu: %s\n", frame_index,
                error.message);
        return false;
    }
    if (!ab_classic_camera_update(camera->owner, seconds, &resolved, &error)) {
        fprintf(stderr, "First-level camera frame %zu: %s\n", frame_index,
                error.message);
        return false;
    }
    return level_camera_compare(file, camera, frame_index + 1u);
}

#endif
