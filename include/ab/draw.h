#ifndef AB_DRAW_H
#define AB_DRAW_H

#include "ab/sprite.h"

typedef enum {
    AB_ANCHOR_TOP,
    AB_ANCHOR_VCENTER,
    AB_ANCHOR_BOTTOM,
    AB_ANCHOR_BASELINE,
    AB_ANCHOR_VPIVOT,
} AbVerticalAnchor;

typedef enum {
    AB_ANCHOR_LEFT,
    AB_ANCHOR_HCENTER,
    AB_ANCHOR_RIGHT,
    AB_ANCHOR_HPIVOT,
} AbHorizontalAnchor;

typedef struct {
    float x;
    float y;
    float scale_x;
    float scale_y;
    AbVerticalAnchor vertical;
    AbHorizontalAnchor horizontal;
} AbSpritePlacement;

typedef struct {
    float x, y, width, height;
    AbVerticalAnchor vertical;
    AbHorizontalAnchor horizontal;
} AbSpriteSizedPlacement;

typedef struct {
    float x;
    float y;
    float u;
    float v;
} AbDrawVertex;

typedef struct {
    /* Native unrotated atlas order: top-left, top-right, bottom-left, bottom-right. */
    AbDrawVertex vertices[4];
} AbDrawQuad;

typedef struct {
    float xx;
    float xy;
    float tx;
    float yx;
    float yy;
    float ty;
} AbTransform2;

/* Factors follow the target FX pass states; equations use addition. */
typedef enum {
    AB_BLEND_OPAQUE,
    AB_BLEND_PREMULTIPLIED,
    AB_BLEND_STRAIGHT,
    AB_BLEND_ADDITIVE,
    AB_BLEND_MULTIPLY,
} AbBlend;

typedef enum {
    AB_DRAW_PIXELS,
    AB_DRAW_CLIP,
} AbDrawCoordinates;

typedef struct {
    int32_t left, top, right, bottom;
    bool enabled;
} AbDrawClip;

typedef struct {
    uint32_t x, y, width, height;
} AbDrawScissor;

typedef struct {
    AbBlend blend;
    uint32_t texture;
    AbDrawQuad quad;
    /* Component multiplier and a separate factor applied to all four channels. */
    float color[4];
    float opacity;
    /* Common accepts recovered clip coordinates without a second transform. */
    AbDrawCoordinates coordinates;
    /* Integer top-left screen bounds, independent of vertex coordinates. */
    AbDrawClip scissor;
} AbDrawCommand;

/* Intersect the retained screen request with the framebuffer for backend APIs.
 * Disabled clipping selects the complete framebuffer. Empty intersections are
 * valid and have zero area. No mutation, allocation or coordinate rounding. */
bool ab_draw_clip_scissor(AbDrawClip clip, uint32_t width, uint32_t height,
                          AbDrawScissor *output, AbError *error);

typedef struct {
    AbDrawCommand *commands;
    size_t count;
    size_t capacity;
} AbDrawList;

/* SPRT has no source rotation. Retains negative coordinates and dimensions. */
bool ab_sprite_quad(const AbSpriteRegion *region, uint32_t texture_width,
                    uint32_t texture_height, AbSpritePlacement placement,
                    AbDrawQuad *output, AbError *error);
/* Native explicitly-sized Sprite.draw route. Anchor offsets use the original
 * region dimensions/rounded pivots without scaling by requested size. UVs still
 * cover the full region. Signed/zero sizes remain in caller order. Finite input
 * and results are required; failure preserves output. No resource lookup. */
bool ab_sprite_sized_quad(const AbSpriteRegion *region, uint32_t texture_width,
                          uint32_t texture_height, AbSpriteSizedPlacement placement,
                          AbDrawQuad *output, AbError *error);
/* Uses the native affine path's y-product, x-product, translation order. */
bool ab_quad_transform(const AbDrawQuad *input, AbTransform2 transform,
                       AbDrawQuad *output, AbError *error);

/* Allocate once outside frame rendering. Push never allocates or reorders. */
bool ab_draw_list_init(size_t capacity, size_t allocation_limit, AbDrawList *output,
                       AbError *error);
bool ab_draw_command_check(const AbDrawCommand *command, AbError *error);
bool ab_draw_list_push(AbDrawList *list, const AbDrawCommand *command, AbError *error);
void ab_draw_list_clear(AbDrawList *list);
void ab_draw_list_free(AbDrawList *list);

#endif
