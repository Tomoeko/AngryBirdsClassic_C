#include "ab/classic_render_scene.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct AbClassicRenderScene {
    AbClassicLevel *level;
    AbClassicScene *objects;
    AbClassicRenderSceneConfig config;
    AbSceneMembership membership;
    AbClassicRenderRoute *routes;
    AbSceneId *initial_ids;
    AbLuaAssetRenderer *extras;
    bool *initial_live;
    size_t initial_count, object_count, object_capacity, extra_count, bytes;
};

bool ab_classic_render_scene_update_gfx(AbClassicRenderScene *scene, float seconds,
                                        AbError *error) {
    (void)seconds;
    if (!scene || scene->membership.count > scene->membership.capacity)
        return ab_fail(error, "Invalid ordinary scene GFX update");
    for (size_t index = 0; index < scene->membership.count; ++index) {
        AbClassicRenderRoute route = scene->routes[index];
        if (route.kind == AB_CLASSIC_RENDER_OBJECT) {
            if (route.index >= scene->object_count || !scene->initial_live[route.index])
                return ab_fail(error, "Stale ordinary object GFX membership");
        } else if (route.kind != AB_CLASSIC_RENDER_LUA ||
                   route.index >= scene->extra_count) {
            return ab_fail(error, "Unknown ordinary scene GFX renderer");
        }
        /* Native GameObjectRenderer.update has neither effect here; the only
         * other owned renderer class, LuaAssetRenderer, has an empty update. */
    }
    return true;
}

static bool reserve_bytes(size_t count, size_t size, size_t limit, size_t *bytes) {
    if (*bytes > limit || count > (limit - *bytes) / size)
        return false;
    *bytes += count * size;
    return true;
}

static bool is_ground(AbSpan name) {
    return name.size == 6 && memcmp(name.data, "ground", 6) == 0;
}

static void insert_route(AbClassicRenderScene *scene, const void *identity,
                         AbClassicRenderRoute route) {
    size_t index = 0;
    /* Only this owner inserts unique identities. Numeric IDs may still alias. */
    while (scene->membership.items[index].object != identity)
        ++index;
    memmove(scene->routes + index + 1, scene->routes + index,
            (scene->membership.count - index - 1) * sizeof(*scene->routes));
    scene->routes[index] = route;
}

static bool remove_id(AbClassicRenderScene *scene, int32_t id, bool *found,
                      AbError *error) {
    size_t index = 0;
    while (index < scene->membership.count && scene->membership.items[index].id != id)
        ++index;
    AbSceneMember removed;
    if (!ab_scene_membership_remove_id(&scene->membership, id, &removed, found, error))
        return false;
    if (!*found)
        return true;
    AbClassicRenderRoute route = scene->routes[index];
    if (route.kind == AB_CLASSIC_RENDER_OBJECT)
        scene->initial_live[route.index] = false;
    memmove(scene->routes + index, scene->routes + index + 1,
            (scene->membership.count - index) * sizeof(*scene->routes));
    return true;
}

static bool initial_members(AbClassicRenderScene *scene, AbError *error) {
    AbClassicSceneView objects = ab_classic_scene_view(scene->objects);
    AbClassicLevelView level = ab_classic_level_view(scene->level);
    for (size_t index = 0; index < scene->initial_count; ++index) {
        if (is_ground(level.bodies->objects[index].name))
            continue;
        if (objects.items[index].collider == 7)
            return ab_fail(error, "Collider 7 scene construction remains separate");
        if (!ab_scene_id_next(scene->config.id_counter, scene->initial_ids + index,
                              error))
            return false;
        AbSceneMember member = {(void *)(objects.items + index),
                                scene->initial_ids[index].value,
                                objects.items[index].layer};
        if (!ab_scene_membership_insert(&scene->membership, member, error))
            return false;
        insert_route(scene, member.object,
                     (AbClassicRenderRoute){AB_CLASSIC_RENDER_OBJECT, index});
        scene->initial_live[index] = true;
    }
    return true;
}

