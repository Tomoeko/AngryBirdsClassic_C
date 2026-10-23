#ifndef AB_CLASSIC_VISUALS_H
#define AB_CLASSIC_VISUALS_H

#include "ab/classic_level.h"
#include "ab/composite_resources.h"
#include "ab/object_visual.h"
#include "ab/sprite_atlas.h"
#include "ab/sprite_provider.h"

typedef struct AbClassicVisuals AbClassicVisuals;

typedef struct {
    const AbSpriteAtlas *atlas;
    const AbCompositeResources *composites;
    void *context;
    void (*missing)(void *context, size_t object, AbSpan name);
    size_t storage_bytes;
    size_t name_capacity;
    /* Select either this provider or atlas/composites. The provider is borrowed
     * at a stable address and may resolve across active resource groups. */
    const AbSpriteProvider *provider;
} AbClassicVisualResources;

typedef struct {
    AbClassicLevel *level;
    const AbSpriteAtlas *atlas;
    const AbCompositeResources *composites;
    size_t count;
    const AbSpriteProvider *provider;
} AbClassicVisualView;

/* Own canonical script-sprite fields, optional attached ordinary asset state
 * and cached atlas bindings in body-object creation order. Seed from the level's
 * prepared initial names; ground/collider 7 have no attached ordinary asset.
 * Initial rendered names must be strings, including empty; an unrendered nil
 * name remains absent. Other typed names reject. Ordinary lookup precedes the
 * explicitly supplied composite set; missing resources retain names/empty assets.
 * Resource-group selection, lazy loading, special renderers and complete object
 * construction remain separate. No effects/RNG, pixels, GPU work or frame-time
 * allocation. storage_bytes bounds this owner, fixed entries and an immutable
 * sprite-name pool. Its slot count derives from the remaining byte budget;
 * name_capacity bounds each distinct copied name, including embedded zeros.
 * Equal names share a stable slot; later assignments never overwrite a retained
 * name. Exhausted storage rejects a new distinct name before property changes.
 * Level and the selected atlas/composite resources or provider remain borrowed
 * until close. Reserve stable entries for the level's full object capacity;
 * later explicit attachments append in source creation order. Removal detaches
 * the retained visual; explicit native circle replacement can reattach that row.
 * Output starts NULL; all partial allocations are released on failure.
 */
bool ab_classic_visuals_open(AbClassicLevel *level, AbClassicVisualResources resources,
                             AbClassicVisuals **output, AbError *error);
void ab_classic_visuals_close(AbClassicVisuals *visuals);
AbClassicVisualView ab_classic_visuals_view(const AbClassicVisuals *visuals);
size_t ab_classic_visuals_bytes(const AbClassicVisuals *visuals);

/* Attach an empty ordinary asset to the next newly registered level object.
 * Stable entries reserve the level's full object capacity at open. No sprite
 * assignment or resource lookup occurs here; set_sprite follows attachment in
 * createRendererForGameObject. A detached existing row can be rebuilt after
 * native circle replacement. No gaps, attached-row reuse, ground or collider 7. A
 * failed later sprite assignment retains this attachment and requires session discard.
 */
bool ab_classic_visuals_attach_object(AbClassicVisuals *visuals, size_t object,
                                      AbError *error);

/* Borrow object views until close; all interned name bytes remain valid until
 * close, including names retained before later assignments. Binding succeeds only for a
 * currently bound ordinary sprite, and preserves output on failure. Cached metadata
 * avoids repeated atlas lookup during pose/draw synchronization. */
const AbObjectVisual *ab_classic_visuals_object(const AbClassicVisuals *visuals,
                                                size_t object);
bool ab_classic_visuals_binding(const AbClassicVisuals *visuals, size_t object,
                                AbSpriteAtlasBinding *output);

/* Remove the currently attached ordinary renderer while retaining its stable
 * metadata and interned names until close. Physical-removal lifetime ownership
 * stays with the caller; no GPU resource is released by this shared view. */
bool ab_classic_visuals_detach(AbClassicVisuals *visuals, size_t object,
                               AbError *error);

/* Apply the recovered GameLua/SpriteAsset write order through selected resources.
 * Missing resources succeed with an empty asset and optional missing callback.
 * Publish the canonical physical sprite field before asset lookup, independently
 * of a rebound script.physicsObject. Its interned name bytes remain stable until
 * the visual owner closes, including earlier values retained by callbacks.
 * Copy the name before changing fields. Invalid spans/over-capacity names leave
 * prior state unchanged; later failures retain metadata/asset changes. No
 * allocation or GPU work. A name may overlap this object's current name buffer.
 */
bool ab_classic_visuals_set_sprite(AbClassicVisuals *visuals, size_t object,
                                   AbSpan name, AbError *error);

/* Runtime getDamageSpriteNames over the canonical physical table: use its
 * current sprite, definition, strength, initialStrength and truthy damageSprites
 * override. Missing definitions/ranges/strength retain the current sprite for
 * all three face names. Normalize the original constructor's material strength
 * fallback without mutating our catalog. Range iteration requires that table's
 * retained APK constructor layout and caller-owned disjoint scratch. Rebinding
 * to another catalog range table is supported; new/mutated layouts reject.
 * No visual or damage-field writes, RNG or allocation. Names remain borrowed;
 * scratch may change on error, while output commits only on success. */
bool ab_classic_visuals_damage_names(AbClassicVisuals *visuals, size_t object,
                                     AbStringOrderWorkspace *workspace,
                                     AbDamageSpriteNames *output, AbError *error);

/* Runtime getDamageSprite: query names, then set the actual visual sprite unless
 * dontUpdateDamageSprite or frozenSolid is truthy. Return the descriptor after
 * that boundary succeeds. It does not assign damageSprite/blinkSprite/smileSprite;
 * collision/character callers publish those at their own recovered phases.
 * A late failure retains visual/property effects; stop the session on error. */
bool ab_classic_visuals_update_damage(AbClassicVisuals *visuals, size_t object,
                                      AbStringOrderWorkspace *workspace,
                                      AbDamageSpriteNames *output, AbError *error);

#endif
