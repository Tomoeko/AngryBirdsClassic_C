#ifndef AB_COMPOSITE_RESOURCES_H
#define AB_COMPOSITE_RESOURCES_H

#include "ab/composite.h"
#include "ab/composite_bounds.h"
#include "ab/sprite_set.h"

typedef struct AbCompositeResources AbCompositeResources;

typedef bool (*AbCompositeSpriteResolver)(void *context, AbSpan name,
                                          AbSpriteBinding *output, AbError *error);

typedef struct {
    AbSpan metadata_path;
    const AbComposite *source;
    const AbSpriteBinding *parts;
    AbCompositeBounds bounds;
    bool extended;
} AbResolvedComposite;

/* Own explicitly selected KA3D/RVIO COMP payloads, parsed definitions and cached
 * ordinary-SPRT bindings/bounds. Resolve every part at open; missing sprites
 * fail. This entry point retains explicit sprite-set lookup policy, which is
 * separate from the native sheet-map policy. Archive paths and sprite set are
 * borrowed until close. allocation_limit bounds all retained owner data; entry_limit
 * bounds each ZIP payload. Temporary bounds workspace is separately bounded by
 * allocation_limit, so this is not a peak-memory limit. No pixels/GPU objects,
 * runtime Lua or resource-group selection. Output starts NULL; failures release
 * every partial allocation. Open/close occur outside frame rendering. */
bool ab_composite_resources_open(const AbZip *archive, const AbSpriteSet *sprites,
                                 const char *const *paths, size_t count,
                                 size_t allocation_limit, size_t entry_limit,
                                 AbCompositeResources **output, AbError *error);

/* Resolve part sprite identifiers with a caller policy, including native
 * first-sheet-key lookup. Context is used only during open; returned bindings
 * require their dependencies while inspecting/drawing parts. Close only frees
 * owned arrays and never dereferences bindings, allowing sheets to be released
 * earlier during group teardown. Resolver prefix effects on failure belong to
 * the caller. Other ownership/storage rules above apply. */
bool ab_composite_resources_open_resolved(const AbZip *archive,
                                          const char *const *paths, size_t count,
                                          size_t allocation_limit, size_t entry_limit,
                                          AbCompositeSpriteResolver resolve,
                                          void *context, AbCompositeResources **output,
                                          AbError *error);
void ab_composite_resources_close(AbCompositeResources *resources);
size_t ab_composite_resources_count(const AbCompositeResources *resources);
size_t ab_composite_resources_bytes(const AbCompositeResources *resources);

/* Stable borrowed views, no allocation/decoding/I/O. Enumeration follows explicit
 * file/definition order. Find uses last file/last definition, an explicit-set
 * policy rather than recovered native group ordering. Invalid/missing queries
 * preserve output. Views expire with this owner and its borrowed dependencies. */
bool ab_composite_resources_item(const AbCompositeResources *resources, size_t index,
                                 AbResolvedComposite *output);
bool ab_composite_resources_find(const AbCompositeResources *resources, AbSpan name,
                                 AbResolvedComposite *output);

#endif