bool ab_classic_render_scene_open(AbClassicLevel *level,
                                  AbClassicRenderSceneConfig config,
                                  AbClassicRenderScene **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing classic render scene output");
    *output = NULL;
    AbClassicLevelView view = ab_classic_level_view(level);
    if (!view.bodies || !view.frame || !config.provider || !config.id_counter ||
        view.bodies->count > view.bodies->capacity ||
        view.bodies->capacity > SIZE_MAX - config.extra_renderers)
        return ab_fail(error, "Invalid classic render scene input");
    size_t count = view.bodies->count;
    size_t object_capacity = view.bodies->capacity;
    size_t capacity = object_capacity + config.extra_renderers;
    size_t bytes = sizeof(AbClassicRenderScene);
    if (!reserve_bytes(capacity, sizeof(AbSceneMember) + sizeof(AbClassicRenderRoute),
                       config.storage_bytes, &bytes) ||
        !reserve_bytes(object_capacity, sizeof(AbSceneId) + sizeof(bool),
                       config.storage_bytes, &bytes) ||
        !reserve_bytes(config.extra_renderers, sizeof(AbLuaAssetRenderer),
                       config.storage_bytes, &bytes))
        return ab_fail(error, "Classic render scene exceeds storage limit");
    AbClassicRenderScene *scene = calloc(1, sizeof(*scene));
    if (!scene)
        return ab_fail(error, "Cannot allocate classic render scene");
    scene->config = config;
    scene->level = level;
    scene->initial_count = count;
    scene->object_count = count;
    scene->object_capacity = object_capacity;
    scene->bytes = bytes;
    scene->membership.capacity = capacity;
    if (capacity) {
        scene->membership.items = calloc(capacity, sizeof(*scene->membership.items));
        scene->routes = calloc(capacity, sizeof(*scene->routes));
        if (!scene->membership.items || !scene->routes)
            goto allocation_failed;
    }
    if (object_capacity) {
        scene->initial_ids = calloc(object_capacity, sizeof(*scene->initial_ids));
        scene->initial_live = calloc(object_capacity, sizeof(*scene->initial_live));
        if (!scene->initial_ids || !scene->initial_live)
            goto allocation_failed;
    }
    if (config.extra_renderers) {
        scene->extras = calloc(config.extra_renderers, sizeof(*scene->extras));
        if (!scene->extras)
            goto allocation_failed;
    }
    if (!ab_classic_scene_open_resources(level, config.provider, config.object_blend,
                                         config.storage_bytes - bytes, &scene->objects,
                                         error))
        goto release_scene;
    scene->bytes += ab_classic_scene_bytes(scene->objects);
    if (!initial_members(scene, error))
        goto release_scene;
    *output = scene;
    return true;

allocation_failed:
    ab_fail(error, "Cannot allocate classic render scene arrays");
release_scene:
    ab_classic_render_scene_close(scene);
    return false;
}

void ab_classic_render_scene_close(AbClassicRenderScene *scene) {
    if (!scene)
        return;
    ab_classic_scene_close(scene->objects);
    free(scene->membership.items);
    free(scene->routes);
    free(scene->initial_ids);
    free(scene->initial_live);
    free(scene->extras);
    free(scene);
}

size_t ab_classic_render_scene_bytes(const AbClassicRenderScene *scene) {
    return scene ? scene->bytes : 0;
}

AbClassicScene *ab_classic_render_scene_objects(AbClassicRenderScene *scene) {
    return scene ? scene->objects : NULL;
}

AbClassicRenderSceneView
ab_classic_render_scene_view(const AbClassicRenderScene *scene) {
    if (!scene)
        return (AbClassicRenderSceneView){0};
    return (AbClassicRenderSceneView){scene->membership.items, scene->routes,
                                      scene->initial_ids,      scene->extras,
                                      scene->membership.count, scene->initial_count,
                                      scene->extra_count,      scene->object_count};
}

bool ab_classic_render_scene_create_object(AbClassicRenderScene *scene,
                                           AbClassicVisuals *visuals, size_t object,
                                           AbSpan sprite, float layer, AbError *error) {
    AbClassicVisualView view = ab_classic_visuals_view(visuals);
    if (!scene || view.level != scene->level ||
        view.provider != scene->config.provider || view.count != scene->object_count ||
        object > scene->object_count || object >= scene->object_capacity ||
        (object < scene->object_count && scene->initial_live[object]) ||
        !isfinite(layer) || layer < -2147483648.0f || layer >= 2147483648.0f ||
        (sprite.size && !sprite.data))
        return ab_fail(error, "Invalid dynamic ordinary renderer binding");
    if (!ab_scene_id_next(scene->config.id_counter, scene->initial_ids + object,
                          error) ||
        !(object == scene->object_count
              ? ab_classic_scene_append_object(scene->objects, object, layer, error)
              : ab_classic_scene_recreate_object(scene->objects, object, layer, error)))
        return false;
    AbClassicSceneView objects = ab_classic_scene_view(scene->objects);
    AbSceneMember member = {(void *)(objects.items + object),
                            scene->initial_ids[object].value, (int32_t)layer};
    if (!ab_scene_membership_insert(&scene->membership, member, error))
        return false;
    insert_route(scene, member.object,
                 (AbClassicRenderRoute){AB_CLASSIC_RENDER_OBJECT, object});
    scene->initial_live[object] = true;
    if (object == scene->object_count)
        ++scene->object_count;
    return ab_classic_visuals_attach_object(visuals, object, error) &&
           ab_classic_visuals_set_sprite(visuals, object, sprite, error) &&
           ab_classic_scene_sync_visuals(scene->objects, visuals, error);
}

