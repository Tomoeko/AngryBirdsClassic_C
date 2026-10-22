#ifndef AB_COMPOSITE_RESOURCE_H
#define AB_COMPOSITE_RESOURCE_H

#include "ab/composite_resources.h"
#include "ab/resource_owners.h"

typedef struct AbCompositeResource AbCompositeResource;

/* Own one decoded composite source and its final named member views. Resolve
 * ordinary parts from decoded SPRT providers in the native first-sheet-key order.
 * Map references retain this owner; close releases the caller's initial reference.
 * Part bindings are weak, matching native entries: referenced sheet metadata must
 * remain live while inspecting/drawing parts. Close never dereferences those
 * dependencies, allowing ordered group teardown after sheets have been released.
 * Borrow the archive through final release. Bound retained bytes and each ZIP
 * payload separately; bounds/codec workspaces have separate bounded lifetimes.
 * No VM or GPU work. Serialize references/views and source-map mutations.
 * Failure leaves output NULL and releases all partially owned storage. */
bool ab_composite_resource_open(const AbZip *archive, const char *metadata_path,
                                const AbResourceOwners *sheets, size_t allocation_limit,
                                size_t entry_limit, AbCompositeResource **output,
                                AbError *error);
void ab_composite_resource_close(AbCompositeResource *resource);
const AbResourceSource *
ab_composite_resource_source(const AbCompositeResource *resource);
const AbCompositeResource *
ab_composite_resource_from_source(const AbResourceSource *source);
bool ab_composite_resource_member(const AbCompositeResource *resource,
                                  const AbResourceMember *member,
                                  AbResolvedComposite *output);
size_t ab_composite_resource_bytes(const AbCompositeResource *resource);

/* Resolve only an active registry binding belonging to this owner. Other source
 * and ordinary sprite bindings mask older composites without fallback. Returned
 * views borrow this owner and its weak dependencies. No allocation/decoding/I/O;
 * invalid/missing queries preserve output. */
bool ab_composite_resource_find(const AbCompositeResource *resource,
                                const AbSpriteRegistry *registry, AbSpan name,
                                AbResolvedComposite *output);

#endif
