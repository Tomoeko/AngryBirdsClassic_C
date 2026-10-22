#ifndef AB_SPRITE_SCENE_H
#define AB_SPRITE_SCENE_H

#include "ab/object_sprite.h"

typedef struct {
    const AbSpriteRegion *region;
    AbObjectSpritePose pose;
    uint32_t texture;
    uint32_t texture_width;
    uint32_t texture_height;
    int32_t layer;
    uint32_t collider;
    AbBlend blend;
    bool visible;
    bool special_animation;
    /* An empty ordinary SpriteAsset; cached geometry is not submitted. */
    bool unbound;
} AbSceneSprite;

/* Items are in insertion order. Prepare indices after scene membership or layer
 * changes, retaining insertion order within each ascending integer layer. Uses
 * caller-owned disjoint storage; no allocation. Invalid storage preserves order.
 * This initialization sort is not needed on ordinary pose-only frame updates. */
bool ab_sprite_scene_order(const AbSceneSprite *items, size_t count, size_t *order,
                           size_t order_capacity, AbError *error);

/* Draw ordinary GameObjectRenderer sprites in the prepared permutation. Hidden
 * objects, empty assets and collider 7 are skipped before resource access. Special
 * animation wrappers reject explicitly. Composite/effect/mesh renderers are separate.
 * Regions and poses are borrowed. Order comes from ab_sprite_scene_order and is
 * unchanged here. Appends without allocation; failure restores list.count, while
 * storage beyond that count may change. Caller supplies valid texture bindings
 * and the recovered blend choice. Normal object color/opacity are white/one. */
bool ab_sprite_scene_draw(const AbSceneSprite *items, size_t count, const size_t *order,
                          AbCoordinateCamera camera, uint32_t viewport_width,
                          uint32_t viewport_height, AbDrawList *list, AbError *error);

#endif
