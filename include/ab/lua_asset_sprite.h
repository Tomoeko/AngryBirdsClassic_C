#ifndef AB_LUA_ASSET_SPRITE_H
#define AB_LUA_ASSET_SPRITE_H

#include "ab/object_sprite.h"
#include "ab/sprite_provider.h"

typedef struct {
    AbVec2 world_position, scale;
    float angle_radians, alpha;
    bool visible, use_alpha;
} AbLuaAssetSpritePose;

/* Resolved ordinary LuaAssetRenderer.draw sprite branch: imageSize.use and
 * drawString are false. Position is already in world units. A NULL region is
 * an empty asset; it and invisible renderers skip geometry/property reads.
 * Other class branches, property lookup/coercion and resource ownership remain
 * separate. No Lua or GPU calls, allocation, physics conversion or scene IDs.
 *
 * state and alpha are live input/output render state. Preserve direct_vertices;
 * publish position/scale/pivot/rotation and use_alpha ? alpha : 1 when active.
 * A skipped success writes submit=false and preserves rectangle/state/alpha.
 * An active success writes submit=true before later image culling. Finite used
 * numeric values, nonzero object scales and the supported rotation domain are
 * required. Invalid input preserves all outputs. Outputs must be disjoint.
 */
bool ab_lua_asset_sprite_state(const AbSpriteRegion *region, AbLuaAssetSpritePose pose,
                               AbCoordinateCamera camera, AbImageRectangle *rectangle,
                               AbSpriteRenderState *state, float *alpha, bool *submit,
                               AbError *error);

typedef struct {
    const AbSpriteRegion *region;
    float uv[8];
    uint32_t texture;
    bool has_alpha;
} AbLuaAssetSpriteBinding;

/* Cache an ordinary sprite/UV/texture selection through the shared provider.
 * Empty or missing names produce an empty asset. Active composites and unknown
 * image alpha policy reject explicitly. Resources/region and uploaded texture
 * remain borrowed; discard/rebind before releasing them or changing source
 * groups. Name bytes are used only during lookup. No allocation, image decoding
 * or GPU calls. Invalid/unsupported input preserves output.
 */
bool ab_lua_asset_sprite_bind(const AbSpriteProvider *provider, AbSpan name,
                              AbLuaAssetSpriteBinding *output, AbError *error);

/* Compose the resolved ordinary branch and native image rectangle/material path
 * into a Common command. Pose is explicit; constructor defaults, property reads,
 * scene membership and GPU submission stay with their owners. Empty/invisible
 * assets skip before geometry reads. Culled success sets visible=false and
 * preserves command. Failure preserves both outputs. No lookup or allocation.
 */
bool ab_lua_asset_sprite_command(const AbLuaAssetSpriteBinding *binding,
                                 AbLuaAssetSpritePose pose, AbCoordinateCamera camera,
                                 bool direct_vertices, uint32_t viewport_width,
                                 uint32_t viewport_height, AbDrawCommand *command,
                                 bool *visible, AbError *error);

#endif
