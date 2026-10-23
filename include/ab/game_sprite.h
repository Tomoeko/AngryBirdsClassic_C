#ifndef AB_GAME_SPRITE_H
#define AB_GAME_SPRITE_H

#include "ab/lua_asset_sprite.h"

typedef struct {
    AbVec2 physics_position, scale;
    float angle, alpha, physics_scale;
} AbGameSpritePose;

/* Resolved utils.drawGameSprite12979: scale the sprite's pivot and bounds,
 * divide script position by physicsScale, then use camera translation/zoom and
 * the explicit rotation origin. The binding is prepared before frame drawing.
 * Native resource lookup, argument coercion and global render-state publication
 * remain caller boundaries. No allocation, lookup or persistent render changes.
 * Empty resources skip; finite used values and nonzero physicsScale are required. */
bool ab_game_sprite_command(const AbLuaAssetSpriteBinding *binding,
                            AbGameSpritePose pose, AbCoordinateCamera camera,
                            uint32_t width, uint32_t height, AbDrawCommand *command,
                            bool *visible, AbError *error);

#endif
