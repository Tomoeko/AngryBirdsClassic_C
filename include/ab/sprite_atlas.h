#ifndef AB_SPRITE_ATLAS_H
#define AB_SPRITE_ATLAS_H

#include "ab/image.h"
#include "ab/sprite_set.h"

typedef struct AbSpriteAtlas AbSpriteAtlas;

typedef enum {
    AB_SPRITE_ALPHA_UNKNOWN,
    AB_SPRITE_ALPHA_OPAQUE,
    AB_SPRITE_ALPHA_PRESENT,
} AbSpriteAlpha;

typedef struct {
    AbSpan path;
    const AbImage *image;
    AbSpriteAlpha alpha;
} AbSpriteAtlasImage;

typedef struct {
    AbSpriteBinding sprite;
    size_t image_index;
    const AbImage *image;
    AbSpriteAlpha alpha;
} AbSpriteAtlasBinding;

/* Decode each distinct referenced PNG/PVR/WebP once, in first sheet occurrence order.
 * Texture references must be basenames relative to their metadata directory,
 * as in all 237 target SPRT files. Unsupported codec variants reject explicitly.
 * Archive bytes and the sprite set remain borrowed until close. The owner,
 * fixed sheet/image arrays and retained RGBA share allocation_limit. entry_limit
 * bounds each temporary ZIP payload; the image decoders also bound their
 * workspace by the remaining allocation limit. This is not a peak-memory limit.
 * PVR/WebP and target RGBA8 PNG source formats retain explicit alpha presence;
 * other PNG upload formats remain UNKNOWN. No alpha conversion, GPU objects or
 * resource-group policy are applied. Output starts NULL; every partial allocation is
 * released on failure. */
bool ab_sprite_atlas_open(const AbZip *archive, const AbSpriteSet *sprites,
                          size_t allocation_limit, size_t entry_limit,
                          AbSpriteAtlas **output, AbError *error);
void ab_sprite_atlas_close(AbSpriteAtlas *atlas);

size_t ab_sprite_atlas_count(const AbSpriteAtlas *atlas);
size_t ab_sprite_atlas_bytes(const AbSpriteAtlas *atlas);
/* Views borrow atlas pixels and archive paths. Invalid/missing lookups preserve
 * output. Find retains the sprite set's last-sheet/last-name policy. No lookup
 * allocates, reads the archive, decodes pixels or changes image indices. */
bool ab_sprite_atlas_image(const AbSpriteAtlas *atlas, size_t index,
                           AbSpriteAtlasImage *output);
bool ab_sprite_atlas_find(const AbSpriteAtlas *atlas, AbSpan name,
                          AbSpriteAtlasBinding *output);

/* Resolve a retained sheet/region selection, independently of registry masking.
 * Both pointers must belong to this live atlas's sprite set. Missing, unloaded
 * or foreign bindings preserve output. */
bool ab_sprite_atlas_resolve(const AbSpriteAtlas *atlas, AbSpriteBinding sprite,
                             AbSpriteAtlasBinding *output);

/* Resolve the active native-matched registry binding into this atlas's retained
 * image. The registry may contain other sets or composite entries; those mask
 * older entries without falling back to the atlas's static sheet order. No
 * allocation, image decoding or upload. Missing/invalid queries preserve output. */
bool ab_sprite_atlas_find_registered(const AbSpriteAtlas *atlas,
                                     const AbSpriteRegistry *registry, AbSpan name,
                                     AbSpriteAtlasBinding *output);

#endif
