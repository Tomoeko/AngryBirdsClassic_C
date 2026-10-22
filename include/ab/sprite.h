#ifndef AB_SPRITE_H
#define AB_SPRITE_H

#include "ab/base.h"

typedef struct {
    AbSpan name;
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
    int16_t pivot_x;
    int16_t pivot_y;
} AbSpriteRegion;

typedef struct {
    AbSpan texture;
    AbSpriteRegion *sprites;
    size_t count;
} AbSpriteSheet;

/* Owns the region array; names and texture borrow input. Output starts empty. */
bool ab_sprite_sheet_read(AbSpan input, size_t allocation_limit, AbSpriteSheet *output,
                          AbError *error);
const AbSpriteRegion *ab_sprite_find(const AbSpriteSheet *sheet, AbSpan name);
void ab_sprite_sheet_free(AbSpriteSheet *sheet);

#endif
