#include "ab/game_sprite.h"

#include <math.h>

bool ab_game_sprite_command(const AbLuaAssetSpriteBinding *binding,
                            AbGameSpritePose pose, AbCoordinateCamera camera,
                            uint32_t width, uint32_t height, AbDrawCommand *command,
                            bool *visible, AbError *error) {
    if (!binding || !command || !visible)
        return ab_fail(error, "Missing game sprite command output/binding");
    if (!binding->region) {
        *visible = false;
        return true;
    }
    if (!isfinite(pose.physics_scale) || pose.physics_scale == 0 ||
        !isfinite(pose.physics_position.x) || !isfinite(pose.physics_position.y) ||
        !isfinite(pose.scale.x) || !isfinite(pose.scale.y))
        return ab_fail(error, "Invalid game sprite position/scale");
    const AbSpriteRegion *region = binding->region;
    float pivot_x = (float)(int32_t)((float)region->pivot_x + 0.5f) * pose.scale.x;
    float pivot_y = (float)(int32_t)((float)region->pivot_y + 0.5f) * pose.scale.y;
    AbImageRectangle rectangle = {
        pose.physics_position.x / pose.physics_scale - pivot_x,
        pose.physics_position.y / pose.physics_scale - pivot_y,
        (float)region->width * pose.scale.x, (float)region->height * pose.scale.y};
    AbSpriteRenderState state = {.angle_radians = pose.angle,
                                 .translation_x = -camera.top_left.x,
                                 .translation_y = -camera.top_left.y,
                                 .scale_x = camera.zoom.x,
                                 .scale_y = camera.zoom.y,
                                 .rotation_origin_x = pivot_x,
                                 .rotation_origin_y = pivot_y};
    return ab_image_rectangle_command(
        rectangle, state, width, height, binding->uv,
        (AbImageRectangleMaterial){binding->texture, binding->has_alpha, pose.alpha},
        command, visible, error);
}
