#ifndef AB_OBJECT_VISUAL_H
#define AB_OBJECT_VISUAL_H

#include "ab/composite.h"
#include "ab/sprite.h"

typedef struct {
    AbSpan name;
    const AbSpriteRegion *sprite;
    const AbComposite *composite;
} AbSpriteAssetState;

typedef struct {
    void *context;
    bool (*sprite)(void *context, AbSpan name, const AbSpriteRegion **output,
                   AbError *error);
    bool (*composite)(void *context, AbSpan name, const AbComposite **output,
                      AbError *error);
    void (*missing)(void *context, AbSpan name);
} AbSpriteAssetHost;

typedef struct {
    AbSpan sprite;
    bool sprite_present;
    AbSpriteAssetState *attached;
} AbObjectVisual;

/* Resolved SpriteAsset::setSprite. Retain the supplied name and clear both
 * bindings before any lookup, including repeated names. Empty names stop there.
 * Prefer an ordinary sprite, otherwise query a composite, otherwise report the
 * missing name without restoring the previous binding. Hosts return NULL for a
 * missing resource; a false return represents a boundary failure and retains
 * preceding state/host changes. Name bytes and returned resources are borrowed
 * and must remain alive until replaced/cleared. No allocation or GPU work.
 */
bool ab_sprite_asset_set(AbSpriteAssetState *state, AbSpan name, AbSpriteAssetHost host,
                         AbError *error);

/* Resolved GameLua::setSprite: write the script's string field first, then
 * update the currently attached asset if present. An object without an attached
 * renderer still retains its new script sprite. Later asset/lookup failures do
 * not undo that field. Object/renderer lifetime remains stable during the call.
 * Valid string spans only; native argument coercion/name lookup is separate.
 */
bool ab_object_visual_set_sprite(AbObjectVisual *object, AbSpan name,
                                 AbSpriteAssetHost host, AbError *error);

#endif
