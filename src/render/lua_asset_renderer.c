#include "ab/lua_asset_renderer.h"

bool ab_lua_asset_renderer_set_sprite(AbLuaAssetRenderer *renderer,
                                      const AbSpriteProvider *provider, AbSpan name,
                                      AbError *error) {
    if (!renderer || !provider || !provider->sprite || !provider->texture ||
        (name.size && !name.data))
        return ab_fail(error, "Invalid Lua asset renderer sprite assignment");
    renderer->sprite_name = name;
    renderer->sprite = (AbLuaAssetSpriteBinding){0};
    return ab_lua_asset_sprite_bind(provider, name, &renderer->sprite, error);
}

bool ab_lua_asset_renderer_create(const AbSpriteProvider *provider, AbSpan sprite,
                                  float layer, uint32_t *counter,
                                  AbSceneMembership *scene, AbLuaAssetRenderer *output,
                                  AbError *error) {
    if (!provider || !provider->sprite || !provider->texture ||
        (sprite.size && !sprite.data) || !counter || !scene || !scene->items ||
        scene->count >= scene->capacity ||
        scene->capacity > SIZE_MAX / sizeof(*scene->items) || !output)
        return ab_fail(error, "Invalid or exhausted Lua asset renderer storage");
    AbLuaAssetRenderer result = {.layer = ab_scene_id_integer(layer),
                                 .pose = {.scale = {1, 1}, .alpha = 1, .visible = true},
                                 .string_scale = 1};
    if (!ab_scene_id_next(counter, &result.id, error) ||
        !ab_lua_asset_renderer_set_sprite(&result, provider, sprite, error))
        return false;
    AbSceneMember member = {output, result.id.value, result.layer};
    if (!ab_scene_membership_insert(scene, member, error))
        return false;
    *output = result;
    return true;
}

bool ab_lua_asset_renderer_command(const AbLuaAssetRenderer *renderer,
                                   AbCoordinateCamera camera, bool direct_vertices,
                                   uint32_t viewport_width, uint32_t viewport_height,
                                   AbDrawCommand *command, bool *visible,
                                   AbError *error) {
    if (!renderer || !command || !visible)
        return ab_fail(error, "Missing Lua asset renderer command bindings");
    if (!renderer->pose.visible || !renderer->sprite.region) {
        *visible = false;
        return true;
    }
    if (renderer->image_size_use || renderer->draw_string)
        return ab_fail(error,
                       "Lua asset custom-size/string draw is not recovered here");
    return ab_lua_asset_sprite_command(&renderer->sprite, renderer->pose, camera,
                                       direct_vertices, viewport_width, viewport_height,
                                       command, visible, error);
}
