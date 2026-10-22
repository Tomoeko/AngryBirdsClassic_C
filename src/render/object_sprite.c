#include "ab/object_sprite.h"
#include "ab/rotation.h"

#include <math.h>

bool ab_object_sprite_state(const AbSpriteRegion *region, AbObjectSpritePose pose,
                            AbCoordinateCamera camera, AbImageRectangle *rectangle,
                            AbSpriteRenderState *state, AbError *error) {
    AbRotation rotation;
    if (!region || !rectangle || !state || !isfinite(pose.physics_position.x) ||
        !isfinite(pose.physics_position.y) || !isfinite(pose.scale.x) ||
        !isfinite(pose.scale.y) || pose.scale.x == 0 || pose.scale.y == 0 ||
        !isfinite(camera.top_left.x) || !isfinite(camera.top_left.y) ||
        !isfinite(camera.zoom.x) || !isfinite(camera.zoom.y) ||
        !ab_rotation_from_angle(pose.angle_radians, &rotation)) {
        return ab_fail(error, "Invalid ordinary object sprite state");
    }
    float pivot_x = (float)(int32_t)((float)region->pivot_x + 0.5f);
    float pivot_y = (float)(int32_t)((float)region->pivot_y + 0.5f);
    float x = -camera.top_left.x;
    float y = -camera.top_left.y;
    x += 20.0f * pose.physics_position.x;
    y += 20.0f * pose.physics_position.y;
    AbSpriteRenderState result = {.angle_radians = pose.angle_radians,
                                  .translation_x = x / pose.scale.x,
                                  .translation_y = y / pose.scale.y,
                                  .scale_x = camera.zoom.x * pose.scale.x,
                                  .scale_y = camera.zoom.y * pose.scale.y,
                                  .rotation_origin_x = pivot_x,
                                  .rotation_origin_y = pivot_y};
    if (!isfinite(result.translation_x) || !isfinite(result.translation_y) ||
        !isfinite(result.scale_x) || !isfinite(result.scale_y)) {
        return ab_fail(error, "Ordinary object sprite state exceeds finite range");
    }
    *rectangle = (AbImageRectangle){0.0f - pivot_x, 0.0f - pivot_y,
                                    (float)region->width, (float)region->height};
    *state = result;
    return true;
}

static AbTransform2 multiply(AbTransform2 left, AbTransform2 right) {
    /* Keep both products, including identity terms and signed zero. */
    AbTransform2 result;
    result.xx = left.xy * right.yx + left.xx * right.xx;
    result.xy = left.xy * right.yy + left.xx * right.xy;
    result.tx = left.xy * right.ty + left.xx * right.tx;
    result.tx += left.tx;
    result.yx = right.yx * left.yy + right.xx * left.yx;
    result.yy = right.yy * left.yy + right.xy * left.yx;
    result.ty = right.ty * left.yy + right.tx * left.yx;
    result.ty += left.ty;
    return result;
}

bool ab_object_sprite_quad(const AbSpriteRegion *region, uint32_t texture_width,
                           uint32_t texture_height, AbObjectSpritePlacement placement,
                           AbDrawQuad *output, AbError *error) {
    AbRotation rotation;
    if (!region || !output || !isfinite(placement.x) || !isfinite(placement.y) ||
        !isfinite(placement.horizontal_multiplier) || !isfinite(placement.zoom) ||
        !isfinite(placement.scale_x) || !isfinite(placement.scale_y) ||
        !isfinite(placement.origin_x) || !isfinite(placement.origin_y) ||
        !ab_rotation_from_angle(placement.angle_radians, &rotation)) {
        return ab_fail(error, "Invalid object sprite placement");
    }
    AbDrawQuad local;
    AbSpritePlacement sprite = {0, 0, 1, 1, AB_ANCHOR_VPIVOT, AB_ANCHOR_HPIVOT};
    if (!ab_sprite_quad(region, texture_width, texture_height, sprite, &local, error)) {
        return false;
    }
    AbTransform2 translation = {
        .xx = 1,
        .yy = 1,
        .tx = (placement.x - placement.origin_x) * placement.zoom,
        .ty = (placement.y - placement.origin_y) * placement.zoom};
    AbTransform2 turn = {rotation.cosine, -rotation.sine,  0,
                         rotation.sine,   rotation.cosine, 0};
    float horizontal = placement.horizontal_multiplier * placement.zoom;
    horizontal *= placement.scale_x;
    float vertical = placement.zoom * placement.scale_y;
    AbTransform2 scale = {.xx = horizontal * 1.0f,
                          .xy = vertical * 0.0f,
                          .yx = horizontal * 0.0f,
                          .yy = vertical * 1.0f};
    AbTransform2 transform = multiply(multiply(translation, turn), scale);
    return ab_quad_transform(&local, transform, output, error);
}