bool ab_classic_render_scene_create_lua(AbClassicRenderScene *scene, AbSpan name,
                                        float layer, AbLuaAssetRenderer **output,
                                        AbError *error) {
    if (!output)
        return ab_fail(error, "Missing classic Lua renderer output");
    *output = NULL;
    if (!scene || scene->extra_count == scene->config.extra_renderers)
        return ab_fail(error, "Classic extra renderer storage is exhausted");
    AbLuaAssetRenderer *renderer = scene->extras + scene->extra_count;
    if (!ab_lua_asset_renderer_create(scene->config.provider, name, layer,
                                      scene->config.id_counter, &scene->membership,
                                      renderer, error))
        return false;
    insert_route(scene, renderer,
                 (AbClassicRenderRoute){AB_CLASSIC_RENDER_LUA, scene->extra_count});
    ++scene->extra_count;
    *output = renderer;
    return true;
}

bool ab_classic_render_scene_remove_number(AbClassicRenderScene *scene, float id,
                                           bool *found, AbError *error) {
    if (!scene || !found)
        return ab_fail(error, "Invalid classic scene numeric removal");
    return remove_id(scene, ab_scene_id_integer(id), found, error);
}

AbClassicLevel *ab_classic_render_scene_level(const AbClassicRenderScene *scene) {
    return scene ? scene->level : NULL;
}

bool ab_classic_render_scene_remove_object(AbClassicRenderScene *scene, size_t object,
                                           bool *found, AbError *error) {
    if (!scene || !found || object >= scene->object_count)
        return ab_fail(error, "Unknown classic renderer object removal");
    AbClassicSceneView view = ab_classic_scene_view(scene->objects);
    void *identity = (void *)(view.items + object);
    size_t index = 0;
    while (index < scene->membership.count &&
           scene->membership.items[index].object != identity)
        ++index;
    AbSceneMember removed;
    if (!ab_scene_membership_remove_object(&scene->membership, identity,
                                           view.items[object].layer, &removed, found,
                                           error))
        return false;
    if (*found) {
        scene->initial_live[object] = false;
        memmove(scene->routes + index, scene->routes + index + 1,
                (scene->membership.count - index) * sizeof(*scene->routes));
    }
    return true;
}

bool ab_classic_render_scene_sync(AbClassicRenderScene *scene, AbError *error) {
    if (!scene)
        return ab_fail(error, "Missing classic render scene");
    AbClassicLevelView view = ab_classic_level_view(scene->level);
    for (size_t index = 0; index < scene->object_count; ++index) {
        if (!scene->initial_live[index] || !view.removed_objects[index])
            continue;
        bool found;
        if (!remove_id(scene, scene->initial_ids[index].value, &found, error) || !found)
            return ab_fail(error, "Forgotten classic renderer is absent from scene");
    }
    return ab_classic_scene_sync_poses(scene->objects, error);
}

bool ab_classic_render_scene_draw(const AbClassicRenderScene *scene,
                                  AbCoordinateCamera camera, uint32_t viewport_width,
                                  uint32_t viewport_height, AbDrawList *list,
                                  AbError *error) {
    if (!scene || !list || list->count > list->capacity ||
        (list->capacity && !list->commands) || !viewport_width || !viewport_height)
        return ab_fail(error, "Invalid classic render scene draw bindings");
    AbClassicSceneView objects = ab_classic_scene_view(scene->objects);
    size_t original_count = list->count;
    const size_t first = 0;
    for (size_t index = 0; index < scene->membership.count; ++index) {
        AbClassicRenderRoute route = scene->routes[index];
        if (route.kind == AB_CLASSIC_RENDER_OBJECT) {
            if (!ab_sprite_scene_draw(objects.items + route.index, 1, &first, camera,
                                      viewport_width, viewport_height, list, error))
                goto restore_list;
        } else {
            AbDrawCommand command = {0};
            bool visible;
            if (!ab_lua_asset_renderer_command(scene->extras + route.index, camera,
                                               false, viewport_width, viewport_height,
                                               &command, &visible, error) ||
                (visible && !ab_draw_list_push(list, &command, error)))
                goto restore_list;
        }
    }
    return true;

restore_list:
    list->count = original_count;
    return false;
}
