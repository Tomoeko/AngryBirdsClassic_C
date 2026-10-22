#ifndef AB_TEST_CLASSIC_RENDER_OWNER_H
#define AB_TEST_CLASSIC_RENDER_OWNER_H

#include "ab/classic_render_scene.h"

typedef struct {
    const AbSpriteAtlas *atlas;
    size_t sprite_queries, texture_queries;
    bool fail_texture;
} RenderOwnerResources;

static bool render_owner_sprite(void *context, AbSpan name,
                                AbSpriteAtlasBinding *output) {
    RenderOwnerResources *resources = context;
    ++resources->sprite_queries;
    return ab_sprite_atlas_find(resources->atlas, name, output);
}

static bool render_owner_texture(void *context, const AbImage *image,
                                 uint32_t *output) {
    RenderOwnerResources *resources = context;
    ++resources->texture_queries;
    if (resources->fail_texture)
        return false;
    for (size_t index = 0; index < ab_sprite_atlas_count(resources->atlas); ++index) {
        AbSpriteAtlasImage view;
        if (ab_sprite_atlas_image(resources->atlas, index, &view) &&
            view.image == image) {
            *output = (uint32_t)index + 1;
            return true;
        }
    }
    return false;
}

static bool render_owner_checks(AbClassicLevel *level, const AbSpriteAtlas *atlas) {
    RenderOwnerResources resources = {.atlas = atlas};
    AbSpriteProvider provider = {.context = &resources,
                                 .sprite = render_owner_sprite,
                                 .texture = render_owner_texture};
    uint32_t counter = 0;
    AbClassicRenderSceneConfig config = {.provider = &provider,
                                         .id_counter = &counter,
                                         .extra_renderers = 3,
                                         .storage_bytes = 16384,
                                         .object_blend = AB_BLEND_PREMULTIPLIED};
    AbClassicRenderScene *scene = NULL;
    CHECK(!ab_classic_render_scene_open(NULL, config, &scene, NULL) && !scene);
    config.storage_bytes = 1;
    CHECK(!ab_classic_render_scene_open(level, config, &scene, NULL) && !scene &&
          !counter);
    config.storage_bytes = 16384;
    config.extra_renderers = SIZE_MAX;
    CHECK(!ab_classic_render_scene_open(level, config, &scene, NULL) && !scene &&
          !counter);
    config.extra_renderers = 3;
    CHECK(ab_classic_render_scene_open(level, config, &scene, NULL));
    size_t bytes = ab_classic_render_scene_bytes(scene);
    CHECK(bytes <= config.storage_bytes && counter == 22);
    ab_classic_render_scene_close(scene);
    scene = NULL;
    counter = 0;
    config.storage_bytes = bytes - 1;
    CHECK(!ab_classic_render_scene_open(level, config, &scene, NULL) && !scene &&
          !counter);
    config.storage_bytes = bytes;
    CHECK(ab_classic_render_scene_open(level, config, &scene, NULL) &&
          ab_classic_render_scene_bytes(scene) == bytes && counter == 22);
    AbClassicRenderSceneView view = ab_classic_render_scene_view(scene);
    CHECK(view.count == 22 && view.initial_count == 23 && !view.extra_count);
    AbLuaAssetRenderer *first = NULL, *second = NULL, *third = NULL, *failed = NULL;
    CHECK(ab_classic_render_scene_create_lua(scene, string("TRAIL_WHITE_1"), 2.75f,
                                             &first, NULL) &&
          first->id.value == 23);
    resources.fail_texture = true;
    CHECK(!ab_classic_render_scene_create_lua(scene, string("TRAIL_WHITE_2"), 2,
                                              &failed, NULL) &&
          !failed && counter == 24);
    CHECK(ab_classic_render_scene_view(scene).count == 23 &&
          ab_classic_render_scene_view(scene).extra_count == 1);
    resources.fail_texture = false;
    CHECK(ab_classic_render_scene_create_lua(scene, string("TRAIL_WHITE_2"), -10,
                                             &second, NULL) &&
          second->id.value == 25);
    CHECK(ab_classic_render_scene_create_lua(scene, string("TRAIL_WHITE_3"), 4, &third,
                                             NULL) &&
          third->id.value == 26);
    first->pose.world_position = (AbVec2){32, 32};
    second->pose.world_position = (AbVec2){64, 32};
    third->pose.world_position = (AbVec2){96, 32};
    view = ab_classic_render_scene_view(scene);
    CHECK(view.count == 25 && view.members[0].object == second &&
          view.routes[0].kind == AB_CLASSIC_RENDER_LUA && view.routes[0].index == 1);
    bool found = true;
    CHECK(ab_classic_render_scene_remove_number(scene, 999, &found, NULL) && !found);
    CHECK(ab_classic_render_scene_remove_number(scene, first->id.script_number, &found,
                                                NULL) &&
          found);
    CHECK(first->id.value == 23 && ab_classic_render_scene_view(scene).count == 24);
    CHECK(!ab_classic_render_scene_create_lua(scene, string("TRAIL_WHITE_1"), 2,
                                              &failed, NULL) &&
          !failed && counter == 26);
    CHECK(ab_classic_render_scene_remove_number(
              scene, view.initial_ids[1].script_number, &found, NULL) &&
          found);
    CHECK(ab_classic_render_scene_sync(scene, NULL) &&
          ab_classic_render_scene_view(scene).count == 23);
    size_t queries = resources.sprite_queries,
           texture_queries = resources.texture_queries;
    AbDrawCommand commands[128] = {{.texture = 99}};
    AbDrawList list = {commands, 1, 128};
    AbCoordinateCamera camera = {.zoom = {1, 1}};
    CHECK(ab_classic_render_scene_draw(scene, camera, 480, 320, &list, NULL) &&
          list.count >= 3 && commands[0].texture == 99 &&
          resources.sprite_queries == queries &&
          resources.texture_queries == texture_queries);
    list.count = 1;
    third->draw_string = true;
    CHECK(!ab_classic_render_scene_draw(scene, camera, 480, 320, &list, NULL) &&
          list.count == 1 && commands[0].texture == 99);
    third->draw_string = false;
    list.capacity = 1;
    CHECK(!ab_classic_render_scene_draw(scene, camera, 480, 320, &list, NULL) &&
          list.count == 1 && commands[0].texture == 99);
    CHECK(!ab_classic_render_scene_sync(NULL, NULL));
    CHECK(!ab_classic_scene_sync_poses(NULL, NULL));
    ab_classic_render_scene_close(scene);
    ab_classic_render_scene_close(NULL);
    puts("Shared classic renderer ownership, capacity, failure effects, routing and "
         "draw rollback pass");
    return true;
}

#endif
