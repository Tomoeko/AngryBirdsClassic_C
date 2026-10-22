#ifndef AB_LUA_ASSET_RENDERER_H
#define AB_LUA_ASSET_RENDERER_H

#include "ab/lua_asset_sprite.h"
#include "ab/scene_id.h"
#include "ab/scene_membership.h"

typedef struct {
    AbSceneId id;
    int32_t layer;
    AbSpan sprite_name;
    AbLuaAssetSpriteBinding sprite;
    AbLuaAssetSpritePose pose;
    bool image_size_use, draw_string;
    float image_size_w, image_size_h, string_scale;
    AbSpan string;
} AbLuaAssetRenderer;

/* Construct the typed ordinary renderer and register its stable identity.
 * Counter is application-owned; scene membership and caller-owned renderer
 * storage remain at fixed addresses. Name, region and texture stay borrowed.
 * The native script-facing fields have a typed C projection here; setSprite is
 * the C method below, not a Lua function/VM. The constructor's imageSize.w/h
 * defaults do not supply the custom draw branch's absent width/height fields.
 *
 * Validate storage/capacity before consuming an ID. Resource-boundary failures
 * after that consume the ID but preserve renderer/membership output. No
 * allocation, GPU work or generic property coercion. Do not construct over a
 * renderer still registered or borrowed. Bindings/output/counter are disjoint.
 */
bool ab_lua_asset_renderer_create(const AbSpriteProvider *provider, AbSpan sprite,
                                  float layer, uint32_t *counter,
                                  AbSceneMembership *scene, AbLuaAssetRenderer *output,
                                  AbError *error);

/* Store the borrowed name and clear the old asset before lookup. Missing names
 * leave an empty asset; later unsupported-resource failures retain the name and
 * empty binding. Provider and name remain valid throughout the call. */
bool ab_lua_asset_renderer_set_sprite(AbLuaAssetRenderer *renderer,
                                      const AbSpriteProvider *provider, AbSpan name,
                                      AbError *error);

/* Hidden/empty ordinary renderers skip before unsupported branch checks.
 * imageSize.use and drawString require separately recovered draw paths and
 * reject when reached. Produces one resolved command without allocation or
 * resource lookup; normal culling/output guarantees match the sprite adapter.
 */
bool ab_lua_asset_renderer_command(const AbLuaAssetRenderer *renderer,
                                   AbCoordinateCamera camera, bool direct_vertices,
                                   uint32_t viewport_width, uint32_t viewport_height,
                                   AbDrawCommand *command, bool *visible,
                                   AbError *error);

#endif
