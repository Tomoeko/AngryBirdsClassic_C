#include "ab/lua_asset_sprite.h"
#include "ab/rotation.h"

#include <math.h>

bool ab_lua_asset_sprite_state(const AbSpriteRegion *region, AbLuaAssetSpritePose pose,
                               AbCoordinateCamera camera, AbImageRectangle *rectangle,
                               AbSpriteRenderState *state, float *alpha, bool *submit,
                               AbError *error) {
    if (!rectangle || !state || !alpha || !submit)
        return ab_fail(error, "Missing Lua asset sprite state outputs");
    if (!region || !pose.visible) {
        *submit = false;
        return true;
    }
    AbRotation rotation;
    float opacity = pose.use_alpha ? pose.alpha : 1;
    if (!isfinite(pose.world_position.x) || !isfinite(pose.world_position.y) ||
        !isfinite(pose.scale.x) || !isfinite(pose.scale.y) || pose.scale.x == 0 ||
        pose.scale.y == 0 || !isfinite(camera.top_left.x) ||
        !isfinite(camera.top_left.y) || !isfinite(camera.zoom.x) ||
        !isfinite(camera.zoom.y) || !isfinite(opacity) ||
        !ab_rotation_from_angle(pose.angle_radians, &rotation))
        return ab_fail(error, "Unsupported Lua asset sprite geometry or alpha");
    float pivot_x = (float)(int32_t)((float)region->pivot_x + 0.5f);
    float pivot_y = (float)(int32_t)((float)region->pivot_y + 0.5f);
    AbSpriteRenderState result = {
        .angle_radians = pose.angle_radians,
        .translation_x = (pose.world_position.x + -camera.top_left.x) / pose.scale.x,
        .translation_y = (pose.world_position.y + -camera.top_left.y) / pose.scale.y,
        .scale_x = pose.scale.x * camera.zoom.x,
        .scale_y = pose.scale.y * camera.zoom.y,
        .rotation_origin_x = pivot_x,
        .rotation_origin_y = pivot_y,
        .direct_vertices = state->direct_vertices};
    if (!isfinite(result.translation_x) || !isfinite(result.translation_y) ||
        !isfinite(result.scale_x) || !isfinite(result.scale_y))
        return ab_fail(error, "Lua asset sprite state exceeds finite range");
    *rectangle = (AbImageRectangle){0.0f - pivot_x, 0.0f - pivot_y,
                                    (float)region->width, (float)region->height};
    *state = result;
    *alpha = opacity;
    *submit = true;
    return true;
}

bool ab_lua_asset_sprite_bind(const AbSpriteProvider *provider, AbSpan name,
                              AbLuaAssetSpriteBinding *output, AbError *error) {
    if (!provider || !provider->sprite || !provider->texture || !output ||
        (name.size && !name.data))
        return ab_fail(error, "Invalid Lua asset sprite resource binding");
    AbLuaAssetSpriteBinding result = {0};
    AbSpriteAtlasBinding binding = {0};
    bool found = name.size && provider->sprite(provider->context, name, &binding);
    if (!found) {
        AbResolvedComposite composite;
        if (name.size && provider->composite &&
            provider->composite(provider->context, name, &composite))
            return ab_fail(error, "Lua asset composite renderer is not supported here");
        *output = result;
        return true;
    }
    if (!binding.image || !binding.sprite.region ||
        (binding.alpha != AB_SPRITE_ALPHA_OPAQUE &&
         binding.alpha != AB_SPRITE_ALPHA_PRESENT) ||
        !provider->texture(provider->context, binding.image, &result.texture) ||
        !result.texture)
        return ab_fail(error,
                       "Missing Lua asset sprite image or texture/alpha binding");
    AbDrawQuad quad;
    AbSpritePlacement placement = {0, 0, 1, 1, AB_ANCHOR_TOP, AB_ANCHOR_LEFT};
    if (!ab_sprite_quad(binding.sprite.region, binding.image->width,
                        binding.image->height, placement, &quad, error))
        return false;
    result.region = binding.sprite.region;
    result.has_alpha = binding.alpha == AB_SPRITE_ALPHA_PRESENT;
    for (size_t index = 0; index < 4; ++index) {
        result.uv[index * 2] = quad.vertices[index].u;
        result.uv[index * 2 + 1] = quad.vertices[index].v;
    }
    *output = result;
    return true;
}

bool ab_lua_asset_sprite_command(const AbLuaAssetSpriteBinding *binding,
                                 AbLuaAssetSpritePose pose, AbCoordinateCamera camera,
                                 bool direct_vertices, uint32_t viewport_width,
                                 uint32_t viewport_height, AbDrawCommand *command,
                                 bool *visible, AbError *error) {
    if (!binding || !command || !visible)
        return ab_fail(error, "Missing Lua asset sprite command bindings");
    AbImageRectangle rectangle = {0};
    AbSpriteRenderState state = {.direct_vertices = direct_vertices};
    float alpha = 1;
    bool submit = false;
    if (!ab_lua_asset_sprite_state(binding->region, pose, camera, &rectangle, &state,
                                   &alpha, &submit, error))
        return false;
    if (!submit) {
        *visible = false;
        return true;
    }
    AbImageRectangleMaterial material = {binding->texture, binding->has_alpha, alpha};
    return ab_image_rectangle_command(rectangle, state, viewport_width, viewport_height,
                                      binding->uv, material, command, visible, error);
}
