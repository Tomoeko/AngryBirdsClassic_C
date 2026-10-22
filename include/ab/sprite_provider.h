#ifndef AB_SPRITE_PROVIDER_H
#define AB_SPRITE_PROVIDER_H

#include "ab/composite_resources.h"
#include "ab/sprite_atlas.h"

/* Borrow a stable, immutable provider and its context while visual/scene owners
 * use it. Queries are serialized, pure lookups: no allocation, decoding, GPU
 * calls or resource mutation. Missing/invalid queries preserve output. Returned
 * metadata and images remain borrowed until the next resource mutation. Cached
 * visual bindings keep their assigned selection until another set-sprite call;
 * callers must discard/rebind them before releasing their source resources.
 * Composite lookup is optional. Texture IDs must be nonzero when found. */
typedef struct {
    void *context;
    bool (*sprite)(void *context, AbSpan name, AbSpriteAtlasBinding *output);
    bool (*composite)(void *context, AbSpan name, AbResolvedComposite *output);
    bool (*texture)(void *context, const AbImage *image, uint32_t *output);
    /* Optional exact cached selection lookup for composite parts. */
    bool (*binding)(void *context, AbSpriteBinding sprite,
                    AbSpriteAtlasBinding *output);
} AbSpriteProvider;

#endif
