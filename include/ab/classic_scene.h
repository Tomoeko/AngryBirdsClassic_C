#ifndef AB_CLASSIC_SCENE_H
#define AB_CLASSIC_SCENE_H

#include "ab/classic_level.h"
#include "ab/classic_visuals.h"
#include "ab/sprite_atlas.h"
#include "ab/sprite_scene.h"

typedef struct AbClassicScene AbClassicScene;

typedef struct {
    /* Items use body-object creation indices; order is the initial layer/vector
     * permutation. Arrays and region pointers remain borrowed until close. */
    const AbSceneSprite *items;
    const size_t *order;
    size_t count;
} AbClassicSceneView;

/* Bind an already loaded classic level's initial ordinary sprites, catalog
 * layers and rendered poses to an already decoded atlas. The level and atlas
 * remain borrowed and must outlive this owner. Texture IDs are consecutive
 * from first_texture in atlas-image order; upload and blend selection remain
 * explicit caller work. Constructor scale is one and visibility is true.
 * The reserved ground has an invisible, unbound placeholder at its object index;
 * it has no ordinary renderer. Collider 7 is excluded before sprite/catalog
 * access. Composite, mask, special
 * animation and extra renderers are outside this adapter. Reserve the level's
 * full object capacity for explicit later ordinary append operations. Scene IDs
 * and membership registration remain in the enclosing render-scene owner.
 * allocation_limit bounds this owner and both arrays. Output starts NULL;
 * all partial allocations are released. No GPU calls or per-frame allocation. */
bool ab_classic_scene_open(AbClassicLevel *level, const AbSpriteAtlas *atlas,
                           uint32_t first_texture, AbBlend blend,
                           size_t allocation_limit, AbClassicScene **output,
                           AbError *error);
/* Bind ordinary sprites through a stable borrowed provider and its synchronized
 * texture cache. Texture IDs need not be consecutive. Missing/empty initial
 * names create empty assets; initial composites reject explicitly. Cached
 * bindings are not automatically retargeted on resource-group changes: discard
 * or rebind them before source release, then sync visuals before drawing.
 * Other membership, storage and rendering limits above still apply. */
bool ab_classic_scene_open_resources(AbClassicLevel *level,
                                     const AbSpriteProvider *provider, AbBlend blend,
                                     size_t allocation_limit, AbClassicScene **output,
                                     AbError *error);
void ab_classic_scene_close(AbClassicScene *scene);
AbClassicSceneView ab_classic_scene_view(const AbClassicScene *scene);
size_t ab_classic_scene_bytes(const AbClassicScene *scene);

/* Reserve stable entries for the level's object capacity at open; initialize
 * the next raw ordinary renderer slot with an empty asset, source integer layer,
 * unit scale and current rendered pose. No resource/property effects or ID/
 * membership registration; the enclosing render-scene owner supplies those.
 * Reject gaps, reused slots, collider 7 and nonfinite/out-of-range layers. */
bool ab_classic_scene_append_object(AbClassicScene *scene, size_t object, float layer,
                                    AbError *error);

/* Reinitialize a retained row after its enclosing renderer was detached. The
 * caller owns scene membership and must remove the old identity first. */
bool ab_classic_scene_recreate_object(AbClassicScene *scene, size_t object, float layer,
                                      AbError *error);

/* These actions change only ordinary renderer bindings. Sprite lookup preserves
 * the previous binding on failure. Scales must be finite and nonzero. Hidden
 * objects and empty assets are skipped by pose sync; hide a removed body before the
 * next draw. Object indices, membership and the borrowed level's frame bindings stay
 * stable. These are boundaries for recovered controller/animation scheduling,
 * not an implementation of that scheduling or native removal callbacks. */
bool ab_classic_scene_set_sprite(AbClassicScene *scene, size_t object, AbSpan name,
                                 AbError *error);
bool ab_classic_scene_set_scale(AbClassicScene *scene, size_t object, AbVec2 scale,
                                AbError *error);
bool ab_classic_scene_set_visible(AbClassicScene *scene, size_t object, bool visible,
                                  AbError *error);
/* Bind canonical visual state from the same level and atlas/provider without name
 * search, resource loading or allocation. Empty/absent assets suppress draw submission
 * while preserving requested visibility, so a later valid sprite can return.
 * Composite assets reject before writes; their renderer remains separate.
 * Fixed slots and no concurrent visual mutation are required. Level-forgotten
 * rows become invisible and skip visual/pose reads, including live orphan bodies.
 */
bool ab_classic_scene_sync_visuals(AbClassicScene *scene,
                                   const AbClassicVisuals *visuals, AbError *error);
/* Copy current rendered poses, then append shared clip-coordinate commands.
 * Caller advances physics/history and other controllers beforehand. A failed
 * draw restores list.count; pose synchronization may already have committed.
 * No camera movement, animation update or resource loading occurs. */
bool ab_classic_scene_draw(AbClassicScene *scene, AbCoordinateCamera camera,
                           uint32_t viewport_width, uint32_t viewport_height,
                           AbDrawList *list, AbError *error);

/* Synchronize ordinary rendered poses without drawing. This separates pose
 * publication from traversal when another owner interleaves renderer classes.
 * Hidden/empty/forgotten objects retain the same skip rules as draw above. */
bool ab_classic_scene_sync_poses(AbClassicScene *scene, AbError *error);

#endif
