#ifndef AB_OBJECT_SPRITE_H
#define AB_OBJECT_SPRITE_H

#include "ab/draw.h"
#include "ab/coordinates.h"
#include "ab/image_rectangle.h"

typedef struct {
    AbVec2 physics_position;
    float angle_radians;
    AbVec2 scale;
} AbObjectSpritePose;

/* Ordinary GameObjectRenderer -> SpriteAsset -> Sprite rectangle boundary.
 * The pose is the already interpolated render pose. This preserves the native
 * divide by object scale before the image's later scale multiplication. Object
 * visibility, collider exclusion, composite sprites and effects are caller work.
 * Outputs are disjoint and unchanged on failure. No allocation. */
bool ab_object_sprite_state(const AbSpriteRegion *region, AbObjectSpritePose pose,
                            AbCoordinateCamera camera, AbImageRectangle *rectangle,
                            AbSpriteRenderState *state, AbError *error);

typedef struct {
    float x;
    float y;
    float angle_radians;
    float horizontal_multiplier;
    float zoom;
    float scale_x;
    float scale_y;
    float origin_x;
    float origin_y;
} AbObjectSpritePlacement;

/* GameObjectRenderer's explicit matrix path. Coordinates are pixels before zoom;
 * origin is subtracted before zoom. Ordinary render-state drawing is separate.
 * No allocation; invalid or unsupported numeric inputs preserve output. */
bool ab_object_sprite_quad(const AbSpriteRegion *region, uint32_t texture_width,
                           uint32_t texture_height, AbObjectSpritePlacement placement,
                           AbDrawQuad *output, AbError *error);

#endif
